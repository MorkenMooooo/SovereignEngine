#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#define LED_STRIP_GPIO_NUM   48
#define LED_STRIP_LED_COUNT  1
#define BREATH_STEPS         100    // 呼吸一个周期的步数
#define BREATH_DELAY_MS      10     // 每步延迟(ms)，总周期约 2s

static const char *TAG = "RGB_BREATH";

// 定义多个颜色 (R, G, B)
static const uint8_t colors[][3] = {
    {255, 0,   0},   // 红
    {0,   255, 0},   // 绿
    {0,   0,   255}, // 蓝
    {255, 128, 0},   // 橙
    {128, 0,   255}, // 紫
};
#define COLOR_COUNT (sizeof(colors) / sizeof(colors[0]))

void app_main(void)
{
    // 初始化 LED strip（RMT 后端）
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_NUM,
        .max_leds = LED_STRIP_LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    int color_index = 0;

    while (1) {
        uint8_t r = colors[color_index][0];
        uint8_t g = colors[color_index][1];
        uint8_t b = colors[color_index][2];

        // 渐亮：亮度从 0 → 最大
        for (int i = 0; i <= BREATH_STEPS; i++) {
            float brightness = (float)i / BREATH_STEPS;
            led_strip_set_pixel(led_strip, 0,
                                (uint8_t)(r * brightness),
                                (uint8_t)(g * brightness),
                                (uint8_t)(b * brightness));
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }

        // 渐暗：亮度从 最大 → 0
        for (int i = BREATH_STEPS; i >= 0; i--) {
            float brightness = (float)i / BREATH_STEPS;
            led_strip_set_pixel(led_strip, 0,
                                (uint8_t)(r * brightness),
                                (uint8_t)(g * brightness),
                                (uint8_t)(b * brightness));
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(BREATH_DELAY_MS));
        }

        // 切换到下一个颜色
        color_index = (color_index + 1) % COLOR_COUNT;
        ESP_LOGI(TAG, "Switching to color index %d", color_index);
    }
}
