// Hello, ESP32!

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

// ADC 通道配置
// GPIO1 -> ADC1_CH0
// GPIO2 -> ADC1_CH1
#define JOYSTICK_VRX_CHANNEL   ADC_CHANNEL_0   // GPIO1
#define JOYSTICK_VRY_CHANNEL   ADC_CHANNEL_1   // GPIO2

// SW 按键引脚
#define JOYSTICK_SW_GPIO       GPIO_NUM_39

void app_main(void)
{
    // --- 配置 SW 按键 GPIO ---
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << JOYSTICK_SW_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // --- 初始化 ADC Oneshot Unit ---
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&init_config, &adc1_handle);

    // --- 配置 VRX 通道 (GPIO1, ADC1_CH0) ---
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,        // 量程约 0~3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT, // 默认 12-bit (0~4095)
    };
    adc_oneshot_config_channel(adc1_handle, JOYSTICK_VRX_CHANNEL, &chan_cfg);

    // --- 配置 VRY 通道 (GPIO2, ADC1_CH1) ---
    adc_oneshot_config_channel(adc1_handle, JOYSTICK_VRY_CHANNEL, &chan_cfg);

    printf("HW-504 Joystick Test Started\n");
    printf("VRX -> GPIO1 (ADC1_CH0), VRY -> GPIO2 (ADC1_CH1), SW -> GPIO39\n");

    while (1) {
        int vrx_raw = 0;
        int vry_raw = 0;

        // 读取 X 轴 ADC 值
        adc_oneshot_read(adc1_handle, JOYSTICK_VRX_CHANNEL, &vrx_raw);

        // 读取 Y 轴 ADC 值
        adc_oneshot_read(adc1_handle, JOYSTICK_VRY_CHANNEL, &vry_raw);

        // 读取 SW 按键状态 (低电平 = 按下)
        int sw_state = gpio_get_level(JOYSTICK_SW_GPIO);

        printf("VRX (raw): %4d | VRY (raw): %4d | SW: %s\n",
               vrx_raw,
               vry_raw,
               (sw_state == 0) ? "PRESSED" : "RELEASED");

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 释放资源（实际上不会执行到这里）
    adc_oneshot_del_unit(adc1_handle);
}