/*
 * LED 状态指示模块实现
 * 使用层级叠加器架构：高优先级层覆盖低优先级层
 */

#include "led_status.h"
#include "ws2812b.h"
#include "wifi_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "LED_STATUS";

// 任务配置
#define LED_STATUS_TASK_STACK_SIZE      2048
#define LED_STATUS_TASK_PRIORITY        2
#define LED_STATUS_REFRESH_INTERVAL_MS  100

// 闪烁配置
#define BLINK_FAST_INTERVAL_MS          200   // 快闪间隔（配网中）
#define BLINK_SLOW_INTERVAL_MS          500   // 慢闪间隔（连接中）

// 状态显示配置表
static const struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool blink;
    uint32_t blink_interval_ms;
} state_config[LED_STATE_MAX] = {
    [LED_STATE_NONE]            = { LED_COLOR_OFF,    false, 0 },
    [LED_STATE_OFF]             = { LED_COLOR_OFF,    false, 0 },
    [LED_STATE_SMARTCONFIG]     = { LED_COLOR_BLUE,   true,  BLINK_FAST_INTERVAL_MS },
    [LED_STATE_WIFI_CONNECTING] = { LED_COLOR_PURPLE, true,  BLINK_SLOW_INTERVAL_MS },
    [LED_STATE_HTTP_ERROR]      = { LED_COLOR_YELLOW, true,  BLINK_SLOW_INTERVAL_MS },
    [LED_STATE_DATA_TX]         = { LED_COLOR_CYAN,   false, 0 },
    [LED_STATE_NORMAL]          = { LED_COLOR_GREEN,  false, 0 },
};

// 层级状态结构
typedef struct {
    led_state_t state;          // 当前状态
    uint32_t expire_time;       // 过期时间（0 表示无超时）
} layer_info_t;

static TaskHandle_t led_task_handle = NULL;
static SemaphoreHandle_t led_mutex = NULL;

// 三层状态
static layer_info_t layers[LED_LAYER_MAX] = {0};

/**
 * @brief 根据 WiFi 状态推断连接层状态
 */
static led_state_t infer_conn_state(void)
{
    if (wifi_connected) {
        return LED_STATE_NORMAL;
    }
    if (smartconfig_active) {
        return LED_STATE_SMARTCONFIG;
    }
    return LED_STATE_WIFI_CONNECTING;
}

/**
 * @brief 计算当前应该显示的状态（从高优先级层向下查找）
 */
static led_state_t compute_display_state(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    for (int i = 0; i < LED_LAYER_MAX; i++) {
        // 检查超时
        if (layers[i].expire_time > 0 && now >= layers[i].expire_time) {
            layers[i].state = LED_STATE_NONE;
            layers[i].expire_time = 0;
        }
        
        // 如果该层有有效状态，返回它
        if (layers[i].state != LED_STATE_NONE) {
            return layers[i].state;
        }
    }
    
    return LED_STATE_OFF;
}

/**
 * @brief LED 状态更新任务
 */
static void led_status_task(void *arg)
{
    bool blink_on = false;
    uint32_t last_blink_time = 0;
    led_state_t last_state = LED_STATE_NONE;
    
    ESP_LOGI(TAG, "LED 状态任务启动");
    
    while (1) {
        xSemaphoreTake(led_mutex, portMAX_DELAY);
        
        // 自动更新连接层状态
        layers[LED_LAYER_CONN].state = infer_conn_state();
        
        // WiFi 断开时清除通信层错误
        if (!wifi_connected && layers[LED_LAYER_COMM].state == LED_STATE_HTTP_ERROR) {
            layers[LED_LAYER_COMM].state = LED_STATE_NONE;
        }
        
        // 计算最终显示状态
        led_state_t display_state = compute_display_state();
        
        xSemaphoreGive(led_mutex);
        
        // 获取显示配置
        uint8_t r = state_config[display_state].r;
        uint8_t g = state_config[display_state].g;
        uint8_t b = state_config[display_state].b;
        bool should_blink = state_config[display_state].blink;
        uint32_t blink_interval = state_config[display_state].blink_interval_ms;
        
        // 状态变化时重置闪烁
        if (display_state != last_state) {
            blink_on = true;
            last_blink_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            last_state = display_state;
        }
        
        // 处理闪烁
        if (should_blink) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_blink_time >= blink_interval) {
                blink_on = !blink_on;
                last_blink_time = now;
            }
            if (!blink_on) {
                r = g = b = 0;
            }
        }
        
        // 更新 LED
        ws2812b_set_pixel(0, r, g, b);
        ws2812b_refresh();
        
        vTaskDelay(pdMS_TO_TICKS(LED_STATUS_REFRESH_INTERVAL_MS));
    }
}

esp_err_t led_status_init(void)
{
    ESP_LOGI(TAG, "初始化 LED 状态模块（层级架构）");
    
    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化所有层为空
    for (int i = 0; i < LED_LAYER_MAX; i++) {
        layers[i].state = LED_STATE_NONE;
        layers[i].expire_time = 0;
    }
    
    // 连接层立即根据 WiFi 状态设置
    layers[LED_LAYER_CONN].state = infer_conn_state();
    
    BaseType_t ret = xTaskCreate(
        led_status_task,
        "led_status",
        LED_STATUS_TASK_STACK_SIZE,
        NULL,
        LED_STATUS_TASK_PRIORITY,
        &led_task_handle
    );
    
    if (ret != pdPASS) {
        vSemaphoreDelete(led_mutex);
        ESP_LOGE(TAG, "创建 LED 状态任务失败");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "LED 状态模块初始化完成");
    return ESP_OK;
}

void led_layer_set(led_layer_t layer, led_state_t state, uint32_t timeout_ms)
{
    if (layer >= LED_LAYER_MAX || state >= LED_STATE_MAX) {
        return;
    }
    
    xSemaphoreTake(led_mutex, portMAX_DELAY);
    
    layers[layer].state = state;
    if (timeout_ms > 0) {
        layers[layer].expire_time = xTaskGetTickCount() * portTICK_PERIOD_MS + timeout_ms;
    } else {
        layers[layer].expire_time = 0;
    }
    
    ESP_LOGD(TAG, "层 %d 设置状态 %d, 超时 %lu ms", layer, state, timeout_ms);
    
    xSemaphoreGive(led_mutex);
}

void led_layer_clear(led_layer_t layer)
{
    if (layer >= LED_LAYER_MAX) {
        return;
    }
    
    xSemaphoreTake(led_mutex, portMAX_DELAY);
    layers[layer].state = LED_STATE_NONE;
    layers[layer].expire_time = 0;
    ESP_LOGD(TAG, "层 %d 已清除", layer);
    xSemaphoreGive(led_mutex);
}

led_state_t led_status_get(void)
{
    led_state_t state;
    xSemaphoreTake(led_mutex, portMAX_DELAY);
    state = compute_display_state();
    xSemaphoreGive(led_mutex);
    return state;
}

void led_status_deinit(void)
{
    if (led_task_handle != NULL) {
        vTaskDelete(led_task_handle);
        led_task_handle = NULL;
    }
    
    if (led_mutex != NULL) {
        vSemaphoreDelete(led_mutex);
        led_mutex = NULL;
    }
    
    ws2812b_clear();
    ESP_LOGI(TAG, "LED 状态模块已反初始化");
}
