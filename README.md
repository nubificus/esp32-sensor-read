# ESP32: Over the air update (OTA)

![image](https://github.com/nubificus/esp32-ota-update/assets/78209098/74dc71c5-9be3-4426-9f6f-1e3e2cd2c345)

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
2. Start a TCP Server, to listen for update request
3. Wait until it receives an update
4. Read the Firmware-Image through a TCP stream
5. **Reboot with the new firmware**


## Build

The following commands will build the project. The project must be built upon the device on which the ESP32 Microcontroller is connected.

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
export FIRMWARE_VERSION="0.1.0"
export DEVICE_TYPE="esp32s2"
export APPLICATION_TYPE="thermo"
idf.py build
idf.py flash monitor
```

**Create Docker image**

```bash
export FIRMWARE_VERSION="0.1.0"
export DEVICE_TYPE="esp32s2"
export APPLICATION_TYPE="thermo"
tee Dockerfile > /dev/null << 'EOT'
FROM scratch
COPY ./build/ota.bin /firmware/ota.bin
LABEL "com.urunc.iot.path"="/firmware/ota.bin"
EOT
docker build --push -t harbor.nbfc.io/nubificus/$APPLICATION_TYPE-$DEVICE_TYPE-firmware:$FIRMWARE_VERSION .
rm -f Dockerfile
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
## Firmware Provider App
Now, we have to build a client from which will trigger the microcontroller and provide the new firmware image, in order to accomplish the update. The following example is a C program which first performs a TCP connection with the ESP32 (Server), and afterwards sends the binary file as a stream of chunks:
```C
/* tcp_client.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.8.62"
#define SERVER_PORT 3333
#define CHUNK_SIZE  1024

void send_file(const char *filename) {
    int sock;
    struct sockaddr_in server_address;
    FILE *file;
    char buffer[CHUNK_SIZE];
    size_t bytes_read;

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Define the server address */
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);

    /* Connect to the server */
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error: Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* Open the file */
    file = fopen(filename, "rb");
    if (!file) {
        perror("Error: File opening failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* Read from the file and send it to the server in chunks */
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
	if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("Error: Failed to send data");
            break;
        }
    }

    printf("File %s sent successfully\n", filename);

    /* Close file and socket */
    fclose(file);
    close(sock);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    send_file(argv[1]);
    return 0;
}
```

Don't forget to change **SERVER_IP**. Then, you can build and run the program using the following commands:

```bash
gcc -o client tcp_client.c
./client /path/to/file.bin
```

## Simple Firmware to use for Update

Now we also need to create a simple firmware image, which will be sent by the server to update ESP32. We can do it using the following commands:

```bash
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
The new firmware image is located in `~/esp-idf/projects/dummy-firmware/build/dummy-firmware.bin`.
Therefore, you can use that file for the ota update by providing the path when running the client:
```
./client ~/esp-idf/projects/dummy-firmware/build/dummy-firmware.bin
```

## Multi-platform image building

```bash
git clone -b feat_http_server git@github.com:nubificus/esp32-ota-update.git
cd esp32-ota-update
mkdir -p dist/esp32s2
tee env.list > /dev/null << 'EOT'
FIRMWARE_VERSION=0.2.0
DEVICE_TYPE=esp32s2
APPLICATION_TYPE=thermo
EOT
docker run --rm -v $PWD:/project -w /project espressif/idf:latest idf.py set-target esp32s2
docker run --rm -v $PWD:/project -w /project --env-file ./env.list espressif/idf:latest idf.py build
sudo mv build/ota.bin dist/esp32s2/ota.bin

mkdir -p dist/esp32s3
tee env.list > /dev/null << 'EOT'
FIRMWARE_VERSION=0.2.0
DEVICE_TYPE=esp32s3
APPLICATION_TYPE=thermo
EOT
docker run --rm -v $PWD:/project -w /project espressif/idf:latest idf.py set-target esp32s3
docker run --rm -v $PWD:/project -w /project --env-file ./env.list espressif/idf:latest idf.py build
sudo mv build/ota.bin dist/esp32s3/ota.bin

mkdir -p dist/esp32
tee env.list > /dev/null << 'EOT'
FIRMWARE_VERSION=0.2.0
DEVICE_TYPE=esp32
APPLICATION_TYPE=thermo
EOT
docker run --rm -v $PWD:/project -w /project espressif/idf:latest idf.py set-target esp32
docker run --rm -v $PWD:/project -w /project --env-file ./env.list espressif/idf:latest idf.py build
sudo mv build/ota.bin dist/esp32/ota.bin

sudo chown -R $USER dist

tee Dockerfile > /dev/null << 'EOT'
FROM scratch
ARG DEVICE
COPY dist/${DEVICE}/ota.bin /firmware/ota.bin
LABEL "com.urunc.iot.path"="/firmware/ota.bin"
EOT

docker buildx build --platform custom/esp32 -t harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32 --build-arg DEVICE=esp32 . --push --provenance false
docker buildx build --platform custom/esp32s2 -t harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32s2 --build-arg DEVICE=esp32s2 . --push --provenance false
docker buildx build --platform custom/esp32s3 -t harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32s3 --build-arg DEVICE=esp32s3 . --push --provenance false

docker manifest create harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0 \
  --amend harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32 \
  --amend harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32s2 \
  --amend harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0-esp32s3

docker manifest push harbor.nbfc.io/nubificus/iot/esp32-thermo-firmware:0.2.0
```
