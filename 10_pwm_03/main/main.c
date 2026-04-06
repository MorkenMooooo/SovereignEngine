#include <stdio.h>
#include "freertos/FreeRTOS.h"   // FreeRTOS 实时操作系统核心头文件
#include "freertos/task.h"       // FreeRTOS 任务管理（创建任务、延时等）
#include "driver/gpio.h"         // GPIO 驱动（配置引脚输入输出、读取电平）
#include "driver/ledc.h"         // LEDC 驱动（LED PWM 控制器，用于生成 PWM 信号）
#include "esp_err.h"             // ESP-IDF 错误码处理
#include "esp_log.h"             // ESP-IDF 日志打印

// ===== 宏定义：硬件引脚和 LEDC 参数 =====
#define BUTTON_GPIO         20           // 按键连接的 GPIO 引脚编号
#define LED_GPIO            2            // LED 连接的 GPIO 引脚编号
#define LEDC_TIMER          LEDC_TIMER_0 // 使用 LEDC 定时器 0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE // 使用低速模式（大多数 ESP32 芯片支持）
#define LEDC_CHANNEL        LEDC_CHANNEL_0      // 使用 LEDC 通道 0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT   // PWM 分辨率：13 位（占空比范围 0~8191）
#define LEDC_FREQUENCY      4000                // PWM 频率：4000 Hz

static const char *TAG = "PWM_Breath"; // 日志标签，打印日志时会显示此名称

// 全局变量：当前 LED 模式（volatile 保证多任务访问时不被编译器优化掉）
// 0 = 关闭，1 = 呼吸灯，2 = 闪烁
static volatile int led_mode = 0;

// ===== LEDC 初始化函数 =====
static void ledc_init(void)
{
    // 配置 LEDC 定时器：决定 PWM 的频率和分辨率
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,       // 低速模式
        .timer_num        = LEDC_TIMER,      // 使用定时器 0
        .duty_resolution  = LEDC_DUTY_RES,   // 13 位分辨率
        .freq_hz          = LEDC_FREQUENCY,  // 频率 4000 Hz
        .clk_cfg          = LEDC_AUTO_CLK,   // 自动选择时钟源
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer)); // 应用定时器配置，出错则打印并停止

    // 配置 LEDC 通道：将 PWM 信号绑定到具体的 GPIO 引脚
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,         // 低速模式
        .channel        = LEDC_CHANNEL,      // 通道 0
        .timer_sel      = LEDC_TIMER,        // 绑定定时器 0
        .intr_type      = LEDC_INTR_DISABLE, // 不使用中断
        .gpio_num       = LED_GPIO,          // 输出到 LED 引脚（GPIO2）
        .duty           = 0,                 // 初始占空比为 0（LED 熄灭）
        .hpoint         = 0,                 // 高电平起始点为 0（无相位偏移）
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel)); // 应用通道配置
}

// ===== 按键检测任务 =====
// 功能：短按切换模式（1→2→1 循环），长按超过 1 秒松开后关闭 LED
static void button_task(void *arg)
{
    int last_level = 1;       // 上一次按键电平（1=未按下，因为启用了上拉电阻）
    TickType_t press_start = 0; // 记录按键按下的时刻（FreeRTOS 系统 tick）

    while (1) {
        int level = gpio_get_level(BUTTON_GPIO); // 读取当前按键电平

        // 检测下降沿：上次是高电平，现在是低电平 → 按键刚被按下
        if (last_level == 1 && level == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 延时 20ms 去抖动（消除机械抖动干扰）
            if (gpio_get_level(BUTTON_GPIO) == 0) { // 再次确认确实是按下状态
                press_start = xTaskGetTickCount(); // 记录按下时刻（单位：tick）
                ESP_LOGD(TAG, "Button pressed");   // 调试日志
            }
        }

        // 检测上升沿：上次是低电平，现在是高电平 → 按键刚被松开
        if (last_level == 0 && level == 1) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 延时 20ms 去抖动
            if (gpio_get_level(BUTTON_GPIO) == 1) { // 再次确认确实是松开状态
                // 计算按键持续时间（tick 差值 × 每 tick 毫秒数）
                TickType_t held_time = (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS;
                if (held_time >= 1000) {
                    // 长按（超过 1 秒）：关闭 LED
                    led_mode = 0;
                    ESP_LOGI(TAG, "Long press released (>1s), LED off");
                } else {
                    // 短按：在模式 1 和 2 之间切换（1→2→1→...）
                    led_mode = (led_mode % 2) + 1;
                    ESP_LOGI(TAG, "Short press, mode -> %d", led_mode);
                }
            }
        }

        last_level = level;              // 更新上一次电平状态
        vTaskDelay(pdMS_TO_TICKS(10));   // 每 10ms 检测一次按键，释放 CPU 给其他任务
    }
}

