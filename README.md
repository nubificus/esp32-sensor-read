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

The following commands will build the project. The project must me built upon the device on which the ESP32 Microcontroller is connected.

**Download esp-idf source**
```
cd ~
git clone --recursive https://github.com/espressif/esp-idf.git
```
**Install and set the environment variables**
```
cd esp-idf
./install.sh
. ./export.sh
# You have to run the last command every time the environment variables are lost.
```
**Download the project**
```
mkdir projects && cd projects
git clone https://github.com/nubificus/esp32-ota-update.git
cd esp32-ota-update
```
**Build and Flash**
```
idf.py build
idf.py flash monitor
```

**You may have to define the port explicitly**
```
idf.py -p <PORT> flash monitor
# example: -p /dev/ttyUSB0
```

**Additionally, you may have to change user's rights**
```
sudo adduser <USER> dialout
sudo chmod a+rw <PORT>
```
**To exit ESP32 monitor**
```
Ctr + ]
```
## Firmware Server
Now, we have to build a server from which the microcontroller will be able to retrieve the new firmware image, in order to perform the update. The server must implement the HTTPS Protocol. The following piece of code is a Python script that gives access to the binary file:
```python
# ota-server.py

from http.server import BaseHTTPRequestHandler, HTTPServer
import os
import ssl

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'application/octet-stream')
            self.end_headers()
            with open('<PATH-TO-FIRMWARE-FILE.bin>', 'rb') as file:
                self.wfile.write(file.read())
        else:
            self.send_error(404, "File not found.")

def run(server_class=HTTPServer, handler_class=SimpleHTTPRequestHandler, port=8000):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    # Wrap the HTTPServer with SSL
    httpd.socket = ssl.wrap_socket(httpd.socket, keyfile='<PATH-TO-SERVER-KEY>', certfile='<PATH-TO-CERTIFICATE>', server_side=True)
    print(f'Starting https server on port {port}...')
    httpd.serve_forever()

if __name__ == '__main__':
    run()
```

Don't forget to change **Path-to-new-Firmware**, **Path-to-Server-Key**, **Path-to-Certificate** and (optionally) the server's **port**.

Afterwards, you can start the server using:
```
python ota-server.py
```

## Define Server's IP Address
Of course, as you saw above, we have to define the server's IP Address inside the source file of the firmware. Inside `~/esp-idf/projects/esp32-ota-update/main/ota.c`, modify the following line to hold the new firmware server's address:
```c
const char* firmware_url = "https://<NEW-ADDR>";
```
**Important!**
Don't forget to add the certificate file inside the firmware's repository. Copy the content of the certificate file you have generated using the HTTPS Configuration Procedure, and paste it in: `~/esp-idf/projects/esp32-ota-update/main/certs/ca_cert.pem`. 
## Simple Firmware to use for Update
Now we also need to create a simple firmware image, which will be sent by the server to update ESP32. We can do it using the following commands:
```
cd ~/esp-idf/projects
idf.py create-project dummy-firmware
cd dummy-firmware
```
Now replace `~/esp-idf/projects/dummy-firmware/main/dummy-firmware.c` with the following lines:
```c
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main(void)
{
	printf("---Hello from the Updated Firmware!---\n");
	while (1) {
		vTaskDelay(1000);
	}
}
```
**And Build it**
```
idf.py build
```
The new firmware image is located in `~/esp-idf/projects/dummy-firmware/build/dummy-firmware.bin`. We have to provide this path to the server above.
## Trigger the Update Remotely
Finally, after having followed all the previous steps, the microcontroller should be running the first firmware image, while the firmware-server should be waiting for a request. We can trigger the microcontroller to perform the Over-the-air Update using the following line from a local device:
```
curl http://<Esp32-IP>/update
```
The IP Address of the microcontroller will appear on the screen when you flash/monitor the board. So you can copy it from there. After running the command, the microcontroller should start updating itself.


