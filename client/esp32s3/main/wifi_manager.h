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

/**
 * @brief 初始化 WiFi 连接
 */
void wifi_manager_init(void);

/**
 * @brief WiFi 重连任务
 */
void wifi_reconnect_task(void *arg);

#endif // WIFI_MANAGER_H
