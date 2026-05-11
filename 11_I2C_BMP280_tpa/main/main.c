#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"


/* =============================================
   I2C 引脚与通信参数定义
   BMP280 通过 I2C 总线与 ESP32 通信
   ============================================= */
#define I2C_MASTER_SCL_IO          GPIO_NUM_7    // SCL（时钟线）连接的 GPIO 引脚
#define I2C_MASTER_SDA_IO          GPIO_NUM_15   // SDA（数据线）连接的 GPIO 引脚
#define I2C_MASTER_FREQ_HZ         400000        // I2C 通信速率：400kHz（快速模式）
#define I2C_PORT                   I2C_NUM_0     // 使用 ESP32 的 I2C 控制器 0
#define BMP280_SENSOR_ADDR         0x76          // BMP280 的 I2C 地址（SDO 引脚接 GND 时为 0x76）

/* =============================================
   BMP280 寄存器地址定义
   寄存器是传感器内部的"存储格子"，通过读写这些地址来控制传感器
   ============================================= */
#define bmp280_software_reset      0xB6           // BMP280 的软件复位指令
#define bmp280_chip_id             0xD0           // BMP280 的芯片 ID 寄存器地址，读取应返回 0x58
#define bmp280_calib_data_start    0x88           // 校准数据起始地址，连续读取 24 字节包含温度和气压的校准参数
#define ctrl_meas                  0xF4           // 控制测量寄存器地址，写入不同值可设置测量模式
#define iir_filter                 0xF5           // IIR 滤波器配置寄存器地址，写入不同值可设置滤波器强度和待机时间


i2c_master_bus_handle_t mooooo_i2c_bus_handle; // I2C 总线句柄，用于后续的 I2C 操作
i2c_master_dev_handle_t bmp280_dev_handle;     // BMP280 设备句柄，用于后续的设备通信




/* =============================================
   校准参数（出厂时烧录在传感器内部）
   BMP280 的原始 ADC 数据不是真实的温度/气压值，
   需要用这些校准参数进行数学补偿计算才能得到真实值
   ============================================= */
uint16_t dig_T1;  // 温度校准参数 1，unsigned short
int16_t  dig_T2;  // 温度校准参数 2，signed short
int16_t  dig_T3;  // 温度校准参数 3，signed short
uint16_t dig_P1;  // 气压校准参数 1，unsigned short
int16_t  dig_P2;  // 气压校准参数 2，signed short
int16_t  dig_P3;  // 气压校准参数 3，signed short
int16_t  dig_P4;  // 气压校准参数 4，signed short
int16_t  dig_P5;  // 气压校准参数 5，signed short
int16_t  dig_P6;  // 气压校准参数 6，signed short
int16_t  dig_P7;  // 气压校准参数 7，signed short
int16_t  dig_P8;  // 气压校准参数 8，signed short
int16_t  dig_P9;  // 气压校准参数 9，signed short




/* =============================================
   主函数 app_main()
   ESP-IDF 程序的入口，相当于 Arduino 的 setup() + loop()
   ============================================= */
