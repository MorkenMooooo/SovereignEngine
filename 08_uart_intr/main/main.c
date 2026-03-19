#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "hal/uart_hal.h"
#include "esp_log.h"

QueueHandle_t my_uart_intr_queue_handle;

void led_2_green(void * pvParameters){
    for (;;){
        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(pdMS_TO_TICKS(100));

        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void led_4_red(void * pvParameters){
    uart_event_t received_uart_intr_event_sctruct;
    uint8_t received_uart_data[128];

    for(;;){
        if ( xQueueReceive(my_uart_intr_queue_handle, &received_uart_intr_event_sctruct, portMAX_DELAY) ){
            if ( received_uart_intr_event_sctruct.type == UART_DATA && received_uart_intr_event_sctruct.size > 0 ){
                uart_read_bytes(UART_NUM_0, &received_uart_data, received_uart_intr_event_sctruct.size, pdMS_TO_TICKS(50));
                ESP_LOGI("uart intr read bytes", "%s  %d", received_uart_data, received_uart_intr_event_sctruct.size);
                
                if (memcmp(received_uart_data, "led on", 6) == 0 ){
                    gpio_set_level(GPIO_NUM_4, 1);
                }

                if ( memcmp (received_uart_data, "led off", 7) == 0){
                    gpio_set_level(GPIO_NUM_4, 0);
                }

            }

        }
    }
}


void my_led_gpio_initialize(void){
    gpio_config_t my_led_gpio_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_2 | 1ULL << GPIO_NUM_4),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en =GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK( gpio_config(&my_led_gpio_config) );
    gpio_set_level(GPIO_NUM_4, 0);
}


void app_main(void)
{
    /*一、配置GPIO结构，再使能GPIO*/
    my_led_gpio_initialize();

    /*二、注册UART及启动中断，配置UART结构体parameters并使能，配置UART RX以及TX引脚*/
    uart_config_t my_uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK( uart_driver_install(UART_NUM_0, 1024, 0, 2, &my_uart_intr_queue_handle, 0) );
    ESP_ERROR_CHECK( uart_param_config(UART_NUM_0, &my_uart_config) );
    uart_set_pin(UART_NUM_0, GPIO_NUM_43, GPIO_NUM_44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /*三、显示配置UART中断参数并启用uart intr config() */
    uart_intr_config_t my_uart_intr_config = {
        .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT,
        .rx_timeout_thresh = 10,
        .rxfifo_full_thresh = 120,
        .txfifo_empty_intr_thresh = 0,
    };
    ESP_ERROR_CHECK( uart_intr_config(UART_NUM_0, &my_uart_intr_config) );
    /*四、使能rx环形缓冲区中断 */
    ESP_ERROR_CHECK( uart_enable_rx_intr(UART_NUM_0) );

    /*五、创建FreeRTOS task，封装task任务函数，最后传参到xTaskCreate()*/
    xTaskCreate(led_2_green, "led_2_green", 1024, NULL, 0, NULL);
    xTaskCreate(led_4_red, "led_4_red", 2048, NULL, 1, NULL);
}
