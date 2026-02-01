/*
 * GPIO 按键模块头文件
 * 用于监听 GPIO0 按键按下事件
 */

#ifndef GPIO_BUTTON_H
#define GPIO_BUTTON_H

#include "esp_err.h"

/**
 * @brief 初始化 GPIO 按键监听模块
 * 
 * 配置 GPIO0 为输入模式，启用内部上拉电阻，并设置中断处理程序
 * 按键按下时会触发中断，经过防抖处理后执行回调
 */
esp_err_t gpio_button_init(void);

/**
 * @brief 注册按键按下回调函数
 * 
 * @param callback 按键按下时调用的函数指针
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t gpio_button_register_callback(void (*callback)(void));

/**
 * @brief 注销按键回调函数
 */
void gpio_button_unregister_callback(void);

/**
 * @brief 启动 SmartConfig 配网（供按键回调使用）
 * 此函数会检查当前是否已在配网，避免重复启动
 */
void gpio_button_start_smartconfig(void);

#endif // GPIO_BUTTON_H
