/*
 * LED 状态指示模块头文件
 * 使用层级叠加器架构管理 LED 状态
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * LED 状态层级（优先级从高到低）
 * 
 * 高优先级层会覆盖低优先级层的显示
 */
typedef enum {
    LED_LAYER_OVERRIDE = 0,     // 临时覆盖层（DATA_TX, FORCE_OFF）- 最高优先级
    LED_LAYER_COMM,             // 通信状态层（HTTP_ERROR）
    LED_LAYER_CONN,             // 连接状态层（WIFI状态）- 自动管理
    LED_LAYER_MAX
} led_layer_t;

/**
 * LED 显示状态
 */
typedef enum {
    LED_STATE_NONE = 0,         // 无状态（透明，显示下一层）
    LED_STATE_OFF,              // 关闭（黑色）
    LED_STATE_SMARTCONFIG,      // 配网中（蓝色快闪）
    LED_STATE_WIFI_CONNECTING,  // WiFi 连接中（紫色慢闪）
    LED_STATE_HTTP_ERROR,       // HTTP 错误（黄色慢闪）
    LED_STATE_DATA_TX,          // 数据传输（青色）
    LED_STATE_NORMAL,           // 正常（绿色）
    LED_STATE_MAX
} led_state_t;

// LED 颜色定义 (R, G, B) - 降低亮度至约 10%
#define LED_COLOR_OFF           0, 0, 0
#define LED_COLOR_GREEN         0, 250, 0
#define LED_COLOR_BLUE          0, 0, 250
#define LED_COLOR_YELLOW        250, 180, 0
#define LED_COLOR_PURPLE        120, 0, 120
#define LED_COLOR_CYAN          0, 200, 200

/**
 * @brief 初始化 LED 状态指示模块
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t led_status_init(void);

/**
 * @brief 设置某一层的状态
 * 
 * @param layer 要设置的层级
 * @param state 状态（LED_STATE_NONE 表示清除该层）
 * @param timeout_ms 超时时间（毫秒），0 表示无超时
 */
void led_layer_set(led_layer_t layer, led_state_t state, uint32_t timeout_ms);

/**
 * @brief 清除某一层的状态
 * 
 * @param layer 要清除的层级
 */
void led_layer_clear(led_layer_t layer);

/**
 * @brief 获取当前实际显示的状态
 * 
 * @return 当前显示的状态
 */
led_state_t led_status_get(void);

// ============ 便捷 API（封装层级操作）============

/**
 * @brief 触发数据传输闪烁
 * 
 * @param duration_ms 持续时间（毫秒）
 */
static inline void led_blink_data_tx(uint32_t duration_ms)
{
    led_layer_set(LED_LAYER_OVERRIDE, LED_STATE_DATA_TX, duration_ms);
}

/**
 * @brief 设置 HTTP 错误状态
 * 
 * @param error true 表示 HTTP 错误，false 表示清除
 */
static inline void led_set_http_error(bool error)
{
    if (error) {
        led_layer_set(LED_LAYER_COMM, LED_STATE_HTTP_ERROR, 0);
    } else {
        led_layer_clear(LED_LAYER_COMM);
    }
}

/**
 * @brief 强制关闭 LED
 */
static inline void led_force_off(void)
{
    led_layer_set(LED_LAYER_OVERRIDE, LED_STATE_OFF, 0);
}

/**
 * @brief 恢复 LED 显示（取消强制关闭）
 */
static inline void led_resume(void)
{
    led_layer_clear(LED_LAYER_OVERRIDE);
}

/**
 * @brief 反初始化 LED 状态模块
 */
void led_status_deinit(void);

#endif // LED_STATUS_H
