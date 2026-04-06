#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#define LED_STRIP_GPIO_NUM   48      // GPIO38，对应你的 v1.1 开发板
#define LED_STRIP_LED_COUNT  1       // 板载只有 1 个 RGB LED

static const char *TAG = "WS2812";

void app_main(void)
{
    // 配置 LED strip（使用 RMT 后端）
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_NUM,
        .max_leds = LED_STRIP_LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // 清除初始状态
    led_strip_clear(led_strip);

    while (1) {
        // 设置为红色 (R=16, G=0, B=0)
        led_strip_set_pixel(led_strip, 0, 16, 0, 0);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "LED Red");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 设置为绿色
        led_strip_set_pixel(led_strip, 0, 0, 16, 0);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "LED Green");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 设置为蓝色
        led_strip_set_pixel(led_strip, 0, 0, 0, 16);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "LED Blue");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 关闭
        led_strip_clear(led_strip);
        ESP_LOGI(TAG, "LED Off");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
