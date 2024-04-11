idf_component_register(
	SRCS "ota.c" 
	INCLUDE_DIRS "." 
	REQUIRES esp_https_ota esp_partition app_update esp_netif esp_http_client esp_wifi nvs_flash
	EMBED_TXTFILES certs/ca_cert.pem
)


# set(COMPONENT_EMBED_TXTFILES "certs/ca_cert.pem" )	    
