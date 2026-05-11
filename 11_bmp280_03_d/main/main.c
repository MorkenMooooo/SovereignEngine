#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"   // FreeRTOS 实时操作系统头文件
#include "freertos/task.h"        // FreeRTOS 任务管理（如 vTaskDelay 延时）
#include "driver/i2c_master.h"    // ESP-IDF 的 I2C 主机驱动
#include "esp_log.h"              // ESP-IDF 日志打印工具

static const char *TAG = "BMP280"; // 日志标签，打印时会显示 [BMP280]

/* =============================================
   I2C 引脚与通信参数定义
   BMP280 通过 I2C 总线与 ESP32 通信
   ============================================= */
#define I2C_MASTER_SCL_IO          GPIO_NUM_7    // SCL（时钟线）连接的 GPIO 引脚
#define I2C_MASTER_SDA_IO          GPIO_NUM_15   // SDA（数据线）连接的 GPIO 引脚
#define I2C_MASTER_FREQ_HZ         400000        // I2C 通信速率：400kHz（快速模式）
#define I2C_PORT                   I2C_NUM_0     // 使用 ESP32 的 I2C 控制器 0
#define BMP280_SENSOR_ADDR         0x76          // BMP280 的 I2C 地址（SDO 引脚接 GND 时为 0x76）
#define I2C_TIMEOUT_MS             1000          // I2C 通信超时时间：1000 毫秒

/* =============================================
   BMP280 寄存器地址定义
   寄存器是传感器内部的"存储格子"，通过读写这些地址来控制传感器
   ============================================= */
#define BMP280_REG_CHIP_ID         0xD0  // 芯片 ID 寄存器，读出来应该是 0x58
#define BMP280_REG_RESET           0xE0  // 软复位寄存器，写入 0xB6 可重置传感器
#define BMP280_REG_STATUS          0xF3  // 状态寄存器，可查询传感器是否正在测量
#define BMP280_REG_CTRL_MEAS       0xF4  // 控制寄存器，设置采样精度和工作模式
#define BMP280_REG_CONFIG          0xF5  // 配置寄存器，设置滤波器和待机时间
#define BMP280_REG_PRESS_MSB       0xF7  // 气压数据起始寄存器（共 6 字节：气压3字节+温度3字节）
#define BMP280_REG_CALIB_START     0x88  // 校准参数起始寄存器（共 24 字节）

/* =============================================
   数据类型别名定义
   为了和 BMP280 手册中的命名保持一致
   ============================================= */
typedef int32_t  BMP280_S32_t;  // 有符号 32 位整数
typedef uint32_t BMP280_U32_t;  // 无符号 32 位整数
typedef int64_t  BMP280_S64_t;  // 有符号 64 位整数（气压补偿需要更大范围）

/* =============================================
   全局句柄变量
   句柄是 ESP-IDF 用来管理硬件资源的"引用"
   ============================================= */
static i2c_master_bus_handle_t bus_handle; // I2C 总线句柄（代表整条 I2C 总线）
static i2c_master_dev_handle_t dev_handle; // I2C 设备句柄（代表总线上的 BMP280 这个设备）

/* =============================================
   校准参数（出厂时烧录在传感器内部）
   BMP280 的原始 ADC 数据不是真实的温度/气压值，
   需要用这些校准参数进行数学补偿计算才能得到真实值
   ============================================= */
static uint16_t dig_T1;                    // 温度校准参数 1（无符号）
static int16_t  dig_T2, dig_T3;           // 温度校准参数 2、3（有符号）
static uint16_t dig_P1;                    // 气压校准参数 1（无符号）
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5; // 气压校准参数 2-5
static int16_t  dig_P6, dig_P7, dig_P8, dig_P9; // 气压校准参数 6-9
static BMP280_S32_t t_fine; // 温度补偿的中间值，气压补偿也需要用到它

/* =============================================
   底层 I2C 通信函数：写寄存器
   向传感器的某个寄存器写入一个字节的数据
   ============================================= */
