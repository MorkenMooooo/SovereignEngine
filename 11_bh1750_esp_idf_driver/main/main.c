#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"


// =============================================
// 引脚与参数定义
// =============================================
#define I2C_MASTER_SCL_IO          GPIO_NUM_16
#define I2C_MASTER_SDA_IO          GPIO_NUM_17
#define I2C_MASTER_FREQ_HZ         100000
#define I2C_PORT                   I2C_NUM_0

// BH1750 地址（ADDR 接地）
#define BH1750_ADDR                0x23        // 7位地址
#define BH1750_WRITE_ADDR          0x46        // 写地址 (0x23 << 1)
#define BH1750_READ_ADDR           0x47        // 读地址 (0x23 << 1) | 1

// =============================================
// BH1750 命令定义
// =============================================
#define BH1750_CMD_POWER_ON        0x01        // 上电
#define BH1750_CMD_RESET           0x07        // 软复位
#define BH1750_CMD_ONE_TIME_H_RES  0x20        // 单次高分辨率模式 (1 lx, 120ms)

// =============================================
// 全局句柄
// =============================================
i2c_master_bus_handle_t mooooo_i2c_bus_handle = NULL;
i2c_master_dev_handle_t mooooo_i2c_dev_handle = NULL;


void bh1750_init(void)
{ 
    // 1. 创建 I2C 总线
    i2c_master_bus_config_t i2c_cfg = {
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, // 忽略 7 个时钟周期内的毛刺
        .flags.enable_internal_pullup = true,
        .i2c_port = I2C_PORT,
    };
    // 创建 I2C 总线
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &mooooo_i2c_bus_handle));

    // 2. 探测设备是否在线
    esp_err_t result = i2c_master_probe(mooooo_i2c_bus_handle, BH1750_ADDR, 1000);
    if (result != ESP_OK) {
        ESP_LOGE("BH1750", "Device not found at address 0x%02X", BH1750_ADDR);
        return;
    }
    ESP_LOGI("BH1750", "Device found at address 0x%02X", BH1750_ADDR);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. 添加设备到总线
    i2c_device_config_t bh1750_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .flags.disable_ack_check = false, // 启用 ACK 检查
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(mooooo_i2c_bus_handle, &bh1750_dev_cfg, &mooooo_i2c_dev_handle));

    // 4. 上电唤醒
    result = i2c_master_transmit(mooooo_i2c_dev_handle, (uint8_t[]){BH1750_CMD_POWER_ON}, 1, 1000);
    if (result != ESP_OK) {
        ESP_LOGE("BH1750", "Power on failed");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5. 软件复位（解决 DVI 上拉带来的复位问题）
    result = i2c_master_transmit(mooooo_i2c_dev_handle, (uint8_t[]){BH1750_CMD_RESET}, 1, 1000);
    if (result != ESP_OK) {
        ESP_LOGE("BH1750", "Reset failed");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待复位完成
}


// =============================================
// 主程序
// =============================================
void app_main(void)
{
    // 初始化 BH1750
    bh1750_init();

    // 主循环测量
    while (1)
    {
        // 1. 发送测量命令
        esp_err_t result = i2c_master_transmit(mooooo_i2c_dev_handle, 
            (uint8_t[]){BH1750_CMD_ONE_TIME_H_RES}, 1, 1000);
        
        if (result != ESP_OK) {
            ESP_LOGE("BH1750", "Measure failed");
            return;
        }
        // 2. 等待测量完成 (高分辨率模式需要 120-180ms)
        vTaskDelay(pdMS_TO_TICKS(180));

        // 3. 读取 2 字节数据
        uint8_t data[2] = {0};
        result = i2c_master_receive(mooooo_i2c_dev_handle, data, 2, 1000);
        if (result != ESP_OK) {
            ESP_LOGE("BH1750", "Read failed");
            return;
        }

        // 4. 计算光照强度 (单位 lux)
        uint16_t raw_value = (data[0] << 8) | data[1];
        float lux = (float)raw_value / 1.2; // BH1750 的转换公式

        ESP_LOGI("BH1750", "Lux: %.2f lux", lux);

        vTaskDelay(pdMS_TO_TICKS(2000));

    }
}
