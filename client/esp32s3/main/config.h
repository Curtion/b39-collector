/*
 * 配置文件 - 定义所有模块的配置常量
 */

#ifndef CONFIG_H
#define CONFIG_H

// USB 配置
#define EXAMPLE_USB_HOST_PRIORITY (10)
#define EXAMPLE_USB_DEVICE_VID (0x303A)
#define EXAMPLE_USB_DEVICE_PID (0x1001)
#define RX_BUFFER_SIZE (2048)

// WiFi 配置
#define WIFI_SSID "SukeMeu"
#define WIFI_PASSWORD "123456789"

// HTTP 配置
#define HTTP_URI "https://blog.3gxk.net"

// WiFi 重连延迟（毫秒）
#define WIFI_RECONNECT_DELAY_MS 3000

// 任务配置
#define WIFI_RECONNECT_TASK_PRIORITY 3
#define WIFI_RECONNECT_TASK_STACK_SIZE 4096

#define HTTP_QUEUE_SIZE 5
#define HTTP_TASK_PRIORITY 5
#define HTTP_TASK_STACK_SIZE 8192

#endif // CONFIG_H
