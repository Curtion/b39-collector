/*
 * USB CDC 模块实现
 */

#include "usb_cdc.h"
#include "http_client.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

static const char *TAG = "USB-CDC";
static SemaphoreHandle_t device_disconnected_sem = NULL;

// 接收缓冲区和相关变量
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static size_t rx_buffer_len = 0;

SemaphoreHandle_t usb_cdc_get_disconnect_sem(void)
{
    return device_disconnected_sem;
}

bool usb_cdc_handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    // 遍历接收到的每个字节
    for (size_t i = 0; i < data_len; i++)
    {
        uint8_t byte = data[i];

        // 检查缓冲区是否已满
        if (rx_buffer_len >= RX_BUFFER_SIZE - 1)
        {
            // 缓冲区满，直接清空
            rx_buffer_len = 0;
        }

        // 将字节存入缓冲区
        rx_buffer[rx_buffer_len++] = byte;

        // 检测到 \r\n 结束符
        if (byte == '\n' && rx_buffer_len > 1 && rx_buffer[rx_buffer_len - 2] == '\r')
        {
            // 移除 \r\n 结束符
            rx_buffer_len -= 2;
            rx_buffer[rx_buffer_len] = '\0';

            // 将数据发送到HTTP队列
            http_client_send_from_isr(rx_buffer, rx_buffer_len);

            // 重置缓冲区
            rx_buffer_len = 0;
        }
    }

    return true;
}

void usb_cdc_handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
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

void usb_lib_task(void *arg)
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

void usb_cdc_init(void)
{
    // 创建设备断开信号量
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // 安装 USB Host 驱动
    ESP_LOGI(TAG, "正在安装 USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // 创建一个处理 USB 库事件的任务
    BaseType_t task_created = xTaskCreate(
        usb_lib_task,
        "usb_lib",
        4096,
        xTaskGetCurrentTaskHandle(),
        EXAMPLE_USB_HOST_PRIORITY,
        NULL
    );
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "正在安装 CDC-ACM 驱动");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
}
