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
#include "driver/uart.h"
#include "driver/gpio.h"
#include "string.h"
#include "esp_log.h"

#define tx_pin GPIO_NUM_43
#define rx_pin GPIO_NUM_44

void app_main(void)
{

    // gpio结构体（LED灯，GPIO num 2）
    gpio_config_t my_led_gpio_config = {0};
    my_led_gpio_config.pin_bit_mask = 1ULL << GPIO_NUM_2;
    my_led_gpio_config.mode = GPIO_MODE_OUTPUT;
    my_led_gpio_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    my_led_gpio_config.pull_up_en = GPIO_PULLUP_DISABLE;
    my_led_gpio_config.intr_type = GPIO_INTR_DISABLE;
    // 使能GPIO（LED）
    gpio_config(&my_led_gpio_config);

    
    // UART的结构体
    uart_config_t my_uart_config = {0};
    my_uart_config.baud_rate = 115200;
    my_uart_config.data_bits = UART_DATA_8_BITS;
    my_uart_config.parity = UART_PARITY_DISABLE;
    my_uart_config.stop_bits = UART_STOP_BITS_1;
    my_uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    my_uart_config.source_clk = UART_SCLK_DEFAULT;
    // UART建立通信过程
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &my_uart_config);
    uart_set_pin(UART_NUM_0, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uint8_t data[128];
    int bytes_length = 0;

    while (1)
    {
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM_0, (size_t *) &bytes_length) );
        int rxBytes = uart_read_bytes(UART_NUM_0, data, bytes_length, 100);
        if (rxBytes > 0){
            data[rxBytes] = 0;
            ESP_LOGI("uart_log", "%s\n", data);

            if (data[0] == '1'){
                gpio_set_level(GPIO_NUM_2, 1);
            }

            if (data[0] == '0'){
                gpio_set_level(GPIO_NUM_2, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    

}