static esp_err_t bmp280_write_reg(uint8_t reg_addr, uint8_t data) {
    // 把寄存器地址和数据打包成 2 字节，通过 I2C 发送给传感器
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_TIMEOUT_MS);
}

/* =============================================
   底层 I2C 通信函数：读寄存器
   从传感器的某个寄存器开始，连续读取 len 个字节
   ============================================= */
static esp_err_t bmp280_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
    // 先发送要读取的寄存器地址，再接收传感器返回的数据
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_TIMEOUT_MS);
}

/* =============================================
   温度补偿算法（来自 BMP280 官方手册 3.11.3 节）
   
   传感器 ADC 读出的是原始数字值，不是摄氏度。
   这个函数用校准参数把原始值换算成真实温度。
   
   输入：adc_T —— 传感器读出的原始温度 ADC 值（20位）
   输出：返回温度值（单位：0.01°C，例如返回 2510 表示 25.10°C）
   副作用：同时计算并保存 t_fine（供气压补偿使用）
   ============================================= */
static BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T) {
    BMP280_S32_t var1, var2, T;
    // 第一项：线性补偿
    var1 = ((((adc_T>>3) - ((BMP280_S32_t)dig_T1<<1))) * ((BMP280_S32_t)dig_T2)) >> 11;
    // 第二项：二次补偿（修正非线性误差）
    var2 = ( ((((adc_T>>4) - ((BMP280_S32_t)dig_T1)) * ((adc_T>>4) - ((BMP280_S32_t)dig_T1))) >> 12) * ((BMP280_S32_t)dig_T3) ) >> 14;
    // t_fine 是温度的精细中间值，后续气压补偿也需要它
    t_fine = var1 + var2;
    // 最终温度值（单位 0.01°C）
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

/* =============================================
   气压补偿算法（来自 BMP280 官方手册 3.11.3 节）
   
   同样，ADC 读出的气压原始值需要用校准参数换算。
   注意：必须先调用温度补偿函数（更新 t_fine），再调用此函数！
   
   输入：adc_P —— 传感器读出的原始气压 ADC 值（20位）
   输出：返回气压值（单位：1/256 Pa，例如返回 25600000 表示 100000 Pa = 1000 hPa）
   ============================================= */
static BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P) {
    BMP280_S64_t var1, var2, p;
    // 利用 t_fine 进行温度相关的气压修正
    var1 = ((BMP280_S64_t)t_fine) - 128000;
    var2 = var1 * var1 * (BMP280_S64_t)dig_P6;
    var2 = var2 + ((var1*(BMP280_S64_t)dig_P5)<<17);
    var2 = var2 + (((BMP280_S64_t)dig_P4)<<35);
    var1 = ((var1 * var1 * (BMP280_S64_t)dig_P3)>>8) + ((var1 * (BMP280_S64_t)dig_P2)<<12);
    var1 = (((((BMP280_S64_t)1)<<47)+var1))*((BMP280_S64_t)dig_P1)>>33;
    if (var1 == 0) return 0; // 防止除以零（避免程序崩溃）
    p = 1048576 - adc_P;
    p = (((p<<31) - var2) * 3125) / var1;
    var1 = (((BMP280_S64_t)dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((BMP280_S64_t)dig_P8) * p) >> 19;
    // 最终气压值（单位：1/256 Pa）
    p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dig_P7)<<4);
    return (BMP280_U32_t)p;
}

/* =============================================
   海拔换算函数（国际标准大气压模型）
   
   根据当前气压推算海拔高度。
   公式：h = 44330 × (1 - (P/P0)^0.1903)
   其中 P0 = 101325 Pa（海平面标准大气压）
   
   输入：pressure_pa —— 当前气压（单位：Pa）
   输出：返回估算海拔（单位：米）
   注意：这只是估算值，实际海拔受天气影响
   ============================================= */
static float pressure_to_altitude(float pressure_pa) {
    float sea_level_pa = 101325.0f; // 海平面标准大气压（Pa）
    return 44330.0f * (1.0f - powf((pressure_pa / sea_level_pa), 0.1903f));
}

