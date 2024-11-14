#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_http_server.h"

esp_err_t info_get_handler(httpd_req_t *req);
esp_err_t temp_get_handler(httpd_req_t *req);

#endif // HTTP_SERVER_H
