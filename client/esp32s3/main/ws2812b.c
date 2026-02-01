/*
 * WS2812B LED 控制模块
 * 使用 RMT 驱动 WS2812B LED
 */

#include <string.h>
#include "ws2812b.h"
#include "config.h"

#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz 分辨率, 1 tick = 0.1us

static const char *TAG = "ws2812b";

static uint8_t led_strip_pixels[WS2812B_LED_NUMBERS * 3];
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t simple_encoder = NULL;

// WS2812B 时序定义
static const rmt_symbol_word_t ws2812_zero = {
    .level0 = 1,
    .duration0 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
    .level1 = 0,
    .duration1 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=0.9us
};

static const rmt_symbol_word_t ws2812_one = {
    .level0 = 1,
    .duration0 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=0.9us
    .level1 = 0,
    .duration1 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
};

// Reset 信号，默认 50us
static const rmt_symbol_word_t ws2812_reset = {
    .level0 = 0,
    .duration0 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
    .level1 = 0,
    .duration1 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
};

static size_t encoder_callback(const void *data, size_t data_size,
                               size_t symbols_written, size_t symbols_free,
                               rmt_symbol_word_t *symbols, bool *done, void *arg)
{
    // 编码一个字节需要 8 个符号空间
    if (symbols_free < 8) {
        return 0;
    }

    // 计算当前在数据中的位置
    size_t data_pos = symbols_written / 8;
    uint8_t *data_bytes = (uint8_t*)data;
    
    if (data_pos < data_size) {
        // 编码一个字节
        size_t symbol_pos = 0;
        for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
            if (data_bytes[data_pos] & bitmask) {
                symbols[symbol_pos++] = ws2812_one;
            } else {
                symbols[symbol_pos++] = ws2812_zero;
            }
        }
        // 完成，已写入 8 个符号
        return symbol_pos;
    } else {
        // 所有字节已编码，发送 reset 信号
        symbols[0] = ws2812_reset;
        *done = 1; // 指示传输结束
        return 1;  // 只写入了一个符号
    }
}

esp_err_t ws2812b_init(void)
{
    ESP_LOGI(TAG, "初始化 WS2812B LED (GPIO%d, %d LEDs)", WS2812B_GPIO_NUM, WS2812B_LED_NUMBERS);

    // 创建 RMT TX 通道
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = WS2812B_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &led_chan), TAG, "创建 RMT 通道失败");

    // 创建简单编码器
    const rmt_simple_encoder_config_t simple_encoder_cfg = {
        .callback = encoder_callback
    };
    ESP_RETURN_ON_ERROR(rmt_new_simple_encoder(&simple_encoder_cfg, &simple_encoder), TAG, "创建编码器失败");

    // 启用 RMT 通道
    ESP_RETURN_ON_ERROR(rmt_enable(led_chan), TAG, "启用 RMT 通道失败");

    // 初始化时关闭所有 LED（全黑）
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    ESP_RETURN_ON_ERROR(ws2812b_refresh(), TAG, "初始刷新 LED 失败");

    ESP_LOGI(TAG, "WS2812B 初始化完成，LED 已关闭");
    return ESP_OK;
}

esp_err_t ws2812b_set_pixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (index >= WS2812B_LED_NUMBERS) {
        ESP_LOGE(TAG, "LED 索引 %d 超出范围 (最大 %d)", index, WS2812B_LED_NUMBERS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // WS2812B 使用 GRB 顺序
    led_strip_pixels[index * 3 + 0] = green;
    led_strip_pixels[index * 3 + 1] = red;
    led_strip_pixels[index * 3 + 2] = blue;

    return ESP_OK;
}

esp_err_t ws2812b_set_all_pixels(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int i = 0; i < WS2812B_LED_NUMBERS; i++) {
        led_strip_pixels[i * 3 + 0] = green;
        led_strip_pixels[i * 3 + 1] = red;
        led_strip_pixels[i * 3 + 2] = blue;
    }
    return ESP_OK;
}

esp_err_t ws2812b_refresh(void)
{
    if (led_chan == NULL || simple_encoder == NULL) {
        ESP_LOGE(TAG, "WS2812B 未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    ESP_RETURN_ON_ERROR(rmt_transmit(led_chan, simple_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config),
                        TAG, "RMT 传输失败");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(led_chan, portMAX_DELAY), TAG, "等待 RMT 完成失败");

    return ESP_OK;
}

esp_err_t ws2812b_clear(void)
{
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
    return ws2812b_refresh();
}

void ws2812b_deinit(void)
{
    if (led_chan != NULL) {
        rmt_disable(led_chan);
        rmt_del_channel(led_chan);
        led_chan = NULL;
    }
    if (simple_encoder != NULL) {
        rmt_del_encoder(simple_encoder);
        simple_encoder = NULL;
    }
    ESP_LOGI(TAG, "WS2812B 已反初始化");
}
