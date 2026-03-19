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
#include "driver/uart.h"
#include "hal/uart_hal.h"
#include "esp_log.h"

QueueHandle_t my_uart_intr_queue_handle;

void my_led_initialize(void){
    gpio_config_t my_gpio_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&my_gpio_config);
    gpio_set_level(GPIO_NUM_4, 0);
}


void led_green_2(void *pvParameters){

    for(;;){
        gpio_set_level(GPIO_NUM_2, 1);	// 开灯
		vTaskDelay(pdMS_TO_TICKS(100));
		gpio_set_level(GPIO_NUM_2, 0);	// 关灯
		vTaskDelay(pdMS_TO_TICKS(100));	// 100毫秒
    }
}


void led_red_4(void * pvParameters)
{   uart_event_t my_uart_intr_event_type;
    uint8_t rx_buffer_zone[128];
    for(;;)
    {
        if(xQueueReceive(my_uart_intr_queue_handle, &my_uart_intr_event_type, portMAX_DELAY)){
            if( my_uart_intr_event_type.type == UART_DATA && my_uart_intr_event_type.size > 0){
                ESP_LOGI("uart_intr_trigger", "bytes length: %d", my_uart_intr_event_type.size);

                uart_read_bytes(UART_NUM_0, rx_buffer_zone, my_uart_intr_event_type.size, pdMS_TO_TICKS(100));

                if (my_uart_intr_event_type.size >= 6 &&  memcmp(rx_buffer_zone, "led_on", 6) == 0 ){
                    gpio_set_level(GPIO_NUM_4, 1);
                    ESP_LOGI("Received CMD", "LED ON!");
                }

                if (my_uart_intr_event_type.size >= 7 && memcmp(rx_buffer_zone, "led_off", 7) == 0 ){
                    gpio_set_level(GPIO_NUM_4, 0);
                    ESP_LOGI("Received CMD", "LED OFF!");
                }

            }

        }
        
    }
}


void app_main(void)
{   /*一、配置GPIO结构，再使能GPIO*/
    my_led_initialize();

    /*二、注册UART及启动中断，配置UART结构体parameters并使能，配置UART RX以及TX引脚*/
    uart_config_t my_uart_tele_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_NUM_0, 1024, 0, 2, &my_uart_intr_queue_handle, 0);
    uart_param_config(UART_NUM_0, &my_uart_tele_config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_43, GPIO_NUM_44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /*三、显示配置UART中断参数并启用uart intr config() */
    uart_intr_config_t my_uart_intr_config = {
        .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT,
        .rx_timeout_thresh = 10,
        .rxfifo_full_thresh = 64,
        .txfifo_empty_intr_thresh = 0,

    };
    ESP_ERROR_CHECK( uart_intr_config(UART_NUM_0, &my_uart_intr_config) );
    /* 使能rx环形缓冲区中断 */
    ESP_ERROR_CHECK( uart_enable_rx_intr(UART_NUM_0) );

    /*四、创建FreeRTOS task，封装task任务函数，最后传参到xTaskCreate()*/
    xTaskCreate(led_green_2, "led_green_2", 1024, NULL, 0, NULL);
    xTaskCreate(led_red_4, "led_red_4", 2048, NULL, 0, NULL);
}
