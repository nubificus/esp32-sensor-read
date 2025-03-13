#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "http_server.h"
#include "wifi.h"
#include "esp_wifi.h"

/* Components */
#include "ota-service.h"
#include "esp32-akri.h"

#define WIFI_SUCCESS 1 << 0

const char *TAG = "main";

void app_main(void)
{
	esp_err_t ret;

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ret = connect_wifi();
	if (WIFI_SUCCESS != ret) {
		ESP_LOGI(TAG, "Failed to associate to AP, dying...");
		return;
	}

	ret = akri_server_start();
	if (ret) {
		ESP_LOGE(TAG, "Cannot start akri server");
		abort();
	}

	ret = akri_set_update_handler(ota_request_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set OTA request handler");
		abort();
	}

	ret = akri_set_info_handler(info_get_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set info handler");
		abort();
	}

	ret = akri_set_temp_handler(temp_get_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set temp handler");
		abort();
	}

	ret = akri_set_onboard_handler(onboard_request_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set onboard handler");
		abort();
	}

	while (1) vTaskDelay(1000);
}
