#include <stdio.h>
#include "freertos/FreeRTOS.h"      // 引入 FreeRTOS 头文件，提供 FreeRTOS 相关函数和类型定义
#include "freertos/task.h"          // 引入 FreeRTOS 任务头文件，提供任务创建、删除、挂起、恢复等函数
#include "freertos/event_groups.h"  // 引入 FreeRTOS 事件组头文件，提供事件组相关函数和类型定义
#include "esp_system.h"             // 引入 ESP 系统头文件，提供系统相关函数和类型定义
#include "esp_wifi.h"               // 引入 ESP Wi-Fi 头文件，提供 Wi-Fi 相关函数和类型定义
#include "esp_event.h"              // 引入 ESP-IDF 的事件组头文件 提供 esp_event_loop_create_default、esp_event_handler_register 等函数
#include "esp_log.h"                // 引入 ESP-IDF 日志头文件 提供 ESP_LOGI、ESP_LOGE 等函数
#include "nvs_flash.h"              // 引入 NVS 存储头文件 提供 NVS 相关函数和类型定义
#include "lwip/err.h"               // 引入 lwip 错误头文件 提供 lwip_strerror 函数
#include "lwip/sys.h"               // 引入 lwip 系统头文件 提供 sys_msleep 等函数
#include "esp_https_server.h"       // 引入 ESP-IDF 的 HTTP 服务器头文件 提供 httpd_start、httpd_register_uri_handler、httpd_resp_send 等函数
#include "driver/gpio.h"   // 引入 GPIO 驱动头文件 提供 gpio_set_direction、gpio_set_level 等函数


/* ===== 引脚宏定义 ===== */
#define led_pin GPIO_NUM_38                     // LED 引脚
/* ===== wi-fi 宏定义 ===== */
#define WIFI_SSID           "M"                 // 你家 WiFi 名称
#define WIFI_PASS           "QWERasdf1234...."  // 你家 WiFi 密码
#define WIFI_MAX_RETRY      5                   // 最大重试次数

#define WIFI_CONNECTED_BIT  BIT0                // 连接成功标志位
#define WIFI_FAIL_BIT       BIT1                // 连接失败标志位

/* ===== 函数声明 ===== */
/* ===== 全局变量 ===== */
static EventGroupHandle_t s_wifi_event_group;   // 事件组句柄
static int s_retry_num = 0;                     // 重试计数器
static const char *WiFi_Scan_TAG = "wifi_scan"; // Wi-Fi 扫描日志标签
static const char *WiFi_Station_TAG = "wifi_sta";            // Wi-Fi 连接日志标签
static const char *HTTP_TAG = "http_server";    // HTTP 服务器日志标签
// 定义日志标签字符串，用于 ESP_LOGI/ESP_LOGE 输出时标识来源
// 日志输出格式：I (时间) main: 你的日志内容



/* ===== 以下为函数定义部分 ===== */

/* ===== Wi-Fi 事件处理函数 ===== */
// 定义一个静态函数，用于处理 Wi-Fi 相关事件
// 这个函数会被 esp_event_handler_register 注册为 Wi-Fi 和 IP 事件的回调函数
// 参数说明：
// arg：用户自定义参数，这里没有用到，可以设为 NULL
// event_base：事件基（事件类型），例如 WIFI_EVENT 或 IP_EVENT
// event_id：具体事件 ID，例如 WIFI_EVENT_STA_START、WIFI_EVENT_STA_DISCONNECTED、IP_EVENT_STA_GOT_IP 等
// event_data：事件数据指针，指向一个结构体，包含事件相关的信息，例如 IP 地址等

static void event_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    // 情况1：Wi-Fi 驱动启动完成，立即发起连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(WiFi_Station_TAG, "Wi-Fi 已启动，正在尝试连接...");

    // 情况2：连接断开或失败，尝试重连
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(WiFi_Station_TAG, "连接失败，正在重试（第 %d 次）...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(WiFi_Station_TAG, "已达到最大重试次数，连接失败！");
        }

    // 情况3：成功获取 IP 地址，连接完全成功
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WiFi_Station_TAG, "连接成功！获取到 IP 地址：" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}



