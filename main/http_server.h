#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"

esp_err_t hello_get_handler(httpd_req_t *req);
esp_err_t temp_get_handler(httpd_req_t *req);
esp_err_t version_get_handler(httpd_req_t *req);
esp_err_t device_get_handler(httpd_req_t *req);

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

void disconnect_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);
void connect_handler(void* arg, esp_event_base_t event_base,
                     int32_t event_id, void* event_data);

#endif // HTTP_SERVER_H