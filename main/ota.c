#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_ota_ops.h"
#include "http_server.h"

#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define MAX_FAILURES 10
#define PORT 3333

wifi_config_t wifi_config = {
	.sta = {
		.ssid = WIFI_SSID,
		.password = WIFI_PASS,
		.threshold.authmode = WIFI_AUTH_WPA2_PSK,
		.pmf_cfg = {
			.capable = true,
			.required = false},
	},
};

static EventGroupHandle_t wifi_event_group;

static int s_retry_num = 0;

static const char *TAG = "main";

// Event handler for Wi-Fi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
							   int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ESP_LOGI(TAG, "Connecting to AP...");
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < MAX_FAILURES)
		{
			ESP_LOGI(TAG, "Reconnecting to AP...");
			esp_wifi_connect();
			s_retry_num++;
		}
		else
		{
			xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
		}
	}
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
							 int32_t event_id, void *event_data)
{

	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
	}
}

esp_err_t connect_wifi()
{
	int status = WIFI_FAILURE;

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_event_group = xEventGroupCreate();

	esp_event_handler_instance_t wifi_handler_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_event_handler,
														NULL,
														&wifi_handler_event_instance));

	esp_event_handler_instance_t got_ip_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&ip_event_handler,
														NULL,
														&got_ip_event_instance));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "STA initialization complete");

	EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
										   WIFI_SUCCESS | WIFI_FAILURE,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	if (bits & WIFI_SUCCESS)
	{
		ESP_LOGI(TAG, "Connected to ap");
		status = WIFI_SUCCESS;
	}
	else if (bits & WIFI_FAILURE)
	{
		ESP_LOGI(TAG, "Failed to connect to ap");
		status = WIFI_FAILURE;
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		status = WIFI_FAILURE;
	}

	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
	vEventGroupDelete(wifi_event_group);
	return status;
}

static esp_partition_t *update_partition = NULL;
static esp_ota_handle_t update_handle = 0;
static size_t partition_data_written = 0;

void ota_process_begin()
{
	update_partition = esp_ota_get_next_update_partition(NULL);
	assert(update_partition != NULL);

	esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "esp_ota_begin() failed (%s)", esp_err_to_name(err));
		esp_ota_abort(update_handle);
		while (1)
			vTaskDelay(1000);
	}

	ESP_LOGI(TAG, "esp_ota_begin succeeded");
}

void ota_append_data_to_partition(unsigned char *data, size_t len)
{
	if (esp_ota_write(update_handle, (const void *)data, len) != ESP_OK)
	{
		esp_ota_abort(update_handle);
		ESP_LOGE(TAG, "esp_ota_write() failed");
		while (1)
			vTaskDelay(1000);
	}
	partition_data_written += len;
}

void ota_setup_partition_and_reboot()
{
	ESP_LOGI(TAG, "Total bytes read: %d", partition_data_written);

	esp_err_t err = esp_ota_end(update_handle);
	if (err != ESP_OK)
	{
		if (err == ESP_ERR_OTA_VALIDATE_FAILED)
			ESP_LOGE(TAG, "Image validation failed, image is corrupted");
		else
			ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));

		while (1)
			vTaskDelay(1000);
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		while (1)
			vTaskDelay(1000);
	}
	ESP_LOGI(TAG, "Prepare to restart system!");

	vTaskDelay(4000 / portTICK_PERIOD_MS);

	esp_restart();
}

int ota_write_partition_from_tcp_stream()
{
	unsigned char rx_buffer[1024];
	char addr_str[128];
	int addr_family;
	int ip_protocol;

	struct sockaddr_in destAddr;
	destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(PORT);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;
	inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

	int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
	if (listen_sock < 0)
	{
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		vTaskDelete(NULL);
		return -1;
	}
	ESP_LOGI(TAG, "Socket created");

	int err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
	if (err != 0)
	{
		ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
		ESP_LOGE(TAG, "IPPROTO: %d", ip_protocol);
		close(listen_sock);
		vTaskDelete(NULL);
		return -1;
	}
	ESP_LOGI(TAG, "Socket bound, port %d", PORT);

	err = listen(listen_sock, 1);
	if (err != 0)
	{
		ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
		close(listen_sock);
		vTaskDelete(NULL);
		return -1;
	}

	ESP_LOGI(TAG, "Socket listening");
	struct sockaddr_in sourceAddr;
	uint addrLen = sizeof(sourceAddr);
	int sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
	if (sock < 0)
	{
		ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
		return -1;
	}

	ESP_LOGI(TAG, "Socket accepted");

	while (1)
	{
		int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
		if (len < 0)
		{
			ESP_LOGE(TAG, "recv failed: errno %d", errno);
			return -1;
		}
		else if (len == 0)
		{
			ESP_LOGI(TAG, "Connection closed");
			break;
		}
		else
			ota_append_data_to_partition(rx_buffer, len);
	}

	ESP_LOGI(TAG, "Closing the socket...");
	shutdown(sock, 0);
	close(sock);

	ESP_LOGI(TAG, "Now all the data has been received");

	return 0;
}

void ota_task(void *param)
{
		ota_process_begin();

		if (ota_write_partition_from_tcp_stream() < 0) {
			ESP_LOGE(TAG, "Failed to update");
			while (1) vTaskDelay(1000);
		}

		ota_setup_partition_and_reboot();

		vTaskDelete(NULL);
}

void http_server_task(void *pvParameters)
{
	httpd_handle_t server = NULL;

	// ESP_ERROR_CHECK(example_connect());

	server = start_webserver();

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

	// Keep the task running indefinitely
	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void app_main(void)
{
	esp_err_t status = WIFI_FAILURE;

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}

	ESP_ERROR_CHECK(ret);
	status = connect_wifi();
	if (WIFI_SUCCESS != status) {
		ESP_LOGI(TAG, "Failed to associate to AP, dying...");
		return;
	}

	// Create OTA task
	xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);

	// Create HTTP server task
	xTaskCreate(http_server_task, "http_server_task", 4096, NULL, 5, NULL);
}
