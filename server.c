#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"

// Error code to return when cannot set up network connection
// via function getaddrinfo, bind(), listen(), getsockname()
const int undefinedErrorCode = 10;

//
int set_up_socket(const char* port, unsigned int* portNumber) {
    struct addrinfo* addressInfo = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    if (!port) {
        hints.ai_flags = AI_PASSIVE;
    }
    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &addressInfo))) {
        freeaddrinfo(addressInfo);
        fprintf(stderr, "%s\n", gai_strerror(err));
        exit(undefinedErrorCode);
    }

    // create a socket and bind it to a port
    int socketEndpoint = socket(AF_INET, SOCK_STREAM, 0);

    if (!port) { // server
        if (bind(socketEndpoint, (struct sockaddr*)addressInfo->ai_addr,
                sizeof(struct sockaddr))) {
            perror("Binding");
            exit(undefinedErrorCode);
        }
        // allow up to 10 connection requests to queue
        if (listen(socketEndpoint, 10)) {
            perror("Listen");
            exit(undefinedErrorCode);
        }
        struct sockaddr_in ad;
        memset(&ad, 0, sizeof(struct sockaddr_in));
        socklen_t len = sizeof(struct sockaddr_in);
        if (getsockname(socketEndpoint, (struct sockaddr*)&ad, &len)) {
            exit(undefinedErrorCode);
        }
        *portNumber = ntohs(ad.sin_port);
    } else { // client
        if (connect(socketEndpoint, (struct sockaddr*)addressInfo->ai_addr,
                sizeof(struct sockaddr))) {
            // return 0 when the client cannot to the server with given port
            socketEndpoint = 0; 
        }
    }

    return socketEndpoint;
}