// Wi-Fi 扫描函数，扫描周围的 Wi-Fi 网络并打印信息
void wifi_scan(void)
{
    // 配置扫描参数：扫描所有信道、所有 AP，包括隐藏网络
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    // 阻塞扫描：等待扫描完成后再继续执行
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    // 获取扫描到的 AP 数量
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(WiFi_Scan_TAG, "共扫描到 %d 个 Wi-Fi 网络", ap_count);

    // 分配内存存放扫描结果
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_list == NULL) {
        ESP_LOGE(WiFi_Scan_TAG, "内存分配失败");
        return;
    }

    // 获取扫描结果列表
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

    // 打印表头
    ESP_LOGI(WiFi_Scan_TAG, "%-32s | %-20s | %s | %s",
             "SSID（网络名称）", "BSSID（MAC地址）", "信道", "信号强度(dBm)");
    ESP_LOGI(WiFi_Scan_TAG, "----------------------------------------------------------------");

    // 逐条打印每个 AP 的信息
    for (int i = 0; i < ap_count; i++) {
        ESP_LOGI(WiFi_Scan_TAG, "%-32s | %02x:%02x:%02x:%02x:%02x:%02x | %-4d | %d",
                 (char *)ap_list[i].ssid,
                 ap_list[i].bssid[0], ap_list[i].bssid[1], ap_list[i].bssid[2],
                 ap_list[i].bssid[3], ap_list[i].bssid[4], ap_list[i].bssid[5],
                 ap_list[i].primary,  // 信道号
                 ap_list[i].rssi);    // 信号强度（越接近 0 越强）
    }

    free(ap_list); // 释放内存，防止内存泄漏

    // 扫描完成后停止 Wi-Fi，为后续连接做准备
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(WiFi_Scan_TAG, "扫描完成，Wi-Fi 已停止，准备发起连接...");
}


/* ===== Wi-Fi Station 连接函数 ===== */
void wifi_connect_sta(void)
{
    // 第1步：创建事件组（用于等待连接结果）
    s_wifi_event_group = xEventGroupCreate();

    // 第2步：注册事件回调函数（必须在 esp_wifi_start() 之前注册！）
    esp_event_handler_instance_t instance_any_id;   // 任意事件
    esp_event_handler_instance_t instance_got_ip;   // 获取到 IP 地址'

    // 注册任意 Wi-Fi 事件的回调函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            &event_handler, NULL, &instance_any_id));
    
    
    // 注册任意 Wi-Fi 获取到 IP 地址的事件回调函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            &event_handler, NULL, &instance_got_ip));

    // 第3步：配置 Wi-Fi 连接参数（SSID、密码、加密方式）
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,                          // Wi-Fi 网络名称
            .password = WIFI_PASS,                      // Wi-Fi 密码
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 最低接受 WPA2 加密
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,         // WPA3 兼容配置
            .sae_h2e_identifier = "",
        },
    };

    // 第4步：设置 Station 模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 第5步：将连接配置写入 Wi-Fi 驱动
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 第6步：启动 Wi-Fi 驱动
    // 启动后会触发 WIFI_EVENT_STA_START 事件
    // 事件回调函数会自动调用 esp_wifi_connect() 发起连接
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WiFi_Station_TAG, "wifi_connect_sta 完成，等待连接结果...");

    // 第7步：阻塞等待，直到连接成功或失败
    EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,        // 不自动清除标志位
            pdFALSE,        // 任意一个标志位满足即返回
            portMAX_DELAY); // 永久等待

    // 第8步：根据结果输出日志
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WiFi_Station_TAG, "成功连接到 WiFi：%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(WiFi_Station_TAG, "连接 WiFi 失败：%s，已超过最大重试次数", WIFI_SSID);
    } else {
        ESP_LOGE(WiFi_Station_TAG, "未知错误");
    }
}



/* ===== LED GPIO初始化函数 ===== */
static void configure_led(void)
// 定义一个静态函数，用于初始化 LED 引脚
// static 表示这个函数只在当前文件内可见
{
    gpio_reset_pin(led_pin);
    // 将 38 号引脚复位到默认状态（清除之前的配置）
    // 防止之前的配置残留影响当前使用

    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);
    // 将 38 号引脚设置为"推挽输出"模式
    // GPIO_MODE_OUTPUT 表示该引脚用于输出高/低电平
    // 设置完成后，才能用 gpio_set_level 控制引脚电平
}


/* ===== LED 控制函数 ===== */
static void led_control(int level)
// 定义一个静态函数，用于控制 LED 的亮灭
// 参数 level：传入 1 → 输出高电平（LED 亮）；传入 0 → 输出低电平（LED 灭）
{
    gpio_set_level(led_pin, level);
    // 设置 38 号引脚的电平
    // level=1：引脚输出 3.3V（高电平），LED 点亮
    // level=0：引脚输出 0V（低电平），LED 熄灭
}


/* ===== LED 点亮的 HTTP 请求处理函数 ===== */
static esp_err_t led_on_handler(httpd_req_t *req)
// 这是一个 HTTP 请求处理函数（句柄/Handler）
// 当浏览器访问 /led/on 时，HTTP 服务器会自动调用这个函数
// 参数 req：包含本次 HTTP 请求的所有信息（请求头、请求体、连接信息等）
// 返回值 esp_err_t：返回 ESP_OK 表示处理成功，连接保持；返回其他值则关闭连接
{
    led_control(1);
    // 调用前面定义的 led_control 函数，传入 1
    // 效果：38 号引脚输出高电平，LED 点亮

    const char* resp_str = "{\"led\":\"on\"}";
    // 定义要返回给浏览器的响应内容（JSON 格式字符串）
    // \" 是转义字符，表示字符串里的双引号
    // 实际内容是：{"led":"on"}

    httpd_resp_set_type(req, "application/json");
    // 设置 HTTP 响应头的 Content-Type 为 application/json
    // 告诉浏览器：返回的内容是 JSON 格式

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    // 发送 HTTP 响应给浏览器
    // 参数1 req：当前请求对象
    // 参数2 resp_str：要发送的响应内容字符串
    // 参数3 HTTPD_RESP_USE_STRLEN：自动计算字符串长度（不需要手动传长度）

    return ESP_OK;
    // 返回成功，HTTP 连接正常关闭
}