/* =============================================
   单次测量函数（强制模式 Forced Mode）
   
   BMP280 有三种工作模式：
   - Sleep 模式：不测量，最省电
   - Forced 模式：触发一次测量，完成后自动回到 Sleep
   - Normal 模式：持续周期性测量
   
   这里使用 Forced 模式：每次需要数据时手动触发一次测量。
   
   输出参数：
     temperature —— 温度（°C）
     pressure    —— 气压（Pa）
     altitude    —— 估算海拔（m）
   ============================================= */
static esp_err_t bmp280_force_measurement(float *temperature, float *pressure, float *altitude) {

    // 步骤1：向控制寄存器写入 0x25，触发一次强制测量
    // 0x25 = 0b00100101
    //   osrs_t = 001 → 温度过采样 x1（采1次样）
    //   osrs_p = 001 → 气压过采样 x1（采1次样）
    //   mode   = 01  → Forced 模式（触发单次测量）
    ESP_ERROR_CHECK(bmp280_write_reg(BMP280_REG_CTRL_MEAS, 0x25));

    // 步骤2：等待测量完成
    // 状态寄存器 0xF3 的 bit3（measuring 位）= 1 表示正在测量
    // 循环读取状态，直到 bit3 变为 0（测量完成）或超时
    uint8_t status;
    int timeout = 100; // 最多等待 100 × 10ms = 1秒
    do {
        vTaskDelay(pdMS_TO_TICKS(10)); // 等待 10 毫秒再检查
        bmp280_read_reg(BMP280_REG_STATUS, &status, 1);
    } while ((status & 0x08) && --timeout); // 0x08 = bit3，检查 measuring 位
    if (timeout == 0) return ESP_ERR_TIMEOUT; // 超时则返回错误

    // 步骤3：突发读取 6 字节原始数据（从 0xF7 开始）
    // 手册要求必须一次性连续读取全部 6 字节，保证数据一致性
    // 数据布局：[气压高8位][气压中8位][气压低4位+4位填充]
    //           [温度高8位][温度中8位][温度低4位+4位填充]
    uint8_t data[6];
    ESP_ERROR_CHECK(bmp280_read_reg(BMP280_REG_PRESS_MSB, data, 6));

    // 步骤4：将 3 字节拼接成 20 位 ADC 原始值
    // 气压：data[0]是高8位，data[1]是中8位，data[2]高4位是低4位
    int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    // 温度：data[3]是高8位，data[4]是中8位，data[5]高4位是低4位
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);

    // 步骤5：用校准参数进行补偿计算，得到真实物理值
    // 注意：必须先算温度（更新 t_fine），再算气压！
    *temperature = bmp280_compensate_T_int32(adc_T) / 100.0f; // 单位转换：0.01°C → °C
    *pressure    = bmp280_compensate_P_int64(adc_P) / 256.0f; // 单位转换：1/256 Pa → Pa
    *altitude    = pressure_to_altitude(*pressure);            // 由气压推算海拔

    return ESP_OK;
}

/* =============================================
   主函数 app_main()
   ESP-IDF 程序的入口，相当于 Arduino 的 setup() + loop()
   ============================================= */
