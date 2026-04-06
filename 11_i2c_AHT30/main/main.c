#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO   5
#define I2C_MASTER_SDA_IO   6
#define AHT30_I2C_ADDR      0x38
#define I2C_MASTER_FREQ_HZ  100000

static const char *TAG = "AHT30";

// AHT30 命令
static const uint8_t AHT30_CMD_INIT[]    = {0xBE, 0x08, 0x00};
static const uint8_t AHT30_CMD_TRIGGER[] = {0xAC, 0x33, 0x00};

void app_main(void)
{
    // 1. 初始化 I2C 主机总线
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    // 2. 将 AHT30 设备挂载到总线
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT30_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // 3. 发送初始化命令
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, AHT30_CMD_INIT, sizeof(AHT30_CMD_INIT), -1));
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {
        // 4. 发送触发测量命令
        ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, AHT30_CMD_TRIGGER, sizeof(AHT30_CMD_TRIGGER), -1));

        // 5. 等待测量完成（AHT30 典型测量时间约 80ms）
        vTaskDelay(pdMS_TO_TICKS(100));

        // 6. 读取 6 字节数据
        uint8_t data[6] = {0};
        ESP_ERROR_CHECK(i2c_master_receive(dev_handle, data, sizeof(data), -1));

        // 7. 解析数据
        uint32_t humidity_raw    = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
        uint32_t temperature_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

        float humidity    = (float)humidity_raw    / 1048576.0f * 100.0f;
        float temperature = (float)temperature_raw / 1048576.0f * 200.0f - 50.0f;

        ESP_LOGI(TAG, "Temperature: %.2f °C, Humidity: %.2f %%", temperature, humidity);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // 清理资源
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
}
