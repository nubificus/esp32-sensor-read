#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "http_server.h"
#include "esp_random.h"

// See here:
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/simple/main/main.c

esp_err_t info_get_handler(httpd_req_t *req)
{
    char json_response[100];  // Adjust size based on your expected response length
    snprintf(json_response, sizeof(json_response),
                "{\"device\":\"%s\",\"application\":\"%s\",\"version\":\"%s\"}",
                DEVICE_TYPE, APPLICATION_TYPE, FIRMWARE_VERSION);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
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
