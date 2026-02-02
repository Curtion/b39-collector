/*
 * HTTP 服务器模块头文件
 * 提供静态文件服务和配置接口
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 初始化并启动 HTTP 服务器
 * @return ESP_OK 成功，其他错误码表示失败
 */
esp_err_t http_server_init(void);

/**
 * @brief 停止 HTTP 服务器
 * @return ESP_OK 成功，其他错误码表示失败
 */
esp_err_t http_server_stop(void);

/**
 * @brief 获取当前 HTTP URI 配置
 * @return 当前配置的 HTTP URI 字符串
 */
const char* http_server_get_uri(void);

/**
 * @brief 设置 HTTP URI 配置
 * @param uri 新的 HTTP URI
 * @return ESP_OK 成功，其他错误码表示失败
 */
esp_err_t http_server_set_uri(const char *uri);

#endif // HTTP_SERVER_H
