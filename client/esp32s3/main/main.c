/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_wifi.h"

#include "esp_http_client.h"

#define EXAMPLE_USB_HOST_PRIORITY (10)
#define EXAMPLE_USB_DEVICE_VID (0x303A)
#define EXAMPLE_USB_DEVICE_PID (0x1001)
#define RX_BUFFER_SIZE (2048)

#define WIFI_SSID "SukeMeu"
#define WIFI_PASSWORD "123456789"
#define HTTP_URI "https://blog.3gxk.net"

// WiFi重连延迟（毫秒）
#define WIFI_RECONNECT_DELAY_MS 3000

// WiFi重连任务配置
#define WIFI_RECONNECT_TASK_PRIORITY 3
#define WIFI_RECONNECT_TASK_STACK_SIZE 4096

// HTTP任务配置
#define HTTP_QUEUE_SIZE 5
#define HTTP_TASK_PRIORITY 5
#define HTTP_TASK_STACK_SIZE 8192

static const char *TAG = "USB-CDC";
static SemaphoreHandle_t device_disconnected_sem;

// 接收缓冲区和相关变量
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static size_t rx_buffer_len = 0;

// WiFi 事件组句柄
static EventGroupHandle_t s_wifi_event_group;

// HTTP请求队列
static QueueHandle_t http_request_queue = NULL;

// WiFi连接状态标志
static volatile bool wifi_connected = false;
static volatile bool wifi_need_reconnect = false;

// HTTP请求数据结构
typedef struct {
    char data[RX_BUFFER_SIZE];
    size_t len;
} http_request_t;

/**
 * @brief HTTP请求任务 - 在独立任务中执行阻塞式HTTP请求
 */
static void http_request_task(void *arg)
{
    http_request_t req;
    char post_data[RX_BUFFER_SIZE + 100];

    while (1) {
        if (xQueueReceive(http_request_queue, &req, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "接收到数据: %.*s", req.len, req.data);
            ESP_LOGI(TAG, "数据已发送到HTTP队列");

            // 检查WiFi是否已连接
            if (!wifi_connected) {
                ESP_LOGW(TAG, "WiFi未连接, 跳过HTTP请求");
                continue;
            }

            ESP_LOGI(TAG, "HTTP任务处理数据: %.*s", req.len, req.data);

            // 构建POST数据
            snprintf(post_data, sizeof(post_data), "{\"data\":\"%.*s\"}", req.len, req.data);

            // 配置HTTP客户端
            esp_http_client_config_t config = {
                .url = HTTP_URI,
                .method = HTTP_METHOD_POST,
                .timeout_ms = 5000,  // 5秒超时
            };

            esp_http_client_handle_t client = esp_http_client_init(&config);
            if (client == NULL) {
                ESP_LOGE(TAG, "HTTP客户端初始化失败");
                continue;
            }

            // 设置请求头
            esp_http_client_set_header(client, "Content-Type", "application/json");
            // 设置 POST 数据
            esp_http_client_set_post_field(client, post_data, strlen(post_data));

            // 执行请求
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                int status_code = esp_http_client_get_status_code(client);
                ESP_LOGI(TAG, "HTTP请求成功, 状态码: %d", status_code);
            } else {
                ESP_LOGE(TAG, "HTTP请求失败: %s", esp_err_to_name(err));
            }

            // 清理
            esp_http_client_cleanup(client);
        }
    }
}

