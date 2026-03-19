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
#include "driver/spi_master.h"

void app_main(void)
{

    gpio_config_t my_led_pin_config = {0};
    my_led_pin_config.pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4);
    my_led_pin_config.mode = GPIO_MODE_OUTPUT;
    my_led_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
    my_led_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    my_led_pin_config.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&my_led_pin_config);

    gpio_set_level(GPIO_NUM_2, 1);
    gpio_set_level(GPIO_NUM_4, 1);



}
