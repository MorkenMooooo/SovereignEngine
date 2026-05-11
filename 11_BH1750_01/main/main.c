#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "BH1750";

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
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

// =============================================
// I2C 初始化 & 设备探测
// =============================================
esp_err_t bh1750_init(void) {
    // 1. 创建 I2C 总线
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    // 2. 探测设备是否在线
    esp_err_t ret = i2c_master_probe(bus_handle, BH1750_ADDR, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 未找到 (地址 0x%02X)", BH1750_ADDR);
        return ret;
    }
    ESP_LOGI(TAG, "BH1750 已找到 (地址 0x%02X)", BH1750_ADDR);

    // 3. 添加设备到总线
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // 4. 软件复位（解决 DVI 上拉带来的复位问题）
    uint8_t cmd = BH1750_CMD_RESET;
    ret = i2c_master_transmit(dev_handle, &cmd, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "复位命令失败");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(5));

    // 5. 上电唤醒
    cmd = BH1750_CMD_POWER_ON;
    ret = i2c_master_transmit(dev_handle, &cmd, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power On 命令失败");
        return ret;
    }

    return ESP_OK;
}

// =============================================
// 单次测量（高分辨率模式）
// =============================================
esp_err_t bh1750_measure_once(float *lux) {
    // 1. 发送测量命令
    uint8_t cmd = BH1750_CMD_ONE_TIME_H_RES;
    esp_err_t ret = i2c_master_transmit(dev_handle, &cmd, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "测量命令发送失败");
        return ret;
    }

    // 2. 等待测量完成 (高分辨率模式需要 120-180ms)
    vTaskDelay(pdMS_TO_TICKS(180));

    // 3. 读取 2 字节数据
    uint8_t data[2];
    ret = i2c_master_receive(dev_handle, data, 2, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "数据读取失败");
        return ret;
    }

    // 4. 计算光照强度
    uint16_t raw_data = (data[0] << 8) | data[1];
    *lux = (float)raw_data / 1.2f;

    return ESP_OK;
}

// =============================================
// 主程序
// =============================================
void app_main(void) {
    ESP_LOGI(TAG, "BH1750 测试程序启动");

    // 初始化 BH1750
    if (bh1750_init() != ESP_OK) {
        ESP_LOGE(TAG, "初始化失败，程序退出");
        return;
    }

    // 主循环测量
    while (1) {
        float lux;
        if (bh1750_measure_once(&lux) == ESP_OK) {
            ESP_LOGI(TAG, "光照强度: %.2f lux", lux);
        } else {
            ESP_LOGE(TAG, "测量失败");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 每2秒测量一次
    }
}