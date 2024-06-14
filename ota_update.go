package main

import (
	"fmt"
	"net"
	"os"
)

const (
	SERVER_IP   = "192.168.1.205"
	SERVER_PORT = 3333
	CHUNK_SIZE  = 1024
)

func launchOtaUpdate(firmwarePath string, ipAddr string, port int) {
	conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", ipAddr, port))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: Connection failed - %s\n", err)
		os.Exit(1)
	}
	defer conn.Close()

	file, err := os.Open(firmwarePath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: File opening failed - %s\n", err)
		os.Exit(1)
	}
	defer file.Close()

	buffer := make([]byte, CHUNK_SIZE)
	for {
		bytesRead, err := file.Read(buffer)
		if err != nil {
			break
		}
		if bytesRead > 0 {
			_, err = conn.Write(buffer[:bytesRead])
			if err != nil {
				fmt.Fprintf(os.Stderr, "Error: Failed to send data - %s\n", err)
				break
			}
		}
	}

	fmt.Printf("File %s sent successfully\n", firmwarePath)
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintf(os.Stderr, "Usage: %s <file_path>\n", os.Args[0])
		os.Exit(1)
	}

	launchOtaUpdate(os.Args[1], SERVER_IP, SERVER_PORT)
}

