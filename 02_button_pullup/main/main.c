#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define button_pin_2 GPIO_NUM_2

void app_main(void)
{   
    gpio_set_direction(button_pin_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(button_pin_2, GPIO_PULLUP_ONLY);
    

    while(1)
    {   
        int button_get_level = gpio_get_level(button_pin_2);
        printf("%d\n", button_get_level );
        vTaskDelay(pdMS_TO_TICKS(1000) );
    }
}