/* ===== LED 熄灭的 HTTP 请求处理函数 ===== */
static esp_err_t led_off_handler(httpd_req_t *req)
// 与 led_on_handler 完全对称，处理 /led/off 的请求
{
    led_control(0);
    // 传入 0，38 号引脚输出低电平，LED 熄灭

    const char* resp_str = "{\"led\":\"off\"}";
    // 响应内容：{"led":"off"}

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


/* ===== HTTP URI 路由定义 ===== */
static const httpd_uri_t led_on_uri = {
// 定义一个 URI 路由结构体，描述"哪个地址、哪种方法、调用哪个函数"
    .uri     = "/led/on",
    // 路由路径：当访问 http://192.168.1.xxx/led/on 时触发
    .method  = HTTP_GET,
    // HTTP 方法：GET（浏览器直接输入地址访问就是 GET 请求）
    .handler = led_on_handler,
    // 处理函数：触发时调用 led_on_handler
    .user_ctx = NULL
    // 用户自定义上下文数据，这里不需要，填 NULL
};

static const httpd_uri_t led_off_uri = {
// 与 led_on_uri 对称，处理 /led/off 路由
    .uri     = "/led/off",
    .method  = HTTP_GET,
    .handler = led_off_handler,
    .user_ctx = NULL
};


/* ===== 启动 HTTP 服务器函数，注册并返回服务器句柄 ===== */
httpd_handle_t start_webserver(void)
// 定义启动 HTTP 服务器的函数
// 返回值 httpd_handle_t：服务器句柄，后续可用于注册路由或停止服务器
{
    httpd_handle_t server_handle = NULL;
    // 声明服务器句柄变量，初始为空

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    // 使用默认配置初始化服务器配置结构体
    // 默认端口是 80（HTTP 标准端口），任务优先级、栈大小等都有默认值

    if (httpd_start(&server_handle, &http_config) == ESP_OK) {
    // 启动 HTTP 服务器
    // httpd_start 会创建服务器任务，开始监听 TCP 连接
    // 如果启动成功，返回 ESP_OK，server 句柄被赋值

        ESP_LOGI(HTTP_TAG, "HTTP 服务器启动成功，注册 URI 处理器...");
        // 打印日志：服务器启动成功

        httpd_register_uri_handler(server_handle, &led_on_uri);
        // 向服务器注册 /led/on 路由
        // 之后访问 /led/on 就会调用 led_on_handler

        httpd_register_uri_handler(server_handle, &led_off_uri);
        // 向服务器注册 /led/off 路由

        return server_handle;
        // 返回服务器句柄
    }

    ESP_LOGE(HTTP_TAG, "HTTP 服务器启动失败");
    // 如果启动失败，打印错误日志

    return NULL;
    // 返回空句柄，表示启动失败
}



/* ===== 程序入口 ===== */
void app_main(void)
{   
    // 初始化 38 号引脚为输出模式
    // 必须在 led_control 之前调用，否则引脚方向未设置，无法控制电平
    configure_led();
    
    /* 第0步：初始化 NVS（只执行一次）*/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 第1步：初始化 TCP/IP 协议栈（只执行一次）*/
    ESP_ERROR_CHECK(esp_netif_init());

    /* 第2步：创建默认事件循环（只执行一次！不能重复调用）*/
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 第3步：创建默认 Station 网络接口（只执行一次）*/
    esp_netif_create_default_wifi_sta();

    /* 第4步：初始化 Wi-Fi 驱动（只执行一次）*/
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    /* 第5步：设置 Station 模式（只执行一次）*/
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* 第6步：启动 Wi-Fi 驱动，用于扫描（只执行一次）*/
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 第7步：执行扫描，扫描完成后内部会调用 esp_wifi_stop() */
    ESP_LOGI(WiFi_Scan_TAG, "开始扫描 Wi-Fi...");
    wifi_scan();

    /* 第8步：等待 1 秒，确保日志输出完整 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 第9步：发起 Station 连接
     * wifi_connect_sta() 内部会重新注册回调、配置参数、
     * 调用 esp_wifi_start()（此时 Wi-Fi 已被 wifi_scan 停止，可以重新 start）
     */
    ESP_LOGI(WiFi_Station_TAG, "开始连接 WiFi...");
    wifi_connect_sta();


    /* 第10步：启动 HTTP 服务器 */
    // 启动 HTTP 服务器，注册 /led/on 和 /led/off 路由
    // 调用后服务器开始在后台监听请求，不会阻塞 app_main 继续执行
    ESP_LOGI(HTTP_TAG, "启动 HTTP 服务器...");
    start_webserver();

}