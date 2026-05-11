#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// -------------------------------------------------------
// 宏定义
// -------------------------------------------------------
#define LED_GPIO            GPIO_NUM_38
#define TIMER_RESOLUTION_HZ (1 * 1000 * 1000)  // 1 MHz，1 次计数 = 1 微秒

// 各阶段的计数值（单位：微秒）
#define WAIT_5S_COUNT   (5 * 1000 * 1000)   // 等待 5 秒 = 5,000,000 计数
#define LED_ON_COUNT    (1 * 1000 * 1000)   // 亮灯 1 秒 = 1,000,000 计数
// 熄灯后等待 1 秒（题目要求熄灭 1 秒后再等 5 秒，此处熄灭即进入下一轮等待）

static const char *TAG = "GPTIMER_LED";

// -------------------------------------------------------
// 状态枚举：描述当前 LED 处于哪个阶段
// -------------------------------------------------------
typedef enum {
    STATE_WAITING,      // 等待 5 秒阶段（LED 熄灭，等待下次点亮）
    STATE_LED_ON,       // 亮灯 1 秒阶段
} led_state_t;

// 当前状态，初始为"等待 5 秒"
static volatile led_state_t current_state = STATE_WAITING;

// GPTimer 句柄
static gptimer_handle_t gptimer = NULL;

// -------------------------------------------------------
// GPTimer 警报回调函数
// 在警报事件发生时自动调用，运行在中断上下文中
// 注意：不能在此处调用阻塞式 API
// -------------------------------------------------------
static bool timer_alarm_callback(gptimer_handle_t timer,
                                  const gptimer_alarm_event_data_t *edata,
                                  void *user_ctx)
{
    gptimer_alarm_config_t new_alarm = {};

    if (current_state == STATE_WAITING) {
        // 当前处于"等待 5 秒"阶段，5 秒到了 → 点亮 LED，切换到亮灯阶段
        gpio_set_level(LED_GPIO, 1);
        ESP_EARLY_LOGI(TAG, "LED 点亮");

        current_state = STATE_LED_ON;

        // 设置下一次警报：再过 1 秒触发（亮灯 1 秒后熄灭）
        // alarm_count = 当前警报值 + 1 秒的计数
        new_alarm.alarm_count = edata->alarm_value + LED_ON_COUNT;
        new_alarm.flags.auto_reload_on_alarm = false;
        gptimer_set_alarm_action(timer, &new_alarm);

    } else if (current_state == STATE_LED_ON) {
        // 当前处于"亮灯 1 秒"阶段，1 秒到了 → 熄灭 LED，切换到等待阶段
        gpio_set_level(LED_GPIO, 0);
        ESP_EARLY_LOGI(TAG, "LED 熄灭");

        current_state = STATE_WAITING;

        // 设置下一次警报：再过 5 秒触发（等待 5 秒后再次点亮）
        new_alarm.alarm_count = edata->alarm_value + WAIT_5S_COUNT;
        new_alarm.flags.auto_reload_on_alarm = false;
        gptimer_set_alarm_action(timer, &new_alarm);
    }

    // 返回 false：不需要请求任务切换
    return false;
}

void app_main(void)
{
    // -------------------------------------------------------
    // 第一步：初始化 GPIO 38 为输出模式
    // -------------------------------------------------------
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);  // 初始状态：LED 熄灭

    // -------------------------------------------------------
    // 第二步：创建 GPTimer
    // gptimer_config_t：定时器配置结构体
    //   .clk_src：时钟源，选默认（通常为 APB 或 XTAL）
    //   .direction：计数方向，向上计数
    //   .resolution_hz：分辨率，1 MHz = 每计数 1 次代表 1 微秒
    // -------------------------------------------------------
    gptimer_config_t timer_config = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // -------------------------------------------------------
    // 第三步：注册警报回调函数
    // 当计数器到达警报值时，自动调用 timer_alarm_callback
    // -------------------------------------------------------
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_alarm_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    // -------------------------------------------------------
    // 第四步：配置第一次警报
    // 第一次：等待 5 秒后点亮 LED
    // alarm_count = 5,000,000（5 秒后触发）
    // auto_reload_on_alarm = false：不自动重载，由回调手动更新下一次警报值
    // -------------------------------------------------------
    gptimer_alarm_config_t alarm_config = {
        .alarm_count              = WAIT_5S_COUNT,
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    // -------------------------------------------------------
    // 第五步：使能并启动定时器
    // gptimer_enable()：使能定时器，申请系统资源（电源管理锁等）
    // gptimer_start()：启动计数器，开始计数
    // -------------------------------------------------------
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    // -------------------------------------------------------
    // app_main 直接返回，主任务自动删除
    // GPTimer 继续在后台运行，不受影响
    // -------------------------------------------------------
}
