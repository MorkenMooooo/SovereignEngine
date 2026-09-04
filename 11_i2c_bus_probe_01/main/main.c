#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

// 你当前使用的引脚
#define I2C_SCL_PIN  GPIO_NUM_42
#define I2C_SDA_PIN  GPIO_NUM_41
#define I2C_PORT     I2C_NUM_0

static const char *TAG = "I2C_SCAN";

// 扫描所有I2C设备（完整遍历，不中断，找到所有设备）
void i2c_scan_devices(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    i2c_master_bus_handle_t bus_handle;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线初始化失败: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "========== 开始扫描所有I2C设备 ==========");
    // 遍历 0x03 ~ 0x77 所有I2C地址，一个都不漏
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        // 探测当前地址，超时500ms（适配多传感器）
        if (i2c_master_probe(bus_handle, addr, pdMS_TO_TICKS(500)) == ESP_OK) {
            // 找到设备就打印，不跳出循环，继续扫描下一个地址
            ESP_LOGI(TAG, "✅ 找到I2C设备: 0x%02X", addr);
        }
    }

    ESP_LOGI(TAG, "========== I2C扫描全部完成 ==========");
}

void app_main(void) {
    i2c_scan_devices();
}