void app_main(void) {
    ESP_LOGI(TAG, "BMP280 强制模式驱动启动");

    // ---- 第1步：初始化 I2C 总线 ----
    // 配置 I2C 总线参数（引脚、速率等）
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,       // 使用默认时钟源
        .i2c_port = I2C_PORT,                     // 使用 I2C 控制器 0
        .scl_io_num = I2C_MASTER_SCL_IO,          // SCL 引脚
        .sda_io_num = I2C_MASTER_SDA_IO,          // SDA 引脚
        .glitch_ignore_cnt = 7,                   // 毛刺过滤（忽略小于7个时钟周期的干扰）
        .flags.enable_internal_pullup = true,     // 启用内部上拉电阻
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle)); // 创建 I2C 总线

    // 在总线上注册 BMP280 设备
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,   // 7位 I2C 地址
        .device_address = BMP280_SENSOR_ADDR,     // BMP280 地址 0x76
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,       // 通信速率 400kHz
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle)); // 添加设备

    // ---- 第2步：验证芯片 ID ----
    // 读取 0xD0 寄存器，BMP280 应返回 0x58
    // 这一步确认连接的确实是 BMP280，而不是其他传感器
    uint8_t chip_id;
    ESP_ERROR_CHECK(bmp280_read_reg(BMP280_REG_CHIP_ID, &chip_id, 1));
    if (chip_id != 0x58) {
        ESP_LOGE(TAG, "BMP280 ID错误: 0x%02X", chip_id); // 打印错误并停止
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "BMP280 ID验证通过");

    // ---- 第3步：软复位 ----
    // 向复位寄存器写入 0xB6，让传感器恢复出厂默认状态
    // 复位后：Sleep 模式，过采样关闭，滤波器关闭
    bmp280_write_reg(BMP280_REG_RESET, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10)); // 等待 10ms 让复位完成

    // ---- 第4步：读取出厂校准参数 ----
    // 从 0x88 开始读取 24 字节，包含温度和气压的校准系数
    // 每个参数由 2 字节（低字节在前）拼成 16 位整数
    uint8_t calib[24];
    ESP_ERROR_CHECK(bmp280_read_reg(BMP280_REG_CALIB_START, calib, 24));
    dig_T1 = (uint16_t)(calib[1] << 8 | calib[0]); // 温度校准参数 T1
    dig_T2 = (int16_t) (calib[3] << 8 | calib[2]); // 温度校准参数 T2
    dig_T3 = (int16_t) (calib[5] << 8 | calib[4]); // 温度校准参数 T3
    dig_P1 = (uint16_t)(calib[7] << 8 | calib[6]); // 气压校准参数 P1
    dig_P2 = (int16_t) (calib[9] << 8 | calib[8]); // 气压校准参数 P2
    // ... P3 到 P9 同理，依次从 calib 数组中读取
    dig_P3 = (int16_t)(calib[11] << 8 | calib[10]);
    dig_P4 = (int16_t)(calib[13] << 8 | calib[12]);
    dig_P5 = (int16_t)(calib[15] << 8 | calib[14]);
    dig_P6 = (int16_t)(calib[17] << 8 | calib[16]);
    dig_P7 = (int16_t)(calib[19] << 8 | calib[18]);
    dig_P8 = (int16_t)(calib[21] << 8 | calib[20]);
    dig_P9 = (int16_t)(calib[23] << 8 | calib[22]);
    ESP_LOGI(TAG, "校准参数读取完成");

    // ---- 第5步：配置滤波器 ----
    // 向配置寄存器（0xF5）写入 0x00
    // 0x00 表示：IIR 滤波器关闭（Filter::Off），待机时间 0.5ms
    // 注意：根据 BMP280 规格，CONFIG 寄存器的写入必须在 Sleep 模式下进行，
    //       软复位后传感器默认处于 Sleep 模式，所以这里可以直接写入
    bmp280_write_reg(BMP280_REG_CONFIG, 0x00);

    // ---- 第6步：循环触发 5 次单次测量 ----
    // 每次调用 bmp280_force_measurement() 触发一次 Forced 模式测量：
    //   1. 写控制寄存器 → 传感器开始采样
    //   2. 等待采样完成
    //   3. 读取 6 字节原始数据
    //   4. 用校准参数换算成真实温度、气压、海拔
    float temp, press, altitude;
    for (int i = 0; i < 5; i++) {
        if (bmp280_force_measurement(&temp, &press, &altitude) == ESP_OK) {
            // press 单位是 Pa，除以 100 转换为 hPa（百帕，气象常用单位）
            // 例如：101325 Pa ÷ 100 = 1013.25 hPa（标准大气压）
            ESP_LOGI(TAG, "温度: %.2f °C | 气压: %.2f hPa | 海拔: %.2f m",
                     temp, press / 100.0f, altitude);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 每次测量间隔 2 秒
    }

    ESP_LOGI(TAG, "测量完成");
    // 程序结束，app_main 返回后 ESP-IDF 会自动删除此任务
}