/**
 * @brief 数据接收回调函数 - 在中断上下文中执行，必须快速完成，不能有任何阻塞操作
 *
 * @param[in] data     指向接收数据的指针
 * @param[in] data_len 接收数据的字节数
 * @param[in] arg      传递给设备打开函数的参数
 * @return
 *   true:  我们已处理接收到的数据
 *   false: 我们期望更多数据
 */
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    // 遍历接收到的每个字节
    for (size_t i = 0; i < data_len; i++)
    {
        uint8_t byte = data[i];

        // 检查缓冲区是否已满
        if (rx_buffer_len >= RX_BUFFER_SIZE - 1)
        {
            // 缓冲区满，直接清空，不打印日志（中断中不能调用ESP_LOG）
            rx_buffer_len = 0;
        }

        // 将字节存入缓冲区
        rx_buffer[rx_buffer_len++] = byte;

        // 检测到 \r\n 结束符
        if (byte == '\n' && rx_buffer_len > 1 && rx_buffer[rx_buffer_len - 2] == '\r')
        {
            // 移除 \r\n 结束符
            rx_buffer_len -= 2;
            rx_buffer[rx_buffer_len] = '\0'; // 字符串终止符

            // 将数据发送到HTTP队列（非阻塞，中断安全）
            if (http_request_queue != NULL) {
                http_request_t req;
                size_t copy_len = (rx_buffer_len < RX_BUFFER_SIZE - 1) ? rx_buffer_len : (RX_BUFFER_SIZE - 1);
                memcpy(req.data, rx_buffer, copy_len);
                req.data[copy_len] = '\0';
                req.len = copy_len;

                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xQueueSendFromISR(http_request_queue, &req, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }

            // 重置缓冲区
            rx_buffer_len = 0;
        }
    }

    return true;
}

/**
 * @brief 设备事件回调函数
 *
 * 除了处理设备断开连接外，没有其他有用的操作
 *
 * @param[in] event    设备事件类型和数据
 * @param[in] user_ctx 传递给设备打开函数的参数
 */
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM 发生错误, 错误号 = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "设备突然断开连接");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "串口状态通知 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        ESP_LOGW(TAG, "不支持的 CDC 事件: %i", event->type);
        break;
    }
}

/**
 * @brief USB Host 库处理任务
 *
 * @param arg 未使用
 */
static void usb_lib_task(void *arg)
{
    while (1)
    {
        // 开始处理系统事件
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG, "USB: 所有设备已释放");
        }
    }
}

/**
 * @brief WiFi重连任务 - 在独立任务中处理重连延迟
 */
static void wifi_reconnect_task(void *arg)
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
        wifi_need_reconnect = true;  // 设置重连标志，让重连任务处理
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi连接成功! IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/**
 * @brief 初始化 WiFi 连接
 */
static void initialise_wifi(void)
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

/**
 * @brief 主应用程序
 *
 * 在这里我们打开一个 USB CDC 设备并从它接收数据
 */
void app_main(void)
{
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // 创建HTTP请求队列
    http_request_queue = xQueueCreate(HTTP_QUEUE_SIZE, sizeof(http_request_t));
    assert(http_request_queue);

    // 创建HTTP请求任务
    BaseType_t http_task_created = xTaskCreate(
        http_request_task,
        "http_task",
        HTTP_TASK_STACK_SIZE,
        NULL,
        HTTP_TASK_PRIORITY,
        NULL
    );
    assert(http_task_created == pdTRUE);

    // 安装 USB Host 驱动。在整个应用程序中应只调用一次
    ESP_LOGI(TAG, "正在安装 USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // 创建一个处理 USB 库事件的任务
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), EXAMPLE_USB_HOST_PRIORITY, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "正在安装 CDC-ACM 驱动");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = handle_event,
        .data_cb = handle_rx};

    // 初始化 NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 连接WIFI
    initialise_wifi();

    while (true)
    {
        cdc_acm_dev_hdl_t cdc_dev = NULL;

        ESP_LOGI(TAG, "正在打开 CDC ACM 设备 0x%04X:0x%04X...", EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_PID);
        esp_err_t err = cdc_acm_host_open(EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_PID, 0, &dev_config, &cdc_dev);
        if (ESP_OK != err)
        {
            ESP_LOGI(TAG, "设备打开失败");
            continue;
        }
        cdc_acm_host_desc_print(cdc_dev);
        vTaskDelay(pdMS_TO_TICKS(100));

        cdc_acm_line_coding_t line_coding;
        ESP_ERROR_CHECK(cdc_acm_host_line_coding_get(cdc_dev, &line_coding));
        ESP_LOGI(TAG, "串口信息: 波特率: %" PRIu32 ", 停止位: %" PRIu8 ", 校验位: %" PRIu8 ", 数据位: %" PRIu8 "",
                 line_coding.dwDTERate, line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

        ESP_ERROR_CHECK(cdc_acm_host_set_control_line_state(cdc_dev, true, false));

        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}
