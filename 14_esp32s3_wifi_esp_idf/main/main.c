#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* ===== 宏定义 ===== */
#define WIFI_SSID           "M"                 // 你家 WiFi 名称
#define WIFI_PASS           "QWERasdf1234...."  // 你家 WiFi 密码
#define WIFI_MAX_RETRY      5                   // 最大重试次数

#define WIFI_CONNECTED_BIT  BIT0                // 连接成功标志位
#define WIFI_FAIL_BIT       BIT1                // 连接失败标志位

/* ===== 全局变量 ===== */
static EventGroupHandle_t s_wifi_event_group;   // 事件组句柄
static int s_retry_num = 0;                     // 重试计数器
static const char *TAG = "wifi_sta";
static const char *WiFi_Scan_TAG = "wifi_scan";

/* ===== 事件回调函数 ===== */
static void event_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    // 情况1：Wi-Fi 驱动启动完成，立即发起连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Wi-Fi 已启动，正在尝试连接...");

    // 情况2：连接断开或失败，尝试重连
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "连接失败，正在重试（第 %d 次）...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "已达到最大重试次数，连接失败！");
        }

    // 情况3：成功获取 IP 地址，连接完全成功
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "连接成功！获取到 IP 地址：" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ===== Wi-Fi 扫描函数 ===== */
/* 注意：调用前 Wi-Fi 必须已经 start，调用后会 stop，供后续连接重新 start */
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
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID,
            &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP,
            &event_handler, NULL, &instance_got_ip));

    // 第3步：配置 Wi-Fi 连接参数（SSID、密码、加密方式）
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
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

    ESP_LOGI(TAG, "wifi_connect_sta 完成，等待连接结果...");

    // 第7步：阻塞等待，直到连接成功或失败
    EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,        // 不自动清除标志位
            pdFALSE,        // 任意一个标志位满足即返回
            portMAX_DELAY); // 永久等待

    // 第8步：根据结果输出日志
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "成功连接到 WiFi：%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "连接 WiFi 失败：%s，已超过最大重试次数", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "未知错误");
    }
}

/* ===== 程序入口 ===== */
void app_main(void)
{
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
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 第5步：设置 Station 模式（只执行一次）*/
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* 第6步：启动 Wi-Fi 驱动，用于扫描（只执行一次）*/
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 第7步：执行扫描，扫描完成后内部会调用 esp_wifi_stop() */
    ESP_LOGI(TAG, "开始扫描 Wi-Fi...");
    wifi_scan();

    /* 第8步：等待 1 秒，确保日志输出完整 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* 第9步：发起 Station 连接
     * wifi_connect_sta() 内部会重新注册回调、配置参数、
     * 调用 esp_wifi_start()（此时 Wi-Fi 已被 wifi_scan 停止，可以重新 start）
     */
    ESP_LOGI(TAG, "开始连接 WiFi...");
    wifi_connect_sta();
}