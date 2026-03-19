/*
 * Copyright 2026 MorkenMooooo
 * Author: MorkenMooooo
 * Platform: Bilibili & Douyin @ 门主引擎 | Mooooo
 * Contact: itsntcool@qq.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"  // 提供 esp_timer_get_time()

// ================= 全局定义 =================
#define TAG "gpio_intr"

#define BUTTON_PIN   GPIO_NUM_21
#define LED_PIN      GPIO_NUM_2

// 队列句柄（必须在 ISR 前声明）
QueueHandle_t gpio_event_queue = NULL;

// 自定义事件结构体
typedef struct {
    uint32_t gpio_num;
    uint64_t timestamp_us;
} gpio_event_t;

// ================= ISR 函数（去掉 static，保留 IRAM_ATTR）=================
void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    gpio_event_t event = {
        .gpio_num = gpio_num,
        .timestamp_us = esp_timer_get_time()
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gpio_event_queue, &event, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ================= 任务函数 =================
void gpio_task(void* arg)
{
    gpio_event_t event;
    for (;;) {
        if (xQueueReceive(gpio_event_queue, &event, portMAX_DELAY)) {
            // === 软件消抖：等待 30ms 后再次检查电平 ===
            vTaskDelay(pdMS_TO_TICKS(30));  // 关键：消抖延时

            // 重新读取当前电平（必须是低电平才认为是有效按下）
            if (gpio_get_level(BUTTON_PIN) == 0) {
                ESP_LOGI(TAG, "Button pressed (debounced)");

                // 翻转 LED
                int led_state = gpio_get_level(LED_PIN);
                gpio_set_level(LED_PIN, !led_state);
            } else {
                ESP_LOGI(TAG, "False trigger (noise or bounce)");
            }
        }
    }
}

// ================= 主函数 =================
void app_main(void)
{
    // 1. 配置 LED 引脚（输出）
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(LED_PIN, 0); // 初始关闭

    // 2. 配置按钮引脚（输入 + 上拉 + 下降沿中断）
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // 按键接 GND，启用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE         // 按下时产生下降沿
    };
    gpio_config(&button_config);

    // 3. 创建队列
    gpio_event_queue = xQueueCreate(10, sizeof(gpio_event_t));
    if (!gpio_event_queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // 4. 安装 ISR 服务
    gpio_install_isr_service(0);

    // 5. 注册 ISR 到按钮引脚
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*)BUTTON_PIN);

    // 6. 创建任务
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "GPIO interrupt example ready. Press button on GPIO %d", BUTTON_PIN);
}