void app_main(void)
{
    // ---- 第1步：初始化 I2C 总线 ----
    // 配置 I2C 总线参数（引脚、速率等）
    i2c_master_bus_config_t mooooo_i2c_bus_cfg = {
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .glitch_ignore_cnt = 7, // 典型值，单位为 I2C 模块时钟周期
        .flags.enable_internal_pullup = 1, // 启用内部上拉（如果没有外部上拉电阻）
    };
    // 初始化 I2C 总线
    ESP_ERROR_CHECK(i2c_new_master_bus(&mooooo_i2c_bus_cfg, &mooooo_i2c_bus_handle));

    // 在总线上注册 BMP280 设备
    i2c_device_config_t bmp280_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, // 7 位地址
        .device_address = BMP280_SENSOR_ADDR, // BMP280 的 I2C 地址
        .scl_speed_hz = I2C_MASTER_FREQ_HZ, // 400kHz
    };
    ESP_ERROR_CHECK( i2c_master_bus_add_device(mooooo_i2c_bus_handle, &bmp280_dev_cfg, &bmp280_dev_handle) );

    // ---- 第2步：验证芯片 ID ----
    // 读取 0xD0 寄存器，BMP280 应返回 0x58
    // 这一步确认连接的确实是 BMP280，而不是其他传感器
    uint8_t chip_id = 0;
    i2c_master_transmit_receive(bmp280_dev_handle, (uint8_t[]){0xD0}, 1, &chip_id, 1, 1000);
    if (chip_id != 0x58) {
        ESP_LOGI("BMP280 Chip ID", "chip id error: 0x%02X", chip_id);
        return;
    } else {
        ESP_LOGI("BMP280 Chip ID", "chip id correct: 0x%02X", chip_id);
    }


    // ---- 第3步：软复位 ----
    // 向复位寄存器写入 0xB6，让传感器恢复出厂默认状态
    // 复位后：Sleep 模式，过采样关闭，滤波器关闭
    i2c_master_transmit(bmp280_dev_handle, (uint8_t[]){bmp280_software_reset}, 1, 20);
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待 100ms 让复位完成


    // ---- 第4步：读取出厂校准参数 ----
    // 从 0x88 开始读取 24 字节，包含温度和气压的校准系数
    // 每个参数由 2 字节（低字节在前）拼成 16 位整数
    uint8_t recv_builtin_calibration_data[24]; // 存储从传感器读取的校准数据
    i2c_master_transmit_receive(bmp280_dev_handle, (uint8_t[]){bmp280_calib_data_start}, 1, 
                                 recv_builtin_calibration_data, 24, 1000);
    // 将读取到的T 、P 校准数据存储
    dig_T1 = (uint16_t)(recv_builtin_calibration_data[1] << 8) | recv_builtin_calibration_data[0]; // unsigned short
    dig_T2 = (int16_t)(recv_builtin_calibration_data[3] << 8) | recv_builtin_calibration_data[2]; // signed short
    dig_T3 = (int16_t)(recv_builtin_calibration_data[5] << 8) | recv_builtin_calibration_data[4]; // signed short
    dig_P1 = (uint16_t)(recv_builtin_calibration_data[7] << 8) | recv_builtin_calibration_data[6]; // unsigned short
    dig_P2 = (int16_t)(recv_builtin_calibration_data[9] << 8) | recv_builtin_calibration_data[8]; // signed short
    dig_P3 = (int16_t)(recv_builtin_calibration_data[11] << 8) | recv_builtin_calibration_data[10]; // signed short
    dig_P4 = (int16_t)(recv_builtin_calibration_data[13] << 8) | recv_builtin_calibration_data[12]; // signed short
    dig_P5 = (int16_t)(recv_builtin_calibration_data[15] << 8) | recv_builtin_calibration_data[14]; // signed short
    dig_P6 = (int16_t)(recv_builtin_calibration_data[17] << 8) | recv_builtin_calibration_data[16]; // signed short
    dig_P7 = (int16_t)(recv_builtin_calibration_data[19] << 8) | recv_builtin_calibration_data[18]; // signed short  
    dig_P8 = (int16_t)(recv_builtin_calibration_data[21] << 8) | recv_builtin_calibration_data[20]; // signed short
    dig_P9 = (int16_t)(recv_builtin_calibration_data[23] << 8) | recv_builtin_calibration_data[22]; // signed short
    
    ESP_LOGI("BMP280 Calibration", "dig_T1: %u, dig_T2: %d, dig_T3: %d", dig_T1, dig_T2, dig_T3);
    ESP_LOGI("BMP280 Calibration", "dig_P1: %u, dig_P2: %d, dig_P3: %d", dig_P1, dig_P2, dig_P3);
    ESP_LOGI("BMP280 Calibration", "dig_P4: %d, dig_P5: %d, dig_P6: %d", dig_P4, dig_P5, dig_P6);
    ESP_LOGI("BMP280 Calibration", "dig_P7: %d, dig_P8: %d, dig_P9: %d", dig_P7, dig_P8, dig_P9);
    ESP_LOGI("BMP280 Calibration", "Calibration data read successfully");

    // ---- 第5步：配置滤波器 ----
    // 向配置寄存器（0xF5）写入 0x00
    // 0x00 表示：IIR 滤波器关闭（Filter::Off），待机时间 0.5ms
    // 注意：根据 BMP280 规格，CONFIG 寄存器的写入必须在 Sleep 模式下进行，
    //       软复位后传感器默认处于 Sleep 模式，所以这里可以直接写入
    i2c_master_transmit(bmp280_dev_handle, (uint8_t[]){iir_filter, 0x00}, 2, 20);

    // ---- 第6步：循环forced mode 模式进行测量 ----
    // 每次调用 bmp280_force_measurement() 触发一次 Forced 模式测量：
    //   1. 写控制寄存器 → 传感器开始采样
    //   2. 等待采样完成
    //   3. 读取 6 字节原始数据
    //   4. 用校准参数换算成真实温度、气压、海拔
    while (1) { 
        // 步骤1：向控制寄存器写入 0x25，触发一次强制测量
        // 0x25 = 0b00100101
        //   osrs_t = 001 → 温度过采样 x1（采1次样）
        //   osrs_p = 001 → 气压过采样 x1（采1次样）
        //   mode   = 01  → Forced 模式（触发单次测量）
        i2c_master_transmit(bmp280_dev_handle, (uint8_t[]){ctrl_meas, 0x25}, 2, 20);

        // 步骤2：等待测量完成
        // 状态寄存器 0xF3 的 bit3（measuring 位）= 1 表示正在测量
        // 循环读取状态，直到 bit3 变为 0（测量完成）或超时
        uint8_t status;
        int wait_time_out = 100;
        do
        {
            vTaskDelay(pdMS_TO_TICKS(10)); // 每次等待 10ms 后再读状态
            i2c_master_transmit_receive(bmp280_dev_handle, (uint8_t[]){0xF3}, 1, &status, 1, 1000);
        } while ( (status & 0x08) && --wait_time_out ); // 等待 measuring 位清零或超时

        if (wait_time_out == 0) {
            ESP_LOGI("BMP280 Measurement", "measurement timeout");
            return; // 测量超时，退出程序
        }

        // 步骤3：突发读取 6 字节原始数据（从 0xF7 开始）
        // 手册要求必须一次性连续读取全部 6 字节，保证数据一致性
        // 数据布局：[气压高8位][气压中8位][气压低4位+4位填充]
        //           [温度高8位][温度中8位][温度低4位+4位填充]
        uint8_t data[6];
        i2c_master_transmit_receive(bmp280_dev_handle, (uint8_t[]){0xF7}, 1, data, 6, 1000);

        // 步骤4：将 3 字节拼接成 20 位 ADC 原始值
        // 气压：data[0]是高8位，data[1]是中8位，data[2]高4位是低4位
        int32_t adc_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
        int32_t adc_temp = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
        printf("adc_press: %" PRId32 ", adc_temp: %" PRId32 "\n", adc_press, adc_temp);



        // 步骤5：用校准参数进行补偿计算，得到真实物理值
        // 注意：必须先算温度（更新 t_fine），再算气压！

        /* =============================================
        温度补偿算法（来自 BMP280 官方手册 3.11.3 节）
        
        传感器 ADC 读出的是原始数字值，不是摄氏度。
        这个函数用校准参数把原始值换算成真实温度。
        
        输入：adc_T —— 传感器读出的原始温度 ADC 值（20位）
        输出：返回温度值（单位：0.01°C，例如返回 2510 表示 25.10°C）
        副作用：同时计算并保存 t_fine（供气压补偿使用）
        ============================================= */
        int32_t var1, var2, T;
        // 第一项：线性补偿
        var1 = ((((adc_temp>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
        // 第二项：二次补偿（修正非线性误差）
        var2 = ( ((((adc_temp>>4) - ((int32_t)dig_T1)) * ((adc_temp>>4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3) ) >> 14;
        // t_fine 是温度的精细中间值，后续气压补偿也需要它
        int32_t t_fine = var1 + var2;
        // 最终温度值（单位 0.01°C）
        T = (t_fine * 5 + 128) >> 8;
        printf("Temperature: %.2f°C\n", T / 100.0f); // 输出温度值，单位 0.01°C
        

        /* =============================================
        气压补偿算法（来自 BMP280 官方手册 3.11.3 节）
        
        同样，ADC 读出的气压原始值需要用校准参数换算。
        注意：必须先调用温度补偿函数（更新 t_fine），再调用此函数！
        
        输入：adc_P —— 传感器读出的原始气压 ADC 值（20位）
        输出：返回气压值（单位：1/256 Pa，例如返回 25600000 表示 100000 Pa = 1000 hPa）
        ============================================= */
        int64_t p_var1, p_var2, pressure;
        // 利用 t_fine 进行温度相关的气压修正
        p_var1 = ((int64_t)t_fine) - 128000;
        p_var2 = p_var1 * p_var1 * (int64_t)dig_P6;
        p_var2 = p_var2 + ((p_var1*(int64_t)dig_P5)<<17);
        p_var2 = p_var2 + (((int64_t)dig_P4)<<35);
        p_var1 = ((p_var1 * p_var1 * (int64_t)dig_P3)>>8) + ((p_var1 * (int64_t)dig_P2)<<12);
        p_var1 = (((((int64_t)1)<<47)+p_var1))*((int64_t)dig_P1)>>33;
        pressure = 1048576 - adc_press;
        pressure = (((pressure<<31) - p_var2) * 3125) / p_var1;
        p_var1 = (((int64_t)dig_P9) * (pressure>>13) * (pressure>>13)) >> 25;
        p_var2 = (((int64_t)dig_P8) * pressure) >> 19;
        // 最终气压值（单位：1/256 Pa）
        pressure = ((pressure + p_var1 + p_var2) >> 8) + (((int64_t)dig_P7)<<4);
        float pressure_hPa = ( (float)pressure / 256.0f ) / 100.0f; //  1/256 Pa ➡️ Pa ➡️ hPa 
        printf("Pressure: %.2f hPa\n", pressure_hPa); // 输出气压值，单位 hPa


        /* =============================================
        海拔换算函数（国际标准大气压模型）
        
        根据当前气压推算海拔高度。
        公式：h = 44330 × (1 - (P/P0)^0.1903)
        其中 P0 = 101325 Pa（海平面标准大气压）
        
        输入：pressure_pa —— 当前气压（单位：Pa）
        输出：返回估算海拔（单位：米）
        注意：这只是估算值，实际海拔受天气影响
        ============================================= */
        float sea_level_pa = 101325.0f; // 海平面标准大气压（Pa）
        float altitude = 44330.0f * (1.0f - powf( ( (float)pressure / 256.0f ) / sea_level_pa, 0.1903f));
        printf("Altitude: %.2f m\n", altitude); // 输出海拔值，单位：米

        vTaskDelay(pdMS_TO_TICKS(5000)); // 5s 后再次测量
    }



}
