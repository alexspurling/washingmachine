
#include "esp_system.h"
#include "esp_log.h"
#include <lwip/sockets.h>
#include <errno.h>

//Socket stuff
#define PORT_NUMBER 8001

static const char *TAG = "server";

int socket_send(int clientSocket, char* data, int len, int foo) {
    return send(clientSocket, data, len, 0);
}

void start_server(void *args) {
	struct sockaddr_in clientAddress;
	struct sockaddr_in serverAddress;

  printf("Creating socket\n");
	// Create a socket that we will listen upon.
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
    printf("Error creating socket\n");
		ESP_LOGE(TAG, "socket: %d %s", sock, strerror(errno));
		goto END;
	}

  printf("Binding to port\n");
	// Bind our server socket to a port.
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT_NUMBER);
	int rc  = bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc < 0) {
    printf("Error binding socket\n");
		ESP_LOGE(TAG, "bind: %d %s", rc, strerror(errno));
		goto END;
	}

  printf("Listening for data\n");
	// Flag the socket as listening for new connections.
	rc = listen(sock, 5);
	if (rc < 0) {
    printf("Error listening to socket\n");
		ESP_LOGE(TAG, "listen: %d %s", rc, strerror(errno));
		goto END;
	}

  bool reconnect = true;
  while (reconnect) {

    printf("Accepting connection\n");
  	// Listen for a new client connection.
  	socklen_t clientAddressLength = sizeof(clientAddress);
  	int clientSock = accept(sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
  	if (clientSock < 0) {
      printf("Error accepting connection %d %d\n", clientSock, errno);
  		ESP_LOGE(TAG, "accept: %d %s", clientSock, strerror(errno));
  		goto END;
  	}

    printf("Accepted connection\n");
    reconnect = client_connected(clientSock);

    printf("Closing client connection\n");
  	rc = close(clientSock);
  	if (rc < 0) {
      printf("Error closing client connection\n");
  		ESP_LOGE(TAG, "Error closing client connection: %d %s", rc, strerror(errno));
  		goto END;
  	}
  }

  printf("Closing socket\n");
	rc = close(sock);
	if (rc < 0) {
    printf("Error closing socket\n");
		ESP_LOGE(TAG, "Error closing socket: %d %s", rc, strerror(errno));
		goto END;
	}

	END:
  printf("Exiting server\n");
	vTaskDelete(NULL);
}
