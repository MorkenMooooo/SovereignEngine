#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

// -------------------------------------------------------
// LED 配置
// -------------------------------------------------------
#define LED_GPIO            GPIO_NUM_38
#define BLINK_INTERVAL_US   (5 * 1000000)   // 5 秒触发一次点亮
#define LED_ON_DURATION_US  (1 * 1000000)   // 亮 1 秒后熄灭

// -------------------------------------------------------
// I2C / AHT30 配置
// -------------------------------------------------------
#define I2C_MASTER_SCL_IO   5
#define I2C_MASTER_SDA_IO   6
#define AHT30_ADDR          0x38
#define AHT30_INTERVAL_US   (3 * 1000000)   // 每 3 秒采集一次

static const char *TAG_LED   = "ESP_TIMER_LED";
static const char *TAG_AHT30 = "AHT30";

// -------------------------------------------------------
// 定时器句柄
// -------------------------------------------------------
static esp_timer_handle_t led_on_timer_handle  = NULL;
static esp_timer_handle_t led_off_timer_handle = NULL;
static esp_timer_handle_t aht30_timer_handle   = NULL;

// -------------------------------------------------------
// AHT30 设备句柄（全局，供定时器回调使用）
// -------------------------------------------------------
static i2c_master_dev_handle_t aht30_device_handle = NULL;

// -------------------------------------------------------
// 回调 A：点亮 LED，启动 1 秒熄灭定时器
// -------------------------------------------------------
static void led_on_callback(void *arg)
{
    gpio_set_level(LED_GPIO, 1);
    ESP_LOGI(TAG_LED, "LED ON");
    ESP_ERROR_CHECK(esp_timer_start_once(led_off_timer_handle, LED_ON_DURATION_US));
}

// -------------------------------------------------------
// 回调 B：熄灭 LED，重新启动 5 秒触发定时器
// -------------------------------------------------------
static void led_off_callback(void *arg)
{
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG_LED, "LED OFF");
    ESP_ERROR_CHECK(esp_timer_start_once(led_on_timer_handle, BLINK_INTERVAL_US));
}

// -------------------------------------------------------
// 回调 C：每 3 秒触发，读取 AHT30 温湿度
// 注意：此回调运行在 esp_timer task 中，可以使用 vTaskDelay
// -------------------------------------------------------
static void aht30_read_callback(void *arg)
{
    // 发送触发测量命令
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00};
    esp_err_t ret = i2c_master_transmit(aht30_device_handle, trigger_cmd,
                                        sizeof(trigger_cmd), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT30, "Transmit failed: %s", esp_err_to_name(ret));
        return;
    }

    // 等待 AHT30 完成测量（典型 80ms，这里用 100ms）
    vTaskDelay(pdMS_TO_TICKS(100));

    // 读取 6 字节数据
    uint8_t data[6] = {0};
    ret = i2c_master_receive(aht30_device_handle, data, sizeof(data), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT30, "Receive failed: %s", esp_err_to_name(ret));
        return;
    }

    // 解析湿度（20 位原始值）
    uint32_t humidity_raw = ((uint32_t)data[1] << 12)
                          | ((uint32_t)data[2] << 4)
                          | (data[3] >> 4);

    // 解析温度（20 位原始值）
    uint32_t temperature_raw = (((uint32_t)data[3] & 0x0F) << 16)
                             | ((uint32_t)data[4] << 8)
                             | data[5];

    float humidity    = (float)humidity_raw    / 1048576.0f * 100.0f;
    float temperature = (float)temperature_raw / 1048576.0f * 200.0f - 50.0f;

    ESP_LOGI(TAG_AHT30, "Temperature: %.2f °C, Humidity: %.2f %%",
             temperature, humidity);
}

// -------------------------------------------------------
// app_main
// -------------------------------------------------------
void app_main(void)
{
    // =====================================================
    // 1. 初始化 LED GPIO
    // =====================================================
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);

    // =====================================================
    // 2. 初始化 I2C 总线
    // =====================================================
    i2c_master_bus_config_t bus_config = {
        .i2c_port                = I2C_NUM_0,
        .sda_io_num              = I2C_MASTER_SDA_IO,
        .scl_io_num              = I2C_MASTER_SCL_IO,
        .clk_source              = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt       = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // =====================================================
    // 3. 挂载 AHT30 设备
    // =====================================================
    i2c_device_config_t aht30_dev_config = {
        .dev_addr_length       = I2C_ADDR_BIT_LEN_7,
        .device_address        = AHT30_ADDR,
        .scl_speed_hz          = 100000,
        .flags.disable_ack_check = false,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle,
                                              &aht30_dev_config,
                                              &aht30_device_handle));
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待设备稳定

    // =====================================================
    // 4. 创建并启动 LED 定时器
    // =====================================================
    const esp_timer_create_args_t led_on_args = {
        .callback = &led_on_callback,
        .name     = "led_on_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_on_args, &led_on_timer_handle));

    const esp_timer_create_args_t led_off_args = {
        .callback = &led_off_callback,
        .name     = "led_off_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_off_args, &led_off_timer_handle));

    // 5 秒后第一次点亮 LED
    ESP_ERROR_CHECK(esp_timer_start_once(led_on_timer_handle, BLINK_INTERVAL_US));
    ESP_LOGI(TAG_LED, "LED timer started");

    // =====================================================
    // 5. 创建并启动 AHT30 周期定时器（每 3 秒触发一次）
    // =====================================================
    const esp_timer_create_args_t aht30_args = {
        .callback = &aht30_read_callback,
        .name     = "aht30_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&aht30_args, &aht30_timer_handle));

    // 立即触发第一次，之后每 3 秒周期触发
    ESP_ERROR_CHECK(esp_timer_start_periodic(aht30_timer_handle, AHT30_INTERVAL_US));
    ESP_LOGI(TAG_AHT30, "AHT30 timer started");

    // app_main 返回，定时器在后台继续运行
}