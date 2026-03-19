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
