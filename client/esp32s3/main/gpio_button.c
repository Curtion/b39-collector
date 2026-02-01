/*
 * GPIO 按键模块实现
 * 监听 GPIO0 按键按下事件，支持防抖处理
 */

#include "gpio_button.h"
#include "config.h"
#include "wifi_manager.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"

static const char *TAG = "GPIO_BUTTON";

// 按键状态
static volatile bool button_pressed = false;
static volatile uint32_t last_press_time = 0;
static void (*button_callback)(void) = NULL;
static SemaphoreHandle_t button_sem = NULL;

// 防抖时间（毫秒）
#define DEBOUNCE_MS 50

// 长按阈值（毫秒）- 超过此时间认为是长按，用于重置配网
#define LONG_PRESS_MS 3000

// 按键检测任务句柄
static TaskHandle_t button_task_handle = NULL;

/**
 * @brief GPIO 中断处理程序
 * 
 * 当 GPIO0 电平变化时触发，使用信号量通知按键处理任务
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    if (button_sem != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(button_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/**
 * @brief 按键处理任务
 * 
 * 等待中断信号，进行防抖处理，检测长按/短按
 */
static void button_task(void *arg)
{
    uint32_t press_start_time = 0;
    bool is_pressing = false;
    bool long_press_triggered = false;

    ESP_LOGI(TAG, "按键监听任务已启动, GPIO: %d", GPIO_BUTTON_PIN);

    while (1) {
        TickType_t wait_ticks = portMAX_DELAY;

        // 如果按键已按下且未触发长按，设置超时时间为剩余的长按时间
        if (is_pressing && !long_press_triggered) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t elapsed = now - press_start_time;
            if (elapsed < LONG_PRESS_MS) {
                wait_ticks = pdMS_TO_TICKS(LONG_PRESS_MS - elapsed);
            } else {
                wait_ticks = 0;
            }
        }

        // 等待中断信号或超时
        if (xSemaphoreTake(button_sem, wait_ticks) == pdTRUE) {
            // 收到中断信号（电平变化）
            int level = gpio_get_level(GPIO_BUTTON_PIN);
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // 防抖检查
            if ((current_time - last_press_time) < DEBOUNCE_MS) {
                continue;
            }
            last_press_time = current_time;

            if (level == 0) {
                // 按键按下
                if (!is_pressing) {
                    is_pressing = true;
                    long_press_triggered = false;
                    press_start_time = current_time;
                    ESP_LOGD(TAG, "按键按下");
                }
            } else {
                // 按键释放
                if (is_pressing) {
                    uint32_t press_duration = current_time - press_start_time;
                    is_pressing = false;

                    if (!long_press_triggered) {
                        // 未触发长按，则判定为短按
                        ESP_LOGI(TAG, "按键短按检测 (%lu ms)", press_duration);
                        if (button_callback != NULL) {
                            button_callback();
                        }
                    } else {
                        // 已经触发过长按，释放时不做处理（只复位状态）
                        ESP_LOGD(TAG, "长按释放");
                    }
                }
            }
        } else {
            // 超时（表示一直按着直到达到长按阈值）
            if (is_pressing && !long_press_triggered) {
                // 再次确认电平状态（防止误判）
                if (gpio_get_level(GPIO_BUTTON_PIN) == 0) {
                    long_press_triggered = true;
                    uint32_t press_duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) - press_start_time;
                    ESP_LOGI(TAG, "按键长按检测 (%lu ms) - 触发动作，启动 SmartConfig 配网", press_duration);
                    gpio_button_start_smartconfig();
                }
            }
        }
    }
}

esp_err_t gpio_button_init(void)
{
    ESP_LOGI(TAG, "初始化 GPIO 按键模块...");

    // 创建信号量
    button_sem = xSemaphoreCreateBinary();
    if (button_sem == NULL) {
        ESP_LOGE(TAG, "创建信号量失败");
        return ESP_FAIL;
    }

    // 配置 GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,           // 双边沿触发
        .mode = GPIO_MODE_INPUT,                   // 输入模式
        .pin_bit_mask = (1ULL << GPIO_BUTTON_PIN), // 引脚掩码
        .pull_down_en = GPIO_PULLDOWN_DISABLE,     // 禁用下拉
        .pull_up_en = GPIO_PULLUP_ENABLE,          // 启用上拉
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 配置失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(button_sem);
        button_sem = NULL;
        return ret;
    }

    // 安装 GPIO 中断服务
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "GPIO 中断服务安装失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(button_sem);
        button_sem = NULL;
        return ret;
    }

    // 添加中断处理程序
    ret = gpio_isr_handler_add(GPIO_BUTTON_PIN, gpio_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加 GPIO 中断处理程序失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(button_sem);
        button_sem = NULL;
        return ret;
    }

    // 创建按键处理任务
    BaseType_t task_created = xTaskCreate(
        button_task,
        "button_task",
        GPIO_BUTTON_TASK_STACK_SIZE,
        NULL,
        GPIO_BUTTON_TASK_PRIORITY,
        &button_task_handle
    );

    if (task_created != pdTRUE) {
        ESP_LOGE(TAG, "创建按键任务失败");
        gpio_isr_handler_remove(GPIO_BUTTON_PIN);
        vSemaphoreDelete(button_sem);
        button_sem = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GPIO 按键模块初始化完成");
    return ESP_OK;
}

esp_err_t gpio_button_register_callback(void (*callback)(void))
{
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    button_callback = callback;
    ESP_LOGI(TAG, "按键回调函数已注册");
    return ESP_OK;
}

void gpio_button_unregister_callback(void)
{
    button_callback = NULL;
    ESP_LOGI(TAG, "按键回调函数已注销");
}

void gpio_button_start_smartconfig(void)
{
    ESP_LOGI(TAG, "重置 WiFi 配置并重启设备...");

    // 重置 WiFi 配置（清除 NVS 中的保存的凭据）
    esp_err_t ret = wifi_manager_reset_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重置 WiFi 配置失败: %s", esp_err_to_name(ret));
    }

    // 重启设备，重启后 wifi_manager_init() 会检测到无配置并自动启动 SmartConfig
    esp_restart();
}
