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


#include "esp_timer.h"
#include "mqtt_client.h"

static TickType_t last_motion_tick = 0;

esp_err_t tasks_http_handler(httpd_req_t *req);
static const char *TAG = "esp32 sensor";

#include "esp_http_server.h"
#include "cJSON.h"

static char mqtt_broker[128] = {0};
static char mqtt_topic[64] = {0};
static char mqtt_user[64] = {0};
static char mqtt_pass[64] = {0};

static esp_mqtt_client_handle_t mqtt_client = NULL;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_event_handler_cb(event_data);
}

esp_err_t mqtt_start_client() {
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }

    if (strlen(mqtt_broker) == 0) {
        ESP_LOGW(TAG, "MQTT broker URL is empty, cannot start client");
        return ESP_FAIL;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri =  mqtt_broker,
    // optionally: .verification = {...} if you use certs
    };


    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT client started with broker %s", mqtt_broker);
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t *req) {
    char content[512];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(content)) {
	httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }

    int read_len = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, content + read_len, remaining);
        if (ret <= 0) {
	    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
            return ESP_FAIL;
        }
        read_len += ret;
        remaining -= ret;
    }
    content[read_len] = '\0';

    ESP_LOGI(TAG, "Received config: %s", content);

    cJSON *root = cJSON_Parse(content);
    if (!root) {
        ESP_LOGE(TAG, "Invalid JSON");
	httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Json");
        return ESP_FAIL;
    }

    cJSON *broker = cJSON_GetObjectItem(root, "mqtt_broker");
    cJSON *topic = cJSON_GetObjectItem(root, "mqtt_topic");
    cJSON *user = cJSON_GetObjectItem(root, "mqtt_user");
    cJSON *pass = cJSON_GetObjectItem(root, "mqtt_pass");

    if (!broker || !cJSON_IsString(broker)) {
        ESP_LOGE(TAG, "mqtt_broker missing or invalid");
        cJSON_Delete(root);
	httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "mqtt_broker missing or invalid");
        return ESP_FAIL;
    }

    if (!topic || !cJSON_IsString(topic)) {
        ESP_LOGE(TAG, "mqtt_topic missing or invalid");
        cJSON_Delete(root);
	httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "mqtt_topic missing or invalid");
        return ESP_FAIL;
    }

    strncpy(mqtt_broker, broker->valuestring, sizeof(mqtt_broker) - 1);
    mqtt_broker[sizeof(mqtt_broker) - 1] = 0;

    strncpy(mqtt_topic, topic->valuestring, sizeof(mqtt_topic) - 1);
    mqtt_topic[sizeof(mqtt_topic) - 1] = 0;

    if (user && cJSON_IsString(user)) {
        strncpy(mqtt_user, user->valuestring, sizeof(mqtt_user) - 1);
        mqtt_user[sizeof(mqtt_user) - 1] = 0;
    } else {
        mqtt_user[0] = 0;
    }

    if (pass && cJSON_IsString(pass)) {
        strncpy(mqtt_pass, pass->valuestring, sizeof(mqtt_pass) - 1);
        mqtt_pass[sizeof(mqtt_pass) - 1] = 0;
    } else {
        mqtt_pass[0] = 0;
    }

    cJSON_Delete(root);

    if (mqtt_start_client() != ESP_OK) {
	httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "MQTT config updated\n");
    return ESP_OK;
}

void mqtt_announce_shutdown(void) {
    char topic_str[32];
    snprintf(topic_str, sizeof(topic_str), "%s/%s", DEVICE_TYPE, "alive");
    if (mqtt_client) {
	vTaskDelay(pdMS_TO_TICKS(4000));  // Set FPS
        esp_mqtt_client_publish(mqtt_client, topic_str, "0", 0, 1, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void mqtt_announce_online(void *param) {
	char topic_str[32];
	snprintf(topic_str, sizeof(topic_str), "%s/%s", DEVICE_TYPE, "alive");

	while (1) {
		if (mqtt_client) {
		      esp_mqtt_client_publish(mqtt_client, topic_str, "1", 0, 1, 0);
		}
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}


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

	ret = akri_set_handler_generic("/config", HTTP_POST, config_post_handler);
	if (ret) {
		ESP_LOGE(TAG, "Cannot set mqtt config handler");
		abort();
	}

	ret = akri_set_handler_generic("/tasks", HTTP_GET, tasks_http_handler);
        if (ret) {
                ESP_LOGE(TAG, "Cannot set stream handler");
                abort();
        }

	esp_register_shutdown_handler(mqtt_announce_shutdown);
	int task_res = xTaskCreate(mqtt_announce_online, "mqtt_announce", 4096, NULL, 3, NULL);
	if (task_res != pdPASS) {
		ESP_LOGE(TAG, "Failed to create mqtt announce task");
	}

	ESP_LOGI(TAG, "System ready\n");

	srand(time(NULL));  // Seed RNG once
	while (1) {
		char temp_str[16];
		float temp_c = atoi(TEMP_START) * 1.0 + ((float)(rand() % 1000)) / 100.0f;  // 20.00 to 29.99
		snprintf(temp_str, sizeof(temp_str), "%.2f", temp_c);								   

		//int temperature = read_chip_temperature();
		// Publish MQTT message
		if (mqtt_client) {
		      esp_mqtt_client_publish(mqtt_client, mqtt_topic, temp_str, 0, 1, 0);
		}

		vTaskDelay(pdMS_TO_TICKS(1000));  // Set FPS
	}
}
