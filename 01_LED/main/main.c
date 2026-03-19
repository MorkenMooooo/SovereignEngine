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

#define led_pin_2 GPIO_NUM_2
#define high 1
#define low 0

void app_main(void)
{   
    gpio_set_direction(led_pin_2, GPIO_MODE_OUTPUT);
    printf("hello world\n");
    while(1){
        
        gpio_set_level(led_pin_2, low);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(led_pin_2, high);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    

}
