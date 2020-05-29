#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "utils.h"
#include "server.h"

int defaultBufferSize = 80;

typedef struct {
    char* id;
    char* controlPort;
    char* mapperPort;
} ControlData;

typedef enum {
    OK = 0,
    NUMS_OF_ARGS = 1,
    INVALID_CHAR = 2,
    INVALID_PORT = 3,
    INVALID_MAP = 4
} Error;

Error handle_error_message(Error type) {
    const char* errorMessage = "";
    switch (type) {
        case OK:
            return OK;
        case NUMS_OF_ARGS:
            errorMessage = "Usage: control2310 id info [mapper]";
            break;
        case INVALID_CHAR:
            errorMessage = "Invalid char in parameter";
        case INVALID_PORT:
            errorMessage = "Invalid port";
        case INVALID_MAP:
            errorMessage = "Can not connect to map";
        default:
            break;
    }
    fprintf(stderr, "%s\n", errorMessage);
    return type;
}

void* handle_client_job(void* data) {
    ControlData* controlData = (ControlData*)data;
    char* id = controlData->id;
    char* controlPort = controlData->controlPort;
    char* mapperPort = controlData->mapperPort;

    int client = set_up(mapperPort); // set up as client connect to mapperPort
    FILE* writeFile = fdopen(client, "w");
    
    fprintf(writeFile, "!%s:%s\n", id, controlPort);
    fflush(writeFile);

    // close the connection
    

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return handle_error_message(NUMS_OF_ARGS);
    }

    char* id = argv[1];
    char* info = argv[2];
    int mapperPort; // cannot be used !!!!!

    if (!is_valid_id(id) || !is_valid_id(info)) {
        return handle_error_message(INVALID_CHAR);
    }

    // error 4?

    // Server
    int conn_fd;
    int server = set_up(0);
    // Which port did we get?
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(server, (struct sockaddr*)&ad, &len)) {
        // perror("sockname");
        // return 4;
    }
    unsigned int controlPort = ntohs(ad.sin_port);
    printf("%u\n", controlPort);

    if (argv[3]) {
        if (!is_valid_port(argv[3], &mapperPort)) {
            return handle_error_message(INVALID_PORT);
        }
        // Client
        ControlData* controlData = malloc(sizeof(controlData));
        controlData->id = id;
        controlData->controlPort = number_to_string(controlPort);
        controlData->mapperPort = argv[3];

        pthread_t threadId;
        pthread_create(&threadId, 0, handle_client_job, (void*)controlData);
    }

    while (conn_fd = accept(server, 0, 0), conn_fd >= 0) { // change 0, 0 to get info about other end
        FILE* readFile = fdopen(conn_fd, "r");
        FILE* writeFile = fdopen(conn_fd, "w");
        char* buffer = malloc(sizeof(char) * defaultBufferSize);

        // Get command
        while (true) {
            if (read_line(readFile, buffer, &defaultBufferSize)) {
                // handle_command(buffer, mapData, writeFile, lock);
            } else {
                // printf("Disconnected\n");
                break;
            }
        }

        // clean up
        free(buffer);
        fclose(readFile);
        fclose(writeFile);
    }
}