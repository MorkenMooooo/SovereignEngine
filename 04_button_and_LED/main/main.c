#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define button_pin_2 GPIO_NUM_2
#define led_pin_4    GPIO_NUM_4

void app_main(void)
{
    gpio_set_direction(button_pin_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(button_pin_2, GPIO_PULLUP_ONLY);

    gpio_set_direction(led_pin_4, GPIO_MODE_OUTPUT);


    while (1) 
    {

        uint32_t button_level = gpio_get_level(button_pin_2);

        if (button_level == 0) {
            gpio_set_level(led_pin_4, 1);
        } else {
            gpio_set_level(led_pin_4, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

}
