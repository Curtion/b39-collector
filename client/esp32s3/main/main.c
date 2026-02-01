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

#include "nvs_flash.h"
#include "usb/cdc_acm_host.h"

#include "config.h"
#include "wifi_manager.h"
#include "http_client.h"
#include "usb_cdc.h"

static const char *TAG = "MAIN";

/**
 * @brief 主应用程序
 *
 * 初始化所有模块并处理 USB CDC 设备连接
 */
void app_main(void)
{
    // 初始化 HTTP 客户端模块
    http_client_init();

    // 初始化 USB CDC 模块
    usb_cdc_init();

    // 初始化 NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 初始化 WiFi 模块
    wifi_manager_init();

    // CDC 设备配置
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = usb_cdc_handle_event,
        .data_cb = usb_cdc_handle_rx
    };

    SemaphoreHandle_t device_disconnected_sem = usb_cdc_get_disconnect_sem();

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
