/*
 * USB CDC 模块头文件
 */

#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "usb/cdc_acm_host.h"

/**
 * @brief 初始化 USB CDC 模块
 */
void usb_cdc_init(void);

/**
 * @brief USB Host 库处理任务
 */
void usb_lib_task(void *arg);

/**
 * @brief 数据接收回调函数
 */
bool usb_cdc_handle_rx(const uint8_t *data, size_t data_len, void *arg);

/**
 * @brief 设备事件回调函数
 */
void usb_cdc_handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx);

/**
 * @brief 获取设备断开信号量
 */
SemaphoreHandle_t usb_cdc_get_disconnect_sem(void);

#endif // USB_CDC_H
