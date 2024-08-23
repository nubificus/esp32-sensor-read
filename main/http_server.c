#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "http_server.h"
#include "esp_random.h"

// See here:
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/simple/main/main.c

esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char* resp_str = "hello world";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t temp_get_handler(httpd_req_t *req)
{
    char resp_str[10];
    int random_temp = esp_random() % 51; // Generate a random number between 0 and 50
    snprintf(resp_str, sizeof(resp_str), "%d", random_temp);
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t version_get_handler(httpd_req_t *req)
{
    const char* resp_str = "0.1.0-alpha";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t device_get_handler(httpd_req_t *req)
{
    const char* resp_str = "esp32s2";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}


httpd_uri_t hello = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

httpd_uri_t temp = {
    .uri       = "/temp",
    .method    = HTTP_GET,
    .handler   = temp_get_handler,
    .user_ctx  = NULL
};

httpd_uri_t version = {
    .uri       = "/version",
    .method    = HTTP_GET,
    .handler   = version_get_handler,
    .user_ctx  = NULL
};

httpd_uri_t device = {
    .uri       = "/device",
    .method    = HTTP_GET,
    .handler   = device_get_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &temp);
        httpd_register_uri_handler(server, &version);
        httpd_register_uri_handler(server, &device);
    }
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server) {
        httpd_stop(server);
    }
}

void disconnect_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        stop_webserver(*server);
        *server = NULL;
    }
}

void connect_handler(void* arg, esp_event_base_t event_base,
                     int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        *server = start_webserver();
    }
}


