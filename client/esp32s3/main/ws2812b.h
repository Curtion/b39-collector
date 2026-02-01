/*
 * WS2812B LED 控制模块头文件
 */

#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>
#include "esp_err.h"

// WS2812B 配置（从 config.h 引入）
#define WS2812B_GPIO_NUM       48   // LED 数据引脚
#define WS2812B_LED_NUMBERS    1    // LED 数量

/**
 * @brief 初始化 WS2812B LED 模块
 * 
 * 初始化 RMT 通道和编码器，初始化后 LED 为关闭状态（全黑）
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ws2812b_init(void);

/**
 * @brief 设置单个 LED 的颜色
 * 
 * @param index LED 索引（从 0 开始）
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ws2812b_set_pixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 设置所有 LED 为相同颜色
 * 
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ws2812b_set_all_pixels(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 刷新 LED 显示（将颜色数据发送到 LED）
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ws2812b_refresh(void);

/**
 * @brief 关闭所有 LED（全黑）
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ws2812b_clear(void);

/**
 * @brief 反初始化 WS2812B 模块，释放资源
 */
void ws2812b_deinit(void);

#endif // WS2812B_H
