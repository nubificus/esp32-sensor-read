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

