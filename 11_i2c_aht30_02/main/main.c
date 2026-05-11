#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"  // 引入新版 I2C 主机驱动头文件


// 定义 I2C 引脚和参数
#define I2C_MASTER_SCL_IO           5           // SCL 引脚
#define I2C_MASTER_SDA_IO           6           // SDA 引脚


void app_main(void)
{
    // =========================================================
    // 第一步：配置并初始化 I2C 主机总线
    // =========================================================
    // i2c_master_bus_config_t 是总线配置结构体，描述这条 I2C 总线的硬件参数
    i2c_master_bus_config_t my_i2c_bus_config = {
        .i2c_port = I2C_NUM_0,                   // I2C 总线编号
        .sda_io_num = I2C_MASTER_SDA_IO,         // SDA 引脚编号
        .scl_io_num = I2C_MASTER_SCL_IO,         // SCL 引脚编号
        .clk_source = I2C_CLK_SRC_DEFAULT,       // 时钟源选择，使用默认
        .glitch_ignore_cnt = 7,                  // 抖动过滤周期，单位是 I2C 模块时钟周期
        .flags.enable_internal_pullup = true,             // 启用内部上拉电阻
    };
    
    i2c_master_bus_handle_t my_i2c_bus_handle; // 总线句柄，后续操作都需要用到它
    ESP_ERROR_CHECK(i2c_new_master_bus(&my_i2c_bus_config, &my_i2c_bus_handle)); // 初始化 I2C 主机总线，并获取总线句柄



    // =========================================================
    // 第二步：将 AHT30 设备挂载到 I2C 总线上
    // =========================================================
    // i2c_device_config_t 是设备配置结构体，描述挂载在总线上的某个从机设备

    i2c_device_config_t my_aht30_device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,              // 设备地址长度，AHT30 使用 7 位地址
        .device_address = 0x38,                // 设备地址，AHT30 的默认地址是 0x38
        .scl_speed_hz = 100000,                // 时钟频率，AHT30 支持的最大时钟是 400 kHz
        .flags.disable_ack_check = false,          // 启用 ACK 检查，确保通信可靠
    };
    i2c_master_dev_handle_t my_aht30_device_handle; // 从机设备句柄，后续对设备的读写都需要用到它
    ESP_ERROR_CHECK(i2c_master_bus_add_device(my_i2c_bus_handle, 
                                    &my_aht30_device_config, &my_aht30_device_handle)); // 将设备挂载

    
    // 至此，我们已经完成了对 AHT30 设备的初始化
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待 1 秒，确保设备稳定

    // =========================================================
    // 主循环：每隔 2 秒读取一次温湿度数据
    // =========================================================
    while (1)
    {
        /* code */
        // 第四步：发送触发测量命令，通知 AHT30 开始采集温湿度数据
        uint8_t mooooo_aht30_trigger_cmd[3] = {0xAC, 0x33, 0x00}; // AHT30 的触发测量命令
        i2c_master_transmit(my_aht30_device_handle, mooooo_aht30_trigger_cmd, 
                            sizeof(mooooo_aht30_trigger_cmd), -1);
                            
        // AHT30典型值等待 80 ms，确保 AHT30 完成测量，这里我们等待 100 ms，稍微宽裕一些                    
        vTaskDelay(pdMS_TO_TICKS(100)); 
        
        // 第五步：读取测量结果，AHT30 会返回 6 字节数据
        uint8_t mooooo_aht30_data[6] = {0}; // 用于存储从 AHT30 读取的数据

        i2c_master_receive(my_aht30_device_handle, mooooo_aht30_data, sizeof(mooooo_aht30_data), -1);
    
        // =========================================================
        // 第七步：解析原始数据，转换为实际温湿度值
        // =========================================================

        // AHT30 返回的数据格式如下：
        // Byte 0: 状态字节，最高位是测量完成标志
        // Byte 1: 湿度数据高8位
        // Byte 2: 湿度数据低8位
        // 湿度原始值（20位）：Byte 0 & Byte 1 & Byte 2 的高4位
        // 通过位移和或运算拼接成 20 位整数
        uint32_t humidity_raw = ((uint32_t)mooooo_aht30_data[1] << 12)   // mooooo_aht30_data[1] 左移 12 位
                              | ((uint32_t)mooooo_aht30_data[2] << 4)    // mooooo_aht30_data[2] 左移 4 位
                              | (mooooo_aht30_data[3] >> 4);             // mooooo_aht30_data[3] 右移 4 位取高 4 位

        // 温度原始值（20位）：
        //   data[3] 的低 4 位作为高位
        //   data[4] 的全部 8 位作为中间位
        //   data[5] 的全部 8 位作为低位
        uint32_t temperature_raw = (((uint32_t)mooooo_aht30_data[3] & 0x0F) << 16)  // mooooo_aht30_data[3] 低4位左移16位
                                 | ((uint32_t)mooooo_aht30_data[4] << 8)             // mooooo_aht30_data[4] 左移 8 位
                                 | mooooo_aht30_data[5];                             // mooooo_aht30_data[5] 直接作为最低位
        // 根据 AHT30 的数据转换公式，将原始值转换为实际的温湿度值

        // 将原始值转换为实际湿度（%RH）
        // 公式来自 AHT30 数据手册：湿度 = 原始值 / 2^20 * 100
        float humidity    = (float)humidity_raw    / 1048576.0f * 100.0f;

        // 将原始值转换为实际温度（°C）
        // 公式来自 AHT30 数据手册：温度 = 原始值 / 2^20 * 200 - 50
        float temperature = (float)temperature_raw / 1048576.0f * 200.0f - 50.0f;

        ESP_LOGI("AHT30", "Temperature: %.2f °C, Humidity: %.2f %%", temperature, humidity);

        vTaskDelay(pdMS_TO_TICKS(2000));    // 等待2秒，再次读取

    }
    
}
