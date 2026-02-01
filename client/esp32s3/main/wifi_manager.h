/*
 * WiFi 管理模块头文件
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi 连接状态标志（外部访问）
extern volatile bool wifi_connected;
extern volatile bool wifi_need_reconnect;

// SmartConfig 配网状态标志
extern volatile bool smartconfig_done;
extern volatile bool smartconfig_active;

/**
 * @brief 初始化 WiFi 连接
 */
void wifi_manager_init(void);

/**
 * @brief WiFi 重连任务
 */
void wifi_reconnect_task(void *arg);

/**
 * @brief SmartConfig 配网任务
 */
void smartconfig_task(void *arg);

/**
 * @brief 重置 WiFi 配网（清除保存的凭据并重新配网）
 */
esp_err_t wifi_manager_reset_config(void);

/**
 * @brief 获取当前 WiFi SSID
 * @param ssid 输出缓冲区，至少33字节
 * @return ESP_OK 成功，ESP_FAIL 未连接
 */
esp_err_t wifi_manager_get_ssid(char *ssid);

#endif // WIFI_MANAGER_H
