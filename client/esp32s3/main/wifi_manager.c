/*
 * WiFi 管理模块实现
 */

#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WiFi";
static EventGroupHandle_t s_wifi_event_group;

volatile bool wifi_connected = false;
volatile bool wifi_need_reconnect = false;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        wifi_need_reconnect = true;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi连接成功! IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_reconnect_task(void *arg)
{
    while (1) {
        if (wifi_need_reconnect) {
            wifi_need_reconnect = false;
            ESP_LOGI(TAG, "WiFi断开连接，%d秒后重试...", WIFI_RECONNECT_DELAY_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
            ESP_LOGI(TAG, "正在重连WiFi...");
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void wifi_manager_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    // 创建WiFi重连任务
    BaseType_t task_created = xTaskCreate(
        wifi_reconnect_task,
        "wifi_reconnect",
        WIFI_RECONNECT_TASK_STACK_SIZE,
        NULL,
        WIFI_RECONNECT_TASK_PRIORITY,
        NULL
    );
    assert(task_created == pdTRUE);
}
