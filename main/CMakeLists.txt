idf_component_register(
	SRCS "ota.c" 
	INCLUDE_DIRS "." 
	REQUIRES esp_partition app_update esp_netif esp_wifi nvs_flash
)
