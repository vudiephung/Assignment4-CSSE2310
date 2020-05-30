#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include "server.h"

int set_up(const char* port) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;  // IPv4  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    if (!port) {
        hints.ai_flags = AI_PASSIVE; // Because we want to bind with it    
    }
    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &ai))) {
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return 1; // could not work out the address
    }

    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol

    if (!port) { // server
        if (bind(serv, (struct sockaddr*)ai->ai_addr,
                sizeof(struct sockaddr))) {
            // perror("Binding");
            // return 3;
        }

        if (listen(serv, 10)) { // allow up to 10 connection requests to queue
            // perror("Listen");
            // return 4;
        }
    } else { // client
        if (connect(serv, (struct sockaddr*)ai->ai_addr,
                sizeof(struct sockaddr))) {
            return 0;
        }
    }

    return serv;
}