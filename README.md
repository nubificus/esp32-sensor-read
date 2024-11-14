# ESP32: Over the air update (OTA)

## Build

The following commands will build the project. The project must be built upon the device on which the ESP32 Microcontroller is connected.

**Download esp-idf source**
```bash
cd ~
git clone --recursive https://github.com/espressif/esp-idf.git
```
**Install and set the environment variables**
```bash
cd esp-idf
./install.sh
. ./export.sh
# You have to run the last command every time the environment variables are lost.
```
**Download the project**
```bash
mkdir projects && cd projects
git clone https://github.com/nubificus/esp32-ota-update.git --recursive
cd esp32-ota-update
```
**Security Configuration**
If you want to use the secure implementation, set the `OTA_SECURE` environment variable before building. Otherwise, the default configuration is the non-secure.
```bash
export OTA_SECURE=1
```
**Build and Flash**
```bash
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
```bash
idf.py -p <PORT> flash monitor
# example: -p /dev/ttyUSB0
```

**Additionally, you may have to change user's rights**
```bash
sudo adduser <USER> dialout
sudo chmod a+rw <PORT>
```
**To exit ESP32 monitor**
```
Ctr + ]
```
## Firmware Provider App
### Non-secure implementation
In the case of the non-secure implementation, the microcontroller operates as a server, after receiving the post request. Therefore, we need a tcp client to operate as the firmware provider for the microcontroller. The following C program can do this job.
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
### Secure implementation
If you build with `OTA_SECURE`, you will need to check the more advanced OTA Agent implementation, which works with DICE certificates and TLS connection. For more information, view the [repository](https://github.com/nubificus/ota-agent) or the [documentation](https://github.com/nubificus/akri-ecosystem-hub/blob/main/doc/ota-agent.md).

## Simple Firmware to use for Update

Now we also need to create a simple firmware image, which will be sent by the server to update ESP32. You can use the `hello-world` example, located in `~/esp-idf/examples/get-started/hello_world/`. Build with the following commands:

```bash
cd ~/esp-idf/examples/get-started/hello_world/
idf.py build
```
The new firmware image is located in `~/esp-idf/examples/get-started/hello_world/build/hello_world.bin`.
You can use that file for the ota update by providing the path when running the client.

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

## Generate device info offline

### MAC Address

The mac address of an `esp32` device can be parsed using the `esptool.py read_mac` command. If we connect a device to the host and run the command, we are probably getting an output like:
```bash
$ esptool.py read_mac
esptool.py v4.8.dev3
Found 1 serial ports
Serial port /dev/ttyACM0
Connecting....
Detecting chip type... Unsupported detection protocol, switching and trying again...
Connecting....
Detecting chip type... ESP32
Chip is ESP32-D0WD-V3 (revision v3.1)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: e4:65:b8:83:a6:f4
Uploading stub...
Running stub...
Stub running...
MAC: e4:65:b8:83:a6:f4
Hard resetting via RTS pin...
```

Furthermore, if we would like to automate the generation of device-info file, we would probably like to extract only the mac address, without the rest of the output. We can do this with the following command:
```bash
$ esptool.py read_mac | grep "MAC:" | awk '{print $2}' | head -n 1
e4:65:b8:83:a6:f4
```

### Application Hash

The Application hash is saved at offset `0x200b0` of the binary application file (say, `app.bin`) and its length is 16 bytes. Thus we can extract it by running:

```bash
$ xxd -p -s 176 -l 16 build/app.bin | sed 's/../&:/g; s/:$//'
16:20:ad:db:90:4b:5e:71:fd:2a:5c:8c:ba:8d:9c:f3
```

### Bootloader Hash

Finally, the bootloader hash extraction process is more complex. The bootloader hash can be found at the offset of `0x80` in the flash memory and its length is 64 bytes. However, we cannot find it directly at `bootloader.bin` or `bootloader-reflash-digest.bin`. Instead, we follow the instructions below:

 - Save the first 128 bytes of `bootloader-reflash-digest.bin` (the `IV`) in a new file called `iv.bin`:

```bash
$ dd if=build/bootloader/bootloader-reflash-digest.bin of=iv.bin bs=1 count=128
```

 - Generate the bootloader's digest using `espsecure.py digest_secure_bootloader` with `iv.bin`:

```bash
$ espsecure.py digest_secure_bootloader --iv iv.bin --keyfile build/bootloader/secure-bootloader-key-256.bin --output bootloader-digest.bin build/bootloader/bootloader.bin
```

 - and get the 64 bytes by running:

```bash
$ xxd -p -c 64 -s 128 -l 64 bootloader-digest.bin | sed 's/../&:/g; s/:$//'
19:5a:b3:71:25:2c:d2:75:ff:68:05:d6:b5:01:fb:6e:cd:29:df:8e:d8:a6:73:5a:00:1a:a1:49:5f:f8:5b:5f:b7:9b:32:54:55:11:d3:06:33:b4:ec:7a:79:0e:10:28:f3:73:bc:8a:32:e5:2b:fd:20:f7:40:9b:54:c3:72:67
```

 - and finally you can remove the temporary files:
```bash
rm iv.bin bootloader-digest.bin
```

## Script

We can also use a script (`info.sh`) to get all that information at once. Then we can just append the output to a file, which can be used later by the `ota-agent`. The script receives as an argument the path of the `esp-idf` project, and prints a line with the credentials:

```bash
#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_esp_idf_project>"
    exit 1
fi

PROJECT_PATH=$1
cd "$PROJECT_PATH" || { echo "Directory not found: $PROJECT_PATH"; exit 1; }

. ~/esp-idf/export.sh > /dev/null

MAC=$(esptool.py read_mac | grep "MAC:" | awk '{print $2}' | head -n 1)

PROJECT_NAME=$(basename "$PWD")
APP_FILE="build/${PROJECT_NAME}.bin"

APP_HASH=$(xxd -p -s 176 -l 16 "$APP_FILE" | sed 's/../&:/g; s/:$//')

dd if=build/bootloader/bootloader-reflash-digest.bin of=iv.bin bs=1 count=128 &>/dev/null
espsecure.py digest_secure_bootloader --iv iv.bin --keyfile build/bootloader/secure-bootloader-key-256.bin --output bootloader-digest.bin build/bootloader/bootloader.bin > /dev/null
BOOTLOADER_HASH=$(xxd -p -c 64 -s 128 -l 64 bootloader-digest.bin | sed 's/../&:/g; s/:$//')

rm iv.bin bootloader-digest.bin

echo "$MAC $APP_HASH $BOOTLOADER_HASH"
```

### Example

```bash
$ bash info.sh /path/to/esp-idf-project
e4:65:b8:83:a6:f4 b5:5c:42:92:93:32:97:85:5f:a7:0f:47:21:ea:eb:18 19:5a:b3:71:25:2c:d2:75:ff:68:05:d6:b5:01:fb:6e:cd:29:df:8e:d8:a6:73:5a:00:1a:a1:49:5f:f8:5b:5f:b7:9b:32:54:55:11:d3:06:33:b4:ec:7a:79:0e:10:28:f3:73:bc:8a:32:e5:2b:fd:20:f7:40:9b:54:c3:72:67
```

or we can directly append the line to `boards.txt` (file read by the agent):
```bash
$ bash info.sh /path/to/project >> /path/to/boards.txt
```
