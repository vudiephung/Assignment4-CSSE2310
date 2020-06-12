#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "socket.h"

// Error code to return when cannot set up network connection
// via function getaddrinfo(), bind(), listen(), getsockname()
const int undefinedErrorCode = 10;

// Set up socket endpoints and then bind
// (if server) or connect (if client) with it
// Args: Given the 'port' to get the address info,
// 'portNumber' is dereferenced to reuse
// In this assignment, if client, then 'port' != NULL, 'portNumer' == NULL
// If server, then set 'port' = NULL, 'portNumber' != NULL
// return file descriptor of socket endpoint
int set_up_socket(const char* port, unsigned int* portNumber) {
    struct addrinfo* addressInfo = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;

    if (!port) { // Server
        hints.ai_flags = AI_PASSIVE;
    }

    int error;
    if ((error = getaddrinfo("localhost", port, &hints, &addressInfo))) {
        freeaddrinfo(addressInfo);
        fprintf(stderr, "%s\n", gai_strerror(error));
        exit(undefinedErrorCode);
    }

    // create a socket and bind it to a port (if server)
    // or connect with it (if client)
    int socketEndpoint = socket(AF_INET, SOCK_STREAM, 0);

    if (!port) { // server
        if (bind(socketEndpoint, (struct sockaddr*)addressInfo->ai_addr,
                sizeof(struct sockaddr))) {
            perror("Binding");
            exit(undefinedErrorCode);
        }
        int maxQueuePending = 128; //maximum pending connections in queue
        if (listen(socketEndpoint, maxQueuePending)) {
            perror("Listen");
            exit(undefinedErrorCode);
        }

        struct sockaddr_in socketAddress;
        memset(&socketAddress, 0, sizeof(struct sockaddr_in));
        socklen_t length = sizeof(struct sockaddr_in);
        if (getsockname(socketEndpoint, (struct sockaddr*)&socketAddress,
                &length)) {
            exit(undefinedErrorCode);
        }
        *portNumber = ntohs(socketAddress.sin_port);
    } else { // client
        if (connect(socketEndpoint, (struct sockaddr*)addressInfo->ai_addr,
                sizeof(struct sockaddr))) {
            // return 0 when the client cannot to the server with given port
            socketEndpoint = 0; 
        }
    }

    return socketEndpoint;
}