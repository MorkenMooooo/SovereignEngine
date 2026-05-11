#include "esp_timer.h"      // esp_timer 相关 API
#include "esp_log.h"        // ESP_LOGI 日志打印
#include "driver/gpio.h"    // GPIO 控制

#define LED_GPIO            GPIO_NUM_38         // LED 连接的引脚号
#define BLINK_INTERVAL_US   (5 * 1000000)       // 5 秒，单位：微秒
#define LED_ON_DURATION_US  (1 * 1000000)       // 亮 1 秒，单位：微秒

static const char *TAG = "ESP_TIMER_LED";

// -------------------------------------------------------
// 定时器句柄：相当于定时器的"身份证"，后续操作都靠它
// -------------------------------------------------------
static esp_timer_handle_t led_on_timer_handle  = NULL; // 触发闪烁的定时器
static esp_timer_handle_t led_off_timer_handle = NULL; // 熄灭 LED 的定时器



// -------------------------------------------------------
// 回调函数 A：点亮 LED，然后启动 1 秒后熄灭的定时器
// 这个函数在"触发定时器"到期时自动被调用
// -------------------------------------------------------
static void led_on_callback(void *arg)
{
    gpio_set_level(LED_GPIO, 1);  // 点亮 LED（输出高电平）
    ESP_LOGI(TAG, "LED ON");

    // 启动熄灭定时器，1 秒后熄灭 LED
    ESP_ERROR_CHECK(esp_timer_start_once(led_off_timer_handle, LED_ON_DURATION_US));
}



// -------------------------------------------------------
// 回调函数 B：熄灭 LED，然后重新启动 5 秒触发定时器
// 这个函数在"熄灭定时器"到期时自动被调用
// -------------------------------------------------------
static void led_off_callback(void *arg)
{
    gpio_set_level(LED_GPIO, 0);  // 熄灭 LED（输出低电平）
    ESP_LOGI(TAG, "LED OFF");

    // 重新启动触发定时器，等待下一个 5 秒
    // esp_timer_start_once：一次性定时器，到期后只触发一次
    ESP_ERROR_CHECK(esp_timer_start_once(led_on_timer_handle, BLINK_INTERVAL_US));
}



void app_main(void)
{
    // -------------------------------------------------------
    // 第一步：初始化 GPIO 38 为输出模式
    // -------------------------------------------------------
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),  // 选择引脚 38
        .mode         = GPIO_MODE_OUTPUT,    // 设置为输出模式
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,   // 不使用 GPIO 中断
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);  // 初始状态：LED 熄灭

    // -------------------------------------------------------
    // 第二步：创建"点亮 LED"定时器
    // -------------------------------------------------------
    const esp_timer_create_args_t led_on_timer_args = {
        .callback = &led_on_callback,  // 到期时调用 led_on_callback
        .name     = "led_on_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_on_timer_args, &led_on_timer_handle));

    // -------------------------------------------------------
    // 第三步：创建"熄灭 LED"定时器
    // esp_timer_create_args_t：定时器配置参数结构体
    //   .callback：定时器到期时调用哪个函数
    //   .name：定时器名字（调试用，随便起）
    // -------------------------------------------------------
    const esp_timer_create_args_t led_off_timer_args = {
        .callback = &led_off_callback,  // 到期时调用 led_off_callback
        .name     = "led_off_timer",
    };
    // esp_timer_create：根据配置参数创建定时器，把"身份证"存入 led_off_timer_handle
    ESP_ERROR_CHECK(esp_timer_create(&led_off_timer_args, &led_off_timer_handle));


    // -------------------------------------------------------
    // 第四步：启动触发定时器，5 秒后第一次触发
    // esp_timer_start_once：一次性定时器，到期后自动停止
    // -------------------------------------------------------
    ESP_ERROR_CHECK(esp_timer_start_once(led_on_timer_handle, BLINK_INTERVAL_US));
    ESP_LOGI(TAG, "Start LED timer");

    // -------------------------------------------------------
    // app_main 直接返回，主任务自动删除
    // 定时器会继续在后台运行，不受影响
    // -------------------------------------------------------
}