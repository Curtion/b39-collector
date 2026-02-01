/*
 * HTTP 客户端模块实现
 */

#include "http_client.h"
#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_http_client.h"

static const char *TAG = "HTTP";

QueueHandle_t http_request_queue = NULL;

void http_request_task(void *arg)
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
            snprintf(post_data, sizeof(post_data), "{\"data\":\"%.*s\"}", (int)req.len, req.data);

            // 配置HTTP客户端
            esp_http_client_config_t config = {
                .url = HTTP_URI,
                .method = HTTP_METHOD_POST,
                .timeout_ms = 5000,
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

BaseType_t http_client_send_from_isr(const uint8_t *data, size_t len)
{
    if (http_request_queue == NULL) {
        return pdFALSE;
    }

    http_request_t req;
    size_t copy_len = (len < RX_BUFFER_SIZE - 1) ? len : (RX_BUFFER_SIZE - 1);
    memcpy(req.data, data, copy_len);
    req.data[copy_len] = '\0';
    req.len = copy_len;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t result = xQueueSendFromISR(http_request_queue, &req, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return result;
}

void http_client_init(void)
{
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
}
