#include "esp_timer.h"              // esp_timer 相关 API
#include "esp_log.h"                // 日志打印
#include "driver/gpio.h"            // GPIO 控制
#include "freertos/FreeRTOS.h"      // FreeRTOS 基础
#include "freertos/task.h"          // xTaskCreate 等任务 API
#include "freertos/semphr.h"        // 信号量 API

// -------------------------------------------------------
// 宏定义：引脚号和时间间隔
// -------------------------------------------------------
#define LED_GPIO              GPIO_NUM_38
#define INTERVAL_US   (5 * 1000000)   // 每 5 秒触发一次亮灯，单位：微秒
#define LED_ON_DURATION_US    (1 * 1000000)   // 亮灯持续 1 秒，单位：微秒

static const char *TAG = "LED_TIMER_TASK";

// -------------------------------------------------------
// 全局变量
// -------------------------------------------------------

// 二值信号量：只有"有信号（1）"和"无信号（0）"两种状态
// 定时器回调通过它通知 Task："该亮灯了"
static SemaphoreHandle_t  led_sem_handle      = NULL;

// 亮灯触发定时器的句柄：每 5 秒触发一次，通知 Task 亮灯
static esp_timer_handle_t led_on_timer_handle    = NULL;

// 熄灯定时器的句柄：亮灯 1 秒后触发，将 LED 熄灭
static esp_timer_handle_t led_off_timer_handle   = NULL;

// -------------------------------------------------------
// 回调函数 A：熄灯
// 当"熄灯定时器"到期（亮灯 1 秒后），自动调用此函数
// 执行位置：esp_timer 内部任务（非中断上下文）
// -------------------------------------------------------
static void led_off_callback(void *arg)
{
    gpio_set_level(LED_GPIO, 0);    // 将 GPIO 38 输出低电平 → LED 熄灭
    ESP_LOGI(TAG, "LED 熄灭");
}

// -------------------------------------------------------
// 回调函数 B：释放信号量，通知 Task 执行亮灯操作
// 当"亮灯触发定时器"每 5 秒到期时，自动调用此函数
// 注意：此处只释放信号量，不直接操作 GPIO
//       实际亮灯操作在 Task 中完成，符合 ESP-IDF 最佳实践
// -------------------------------------------------------
static void led_sem_callback(void *arg)
{
    // xSemaphoreGiveFromISR：在回调/中断上下文中释放信号量
    // 第二个参数 NULL：不需要请求任务切换
    xSemaphoreGiveFromISR(led_sem_handle, NULL);
}

// -------------------------------------------------------
// LED 控制 Task：专门负责等待信号量并执行亮灯操作
// 收到信号量 → 点亮 LED → 启动熄灯定时器（1 秒后熄灭）
// 使用 for(;;) 无限循环，符合 FreeRTOS Task 的标准写法
// -------------------------------------------------------
static void led_control_task(void *pvParameters)
{
    for (;;) {
        // xSemaphoreTake：等待信号量
        // portMAX_DELAY：永久等待，直到信号量到来为止
        // 在等待期间，此 Task 处于阻塞状态，不占用 CPU
        if (xSemaphoreTake(led_sem_handle, portMAX_DELAY) == pdTRUE) {

            // 收到信号量，说明 5 秒定时器已到期，执行亮灯
            gpio_set_level(LED_GPIO, 1);    // 将 GPIO 38 输出高电平 → LED 点亮
            ESP_LOGI(TAG, "LED 点亮");

            // 启动熄灯定时器：1 秒后自动调用 led_off_callback 熄灭 LED
            // esp_timer_start_once：一次性定时器，到期后只触发一次，不自动重复
            // 注意：若上一次熄灯定时器还未到期，需先调用 esp_timer_stop() 再启动
            esp_timer_stop(led_off_timer_handle);  // 确保熄灯定时器未在运行
            ESP_ERROR_CHECK(esp_timer_start_once(led_off_timer_handle, LED_ON_DURATION_US));
        }
    }
}

// -------------------------------------------------------
// app_main：程序入口，完成所有初始化后直接返回
// 返回后主任务自动删除，其他 Task 和定时器继续运行
// -------------------------------------------------------
void app_main(void)
{
    // -------------------------------------------------------
    // 第一步：初始化 GPIO 38 为输出模式
    // gpio_config_t：GPIO 配置结构体
    // -------------------------------------------------------
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),  // 选择引脚 38（位掩码形式）
        .mode         = GPIO_MODE_OUTPUT,    // 设置为输出模式
        .pull_up_en   = GPIO_PULLUP_DISABLE, // 不启用内部上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,   // 不使用 GPIO 中断
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);  // 初始状态：LED 熄灭

    // -------------------------------------------------------
    // 第二步：创建二值信号量
    // 二值信号量只有两种状态：有信号（1）和无信号（0）
    // 定时器回调"给出"信号量，Task"取走"信号量
    // -------------------------------------------------------
    led_sem_handle = xSemaphoreCreateBinary();



    // -------------------------------------------------------
    // 第三步：创建"sem 定时器"（周期性）
    // -------------------------------------------------------
    const esp_timer_create_args_t led_on_timer_args = {
        .callback = &led_sem_callback,  // 到期时释放信号量
        .name     = "led_sem_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_on_timer_args, &led_on_timer_handle));


    // -------------------------------------------------------
    // 第四步：创建"熄灯定时器"
    // esp_timer_create_args_t：定时器配置参数
    //   .callback：定时器到期时调用哪个函数
    //   .name：定时器名字（仅用于调试，随便起）
    // esp_timer_create：根据配置创建定时器，句柄存入 led_off_timer
    // -------------------------------------------------------
    const esp_timer_create_args_t led_off_timer_args = {
        .callback = &led_off_callback,  // 到期时调用熄灯回调
        .name     = "led_off_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_off_timer_args, &led_off_timer_handle));


    // esp_timer_start_periodic：启动周期性定时器
    // 每隔 INTERVAL_US（5 秒）自动触发一次，无需手动重启
    ESP_ERROR_CHECK(esp_timer_start_periodic(led_on_timer_handle, INTERVAL_US));


    // -------------------------------------------------------
    // 第五步：创建 LED 控制 Task
    // xTaskCreate 参数说明：
    //   led_control_task：Task 函数
    //   "led_ctrl_task"：Task 名字（调试用）
    //   2048：栈大小（字节）
    //   NULL：传给 Task 的参数（不需要）
    //   5：Task 优先级（数字越大优先级越高）
    //   NULL：Task 句柄（不需要保存）
    // -------------------------------------------------------
    xTaskCreate(led_control_task, "led_ctrl_task", 2048, NULL, 5, NULL);

    // -------------------------------------------------------
    // app_main 直接返回
    // 主任务自动删除，led_control_task 和两个定时器继续运行
    // -------------------------------------------------------
}