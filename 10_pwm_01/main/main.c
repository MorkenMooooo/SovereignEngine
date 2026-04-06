#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"


#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_20


void ledc_init()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    }; 
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);
    

}


void led_task(void *arg)
{
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    
    for(;;){
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(30));
            if (gpio_get_level(BUTTON_PIN) == 0) { 
                ledc_set_fade_with_step(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8191, 30, LEDC_FADE_NO_WAIT);
            }
    }
}




void app_main(void)
{
    // 1. configure button pin as input with pull-up
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_config);
    
    ledc_init();

    ESP_LOGI("LEDC", "Press the button to increase brightness, release to decrease brightness");

    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}
