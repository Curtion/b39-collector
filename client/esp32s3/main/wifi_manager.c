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
#include "esp_smartconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WiFi";
static EventGroupHandle_t s_wifi_event_group;

// Event bits
#define CONNECTED_BIT BIT0
#define DISCONNECTED_BIT BIT1
#define SMARTCONFIG_DONE_BIT BIT2

volatile bool wifi_connected = false;
volatile bool wifi_need_reconnect = false;
volatile bool smartconfig_done = false;
volatile bool smartconfig_active = false;

// NVS 命名空间
#define WIFI_NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"

// 从 NVS 加载 WiFi 配置
static esp_err_t load_wifi_config_from_nvs(wifi_config_t *wifi_config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t ssid_len = sizeof(wifi_config->sta.ssid);
    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, (char *)wifi_config->sta.ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    size_t password_len = sizeof(wifi_config->sta.password);
    err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, (char *)wifi_config->sta.password, &password_len);
    
    nvs_close(nvs_handle);
    return err;
}

// 保存 WiFi 配置到 NVS
static esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi 配置已保存到 NVS");
    }
    return err;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi STA 启动");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        wifi_need_reconnect = true;
        xEventGroupSetBits(s_wifi_event_group, DISCONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi 断开连接");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_connected = true;
        wifi_need_reconnect = false;
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, DISCONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi 连接成功! IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(TAG, "SmartConfig 扫描完成");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(TAG, "SmartConfig 找到信道");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(TAG, "SmartConfig 获取到 SSID 和密码");
        
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        char ssid[33] = {0};
        char password[65] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        
        ESP_LOGI(TAG, "SSID: %s", ssid);
        ESP_LOGI(TAG, "PASSWORD: %s", password);

        // 保存配置到 NVS
        save_wifi_config_to_nvs(ssid, password);

        // 断开当前连接，应用新配置
        esp_wifi_disconnect();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        ESP_LOGI(TAG, "SmartConfig 发送确认完成");
        xEventGroupSetBits(s_wifi_event_group, SMARTCONFIG_DONE_BIT);
    }
}

void wifi_reconnect_task(void *arg)
{
    while (1) {
        if (wifi_need_reconnect && !smartconfig_active) {
            wifi_need_reconnect = false;
            ESP_LOGI(TAG, "WiFi 断开连接，%d 秒后重试...", WIFI_RECONNECT_DELAY_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
            ESP_LOGI(TAG, "正在重连 WiFi...");
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void smartconfig_task(void *arg)
{
    ESP_LOGI(TAG, "启动 SmartConfig 配网任务");
    ESP_LOGI(TAG, "请使用 EspTouch 或其他配网 APP 进行配网");
    
    smartconfig_active = true;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SMARTCONFIG_TYPE));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    
    EventBits_t uxBits;
    bool timeout = false;
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(SMARTCONFIG_TIMEOUT_MS);
    
    while (!smartconfig_done && !timeout) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, 
                                     CONNECTED_BIT | SMARTCONFIG_DONE_BIT, 
                                     true, false, pdMS_TO_TICKS(1000));
        
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi 已连接到 AP");
        }
        
        if (uxBits & SMARTCONFIG_DONE_BIT) {
            ESP_LOGI(TAG, "SmartConfig 配网完成");
            smartconfig_done = true;
        }
        
        // 检查超时（使用 tick 差值避免溢出问题）
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            timeout = true;
            ESP_LOGW(TAG, "SmartConfig 配网超时");
        }
    }
    
    smartconfig_active = false;
    esp_smartconfig_stop();
    
    if (smartconfig_done) {
        ESP_LOGI(TAG, "SmartConfig 任务结束，配网成功");
    } else {
        ESP_LOGW(TAG, "SmartConfig 任务结束，配网失败或超时");
        // 配网失败，尝试连接之前保存的配置
        wifi_config_t wifi_config;
        if (load_wifi_config_from_nvs(&wifi_config) == ESP_OK) {
            ESP_LOGI(TAG, "尝试使用已保存的配置连接...");
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
        }
    }
    
    vTaskDelete(NULL);
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
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 尝试从 NVS 加载配置
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    
    esp_err_t err = load_wifi_config_from_nvs(&wifi_config);
    
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 检查是否有有效的 WiFi 配置
    if (err != ESP_OK) {
        // 没有配置，启动 SmartConfig
        ESP_LOGI(TAG, "NVS 中没有 WiFi 配置，启动 SmartConfig 配网");
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
    } else {
        ESP_LOGI(TAG, "从 NVS 加载 WiFi 配置成功，SSID: %s", wifi_config.sta.ssid);
        // 尝试连接
        esp_wifi_connect();
    }

    // 创建 WiFi 重连任务
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

esp_err_t wifi_manager_reset_config(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(nvs_handle);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi 配置已重置");
    }
    
    return err;
}

esp_err_t wifi_manager_get_ssid(char *ssid)
{
    if (!wifi_connected) {
        return ESP_FAIL;
    }
    
    wifi_config_t wifi_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (err == ESP_OK) {
        memcpy(ssid, wifi_config.sta.ssid, 32);
        ssid[32] = '\0';
    }
    return err;
}
