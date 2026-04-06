#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"  

// ================= 全局定义 =================
#define led_pin_2       GPIO_NUM_2
#define button_pin_20   GPIO_NUM_20

QueueHandle_t my_gpio_queue_handle;


void IRAM_ATTR my_gpio_button_isr(void* arg){
    uint32_t intr_gpio_num = (uint32_t) arg;
    xQueueSendFromISR(my_gpio_queue_handle, &intr_gpio_num, NULL);
}


void my_led_button_task(void* arg){
    uint32_t receive_data;
    for(;;){
        if (xQueueReceive(my_gpio_queue_handle, &receive_data, portMAX_DELAY)){
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGI("gpio interrupt", "gpio interrupt triggered! data is %d", receive_data);
            if ( gpio_get_level(button_pin_20) == 0){
                int led_state = gpio_get_level(led_pin_2);

                // LED灯电平翻转
                gpio_set_level(led_pin_2, !led_state);

            }
            
        }
    }
}


void app_main(void)
{

    // 1. 配置 LED 引脚（输出）
    gpio_reset_pin(led_pin_2);
    gpio_set_direction(led_pin_2, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(led_pin_2, 0);
    // 2. 配置按钮引脚（输入 + 上拉 + 下降沿中断）
    gpio_config_t my_button_gpio_config = {
        .pin_bit_mask = 1ULL << button_pin_20,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&my_button_gpio_config);

    // 3. 创建队列
    my_gpio_queue_handle = xQueueCreate(5, sizeof(uint32_t));

    // 6. 创建任务
    xTaskCreate(my_led_button_task, "gpio_intr", 2048, NULL, 0, NULL);

    // 4. 安装 ISR 服务
    gpio_install_isr_service(0);

    // 5. 注册 ISR 到按钮引脚
    gpio_isr_handler_add(button_pin_20, my_gpio_button_isr, (void *) button_pin_20);

    


}
