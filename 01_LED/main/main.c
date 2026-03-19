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
