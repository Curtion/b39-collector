/*
 * HTTP 客户端模块头文件
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "config.h"

// HTTP请求数据结构
typedef struct {
    char data[RX_BUFFER_SIZE];
    size_t len;
} http_request_t;

// HTTP 请求队列（外部访问）
extern QueueHandle_t http_request_queue;

/**
 * @brief 初始化 HTTP 客户端模块
 */
void http_client_init(void);

/**
 * @brief HTTP 请求任务
 */
void http_request_task(void *arg);

/**
 * @brief 发送数据到 HTTP 队列
 * @param data 要发送的数据
 * @param len 数据长度
 * @return pdTRUE 成功，pdFALSE 失败
 */
BaseType_t http_client_send(const uint8_t *data, size_t len);

#endif // HTTP_CLIENT_H
