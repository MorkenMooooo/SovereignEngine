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
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define button_pin_2 GPIO_NUM_2

void app_main(void)
{

    // 1，配置GPIO的模式
    gpio_set_direction(button_pin_2, GPIO_MODE_INPUT);
    // 2，配置成上拉输入
    gpio_set_pull_mode(button_pin_2, GPIO_PULLUP_ONLY);

    // 3，while 循环检测GPIO电平变化
    // 4，if检测到则printf
    while (1)
    {
        uint8_t input_level = gpio_get_level(button_pin_2);
        if (input_level == 0){
            printf("%d\n", input_level);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}
