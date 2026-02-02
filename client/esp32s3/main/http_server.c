/*
 * HTTP 服务器模块实现
 * 提供静态文件服务和配置接口（仿照 ESP-IDF restful_server 示例）
 */

#include "http_server.h"
#include "config.h"

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "HTTP_SERVER";

// NVS 存储配置
#define NVS_NAMESPACE "http_config"
#define NVS_KEY_URI   "http_uri"

// HTTP URI 最大长度
#define HTTP_URI_MAX_LEN 256

// 文件路径最大长度
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)

// 读取缓冲区大小
#define SCRATCH_BUFSIZE (8192)

// SPIFFS 挂载点
#define WEB_MOUNT_POINT "/www"

// 当前 HTTP URI 配置（存储在内存中，启动时从 NVS 加载）
static char s_http_uri[HTTP_URI_MAX_LEN] = "";

// HTTP 服务器句柄
static httpd_handle_t s_server = NULL;

// 服务器上下文结构
typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

static rest_server_context_t *s_rest_context = NULL;

/**
 * @brief 从 NVS 加载 HTTP URI 配置
 */
static esp_err_t load_uri_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS 命名空间不存在，使用默认 URI: %s", s_http_uri);
        return ESP_OK;
    }

    size_t required_size = sizeof(s_http_uri);
    err = nvs_get_str(nvs_handle, NVS_KEY_URI, s_http_uri, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "从 NVS 加载 HTTP URI: %s", s_http_uri);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS 中无 URI 配置，使用默认值: %s", s_http_uri);
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "读取 NVS 失败: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 保存 HTTP URI 配置到 NVS
 */
static esp_err_t save_uri_to_nvs(const char *uri)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "打开 NVS 失败");

    err = nvs_set_str(nvs_handle, NVS_KEY_URI, uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "写入 NVS 失败: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "提交 NVS 失败: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 根据文件扩展名设置 Content-Type
 */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (strstr(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (strstr(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (strstr(filename, ".js")) {
        return httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    } else if (strstr(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (strstr(filename, ".svg")) {
        return httpd_resp_set_type(req, "image/svg+xml");
    } else if (strstr(filename, ".json")) {
        return httpd_resp_set_type(req, "application/json");
    }
    return httpd_resp_set_type(req, "text/plain");
}

/**
 * @brief 初始化 SPIFFS 文件系统
 */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "初始化 SPIFFS 文件系统");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "挂载或格式化 SPIFFS 失败");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "未找到 SPIFFS 分区");
        } else {
            ESP_LOGE(TAG, "SPIFFS 初始化失败: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS 分区大小: %d, 已使用: %d", total, used);
    }

    return ESP_OK;
}

/**
 * @brief 通用静态文件处理器 - 从 SPIFFS 读取文件
 */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;

    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "打开文件失败: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "文件未找到");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "读取文件失败: %s", filepath);
        } else if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "发送文件失败");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "发送文件失败");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    ESP_LOGI(TAG, "文件发送完成: %s", filepath);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief GET /api/config - 获取当前配置
 */
static esp_err_t api_config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON 创建失败");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "http_uri", s_http_uri);

    const char *json_str = cJSON_Print(root);
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/**
 * @brief POST /api/config - 设置配置
 * 请求体格式: {"http_uri": "https://example.com/api"}
 */
static esp_err_t api_config_post_handler(httpd_req_t *req)
{
    char buf[512];
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    if (total_len >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "请求内容过长");
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "接收数据失败");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    ESP_LOGI(TAG, "收到配置请求: %s", buf);

    // 解析 JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 解析失败");
        return ESP_FAIL;
    }

    // 获取 http_uri 字段
    cJSON *uri_item = cJSON_GetObjectItem(root, "http_uri");
    if (uri_item == NULL || !cJSON_IsString(uri_item)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "缺少 http_uri 字段");
        return ESP_FAIL;
    }

    const char *new_uri = uri_item->valuestring;
    if (strlen(new_uri) >= HTTP_URI_MAX_LEN) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI 过长");
        return ESP_FAIL;
    }

    // 更新配置
    strncpy(s_http_uri, new_uri, HTTP_URI_MAX_LEN - 1);
    s_http_uri[HTTP_URI_MAX_LEN - 1] = '\0';

    // 保存到 NVS
    esp_err_t err = save_uri_to_nvs(s_http_uri);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "保存配置失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HTTP URI 已更新为: %s", s_http_uri);

    // 返回成功响应
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"code\": 200,\"message\":\"配置已保存\"}");

    return ESP_OK;
}

esp_err_t http_server_init(void)
{
    // 初始化 SPIFFS 文件系统
    esp_err_t err = init_spiffs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS 初始化失败, HTTP 服务器无法启动");
        return err;
    }

    // 从 NVS 加载配置
    load_uri_from_nvs();

    // 分配服务器上下文
    s_rest_context = calloc(1, sizeof(rest_server_context_t));
    if (s_rest_context == NULL) {
        ESP_LOGE(TAG, "无法分配服务器上下文内存");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(s_rest_context->base_path, WEB_MOUNT_POINT, sizeof(s_rest_context->base_path));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    config.server_port = HTTP_SERVER_PORT;

    ESP_LOGI(TAG, "启动 HTTP 服务器，端口: %d", config.server_port);

    err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务器启动失败: %s", esp_err_to_name(err));
        free(s_rest_context);
        s_rest_context = NULL;
        return err;
    }

    // 注册 URI 处理器

    // API: 获取配置（放在通配符之前注册）
    httpd_uri_t api_config_get_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = api_config_get_handler,
        .user_ctx = s_rest_context
    };
    httpd_register_uri_handler(s_server, &api_config_get_uri);

    // API: 设置配置
    httpd_uri_t api_config_post_uri = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = api_config_post_handler,
        .user_ctx = s_rest_context
    };
    httpd_register_uri_handler(s_server, &api_config_post_uri);

    // 通配符处理器 - 处理所有静态文件请求（放在最后注册）
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = s_rest_context
    };
    httpd_register_uri_handler(s_server, &common_get_uri);

    ESP_LOGI(TAG, "HTTP 服务器已启动");
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(s_server);
    if (err == ESP_OK) {
        s_server = NULL;
        if (s_rest_context) {
            free(s_rest_context);
            s_rest_context = NULL;
        }
        esp_vfs_spiffs_unregister("storage");
        ESP_LOGI(TAG, "HTTP 服务器已停止");
    }
    return err;
}

const char* http_server_get_uri(void)
{
    return s_http_uri;
}

esp_err_t http_server_set_uri(const char *uri)
{
    if (uri == NULL || strlen(uri) >= HTTP_URI_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_http_uri, uri, HTTP_URI_MAX_LEN - 1);
    s_http_uri[HTTP_URI_MAX_LEN - 1] = '\0';

    return save_uri_to_nvs(s_http_uri);
}