// ===== LED 控制任务 =====
static void led_task(void *arg)
{
    ESP_ERROR_CHECK(ledc_fade_func_install(0)); // 安装 LEDC 渐变功能（用于呼吸灯效果）

    int current_mode = 0; // 记录当前正在执行的模式，用于检测模式切换

    while (1) {
        int mode = led_mode; // 读取全局模式变量

        if (mode == 1) {
            // ===== 呼吸灯模式 =====
            if (current_mode != 1) {
                current_mode = 1;
                ESP_LOGI(TAG, "Breathing mode"); // 首次进入时打印日志
            }

            // 渐亮：在 1000ms 内将占空比从当前值渐变到 8191（最亮）
            ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 8191, 1000);
            ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT); // 非阻塞启动渐变

            // 等待 1000ms，每 10ms 检查一次模式是否被切换
            for (int i = 0; i < 100; i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
                if (led_mode != 1) goto mode_changed; // 模式变了，跳转到清理逻辑
            }

            // 渐灭：在 1000ms 内将占空比从当前值渐变到 0（最暗）
            ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, 1000);
            ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);

            for (int i = 0; i < 100; i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
                if (led_mode != 1) goto mode_changed;
            }

        } else if (mode == 2) {
            // ===== 闪烁模式 =====
            if (current_mode != 2) {
                current_mode = 2;
                ESP_LOGI(TAG, "Blink mode");
            }

            // 点亮 LED：设置占空比为 8191（最大亮度）并立即更新
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 8191);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(100)); // 亮 100ms
            if (led_mode != 2) goto mode_changed; // 检查模式是否切换

            // 熄灭 LED：设置占空比为 0
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(100)); // 灭 100ms

        } else {
            // ===== 关闭模式（mode == 0）=====
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0); // 占空比设为 0
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL); // 立即更新
            vTaskDelay(pdMS_TO_TICKS(50));              // 等待 50ms 再检查
        }
        continue; // 继续下一次循环

mode_changed:
        // 模式切换时的清理：先关闭 LED，再重置当前模式标记
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        current_mode = 0; // 重置，下次进入
    }
}

// ===== 程序入口函数 =====
void app_main(void)
{
    // 配置按键 GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO), // 选择 GPIO20 作为按键引脚
        .mode         = GPIO_MODE_INPUT,        // 设置为输入模式
        .pull_up_en   = GPIO_PULLUP_ENABLE,     // 启用内部上拉电阻（未按下时为高电平）
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 不启用下拉
        .intr_type    = GPIO_INTR_DISABLE,      // 不使用中断（用轮询方式检测按键）
    };
    gpio_config(&io_conf); // 应用 GPIO 配置

    ledc_init(); // 初始化 LEDC（PWM 控制器）

    ESP_LOGI(TAG, "Program started. Press button on GPIO %d to switch mode.", BUTTON_GPIO);
    // 打印启动日志，提示用户按下按键切换模式

    // 创建按键检测任务
    // 参数依次：任务函数、任务名称、栈大小(字节)、参数、优先级、任务句柄
    xTaskCreate(button_task, "button_task", 2048, NULL, 6, NULL);
    // 优先级 6，高于 led_task，确保按键响应及时

    // 创建 LED 控制任务
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
    // 优先级 5，栈稍大（4096字节），因为呼吸灯逻辑更复杂
}
