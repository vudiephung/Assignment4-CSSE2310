#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "server.h"
#include "utils.h"

int defaultBufferSize = 80;
// int defaultPortSize = 5; // Max digits of port number is 5 (65536)

typedef struct { // List of destinations
    int numOfDestinations;
    char** destinations; // store port number of destinations
} RocData;

typedef enum {
    NUMS_OF_ARGS = 1,
    INVALID_MAPPER = 2,
    MAPPER_REQUIRED = 3,
    MAPPER_CONNECT = 4,
    MAPPER_ENTRY = 5,
    CONNECTION = 6
} Error;

Error handle_error_message(Error type) {
    const char* errorMessage = "";
    switch (type) {
        case NUMS_OF_ARGS:
            errorMessage = "Usage: roc2310 id mapper {airports}";
            break;
        case INVALID_MAPPER:
            errorMessage = "Invalid mapper port";
            break;
        case MAPPER_REQUIRED:
            errorMessage = "Mapper required";
            break;
        case MAPPER_CONNECT:
            errorMessage = "Failed to connect to mapper";
            break;
        case MAPPER_ENTRY:
            errorMessage = "No map entry for destination";
            break;
        case CONNECTION:
            errorMessage = "Failed to connect to at least one destination";
            break;
        default:
            break;
    }
    fprintf(stderr, "%s\n", errorMessage);
    return type;
}

void set_up(RocData* rocData , int numOfDestinations, char** argv) {
    // Setup struct
    char** destinations = malloc(sizeof(char*) * numOfDestinations);
    rocData->numOfDestinations = numOfDestinations;
    rocData->destinations = destinations;

    // copy argvs into array
    for (int i = 0; i < numOfDestinations; i++) {
        // size_t length = strlen(argv[i + 3]) + 1;
        rocData->destinations[i] =
                malloc(sizeof(char) * defaultBufferSize);
        strcpy(rocData->destinations[i], argv[i + 3]);
    }
}

void handle_ports(RocData* rocData, char* mapperPort) {
    int numOfDestinations = rocData->numOfDestinations;
    // Get list of Ports
    if (is_valid_port(mapperPort)) {
        int client = set_up_socket(mapperPort);
        // unexisting mapper port
        if (!client) {
            exit(handle_error_message(MAPPER_CONNECT));
        }
        // get port of this destination
        FILE* writeFile = fdopen(client, "w");
        FILE* readFile = fdopen(client, "r");

        for (int i = 0; i < numOfDestinations; i++) {
            char* destination = rocData->destinations[i];
            if (!is_valid_port(destination)) {
                fprintf(writeFile, "?%s\n", destination);
                fflush(writeFile);
                char* buffer = malloc(sizeof(char) * defaultBufferSize);
                read_line(readFile, buffer, &defaultBufferSize, 0); ///
                // fprintf(stdout, "%s\n", buffer);
                strcpy(destination, buffer);
                if (!strcmp(buffer, ";")) {
                    exit(handle_error_message(MAPPER_ENTRY));
                }
            }
        }

        // close connections
        fclose(writeFile);
        fclose(readFile);
        close(client);

    } else {
        if (!strcmp(mapperPort, "-")) {
            for (int i = 0; i < numOfDestinations; i++) {
                if (!is_valid_port(rocData->destinations[i])) {
                    exit(handle_error_message(MAPPER_REQUIRED));
                }
            }
        } else {
            exit(handle_error_message(INVALID_MAPPER));
        }
    }
}

void connect_ports(RocData* rocData, char* planeId) {
    int numOfDestinations = rocData->numOfDestinations;
    bool connectionError = false;
    for (int i = 0; i < numOfDestinations; i++) {
        // Connect to each port
        char* destinationPort  = rocData->destinations[i];
        // printf("%s\n", destinationPort);
        int client = set_up_socket(destinationPort);
        if (!client) {
            connectionError = true;
            continue;
        }
        int client2 = dup(client);
        FILE* writeFile = fdopen(client, "w");
        FILE* readFile = fdopen(client2, "r");

        fprintf(writeFile, "%s\n", planeId);
        fflush(writeFile);
        char* message = malloc(sizeof(char) * defaultBufferSize);
        read_line(readFile, message, &defaultBufferSize, 0);
        fprintf(stdout, "%s\n", message);
        fflush(stdout);

        close(client);
        close(client2);
    }

    if (connectionError) {
        exit(handle_error_message(CONNECTION));
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return handle_error_message(NUMS_OF_ARGS);
    }

    // Get args
    char* planeId = argv[1];
    char* mapperPort = argv[2];
    int numOfDestinations = argc - 3;

    RocData* rocData = malloc(sizeof(RocData));
    set_up(rocData, numOfDestinations, argv);

    // Handle list of ports
    handle_ports(rocData, mapperPort);

    // Connect
    connect_ports(rocData, planeId);

    return 0;
}