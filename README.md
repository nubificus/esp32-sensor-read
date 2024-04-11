# ESP32: Over the air update (OTA)

## Steps
1. ESP32 connects to WiFi
```c
/* Modify the credentials */
wifi_config_t wifi_config = {
  .sta = {
    .ssid = "nbfc-priv",
    .password = "Add password here",
    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    .pmf_cfg = {
      .capable = true,
      .required = false
    },
  },
};
```
2. Start an HTTP Server, to listen for update request
3. Wait until it receives an update
4. When it does, ask for the new firmware, from a predefined https address
```c
/* Example */
const char* firmware_url = "https://192.168.8.60:8000";
```
5. **Reboot with the new firmware**


## Build
