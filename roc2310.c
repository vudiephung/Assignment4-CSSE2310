#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "socket.h"
#include "utils.h"

// Default size to allocate a buffer
int defaultBufferSize = 80;

// List of destinations
typedef struct {
    int numOfDestinations;
    char** destinations; // store ports of destinations
} RocData;

// Define error code as enum
typedef enum {
    NUMS_OF_ARGS = 1,
    INVALID_MAPPER = 2,
    MAPPER_REQUIRED = 3,
    MAPPER_CONNECT = 4,
    MAPPER_ENTRY = 5,
    CONNECTION = 6
} Error;

// Print the corresponding message with 'type' to stderr and return that
// error code
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

// Save 'numOfDestinations' and all the destinations getting from the
// 'argv' to the 'rocData', beginning with argv['minimumAgrc'] which is
// argv[3]
// return void;
void initialize_roc(RocData* rocData, int numOfDestinations,
        int minimumAgrc, char** argv) {
    rocData->numOfDestinations = numOfDestinations;
    // Allocate memory for destinations and save it to rocData
    char** destinations = malloc(sizeof(char*) * numOfDestinations);
    rocData->destinations = destinations;

    // copy argvs into array
    for (int i = 0; i < numOfDestinations; i++) {
        rocData->destinations[i] =
                malloc(sizeof(char) * defaultBufferSize);
        strcpy(rocData->destinations[i], argv[i + minimumAgrc]);
    }
}

// Send request(s) to the 'mapperPort' (if its port is given) to ensure that
// all of the destinations provide valid port
// then save all of the converted destination port
// to the 'destinations' variable of 'rocData'.
// If the mapperPort is not provided, checking for error and return with
// coresponding exit code.
// return void if no error
void handle_ports(RocData* rocData, char* mapperPort) {
    int numOfDestinations = rocData->numOfDestinations;

    // If the mapper port is given
    if (is_valid_port(mapperPort)) {
        int client = set_up_socket(mapperPort, NULL);
        // unexisting mapper port
        if (!client) {
            exit(handle_error_message(MAPPER_CONNECT));
        }

        // get port of this destination
        FILE* writeFile = fdopen(client, "w");
        FILE* readFile = fdopen(client, "r");
        for (int i = 0; i < numOfDestinations; i++) {
            char* destination = rocData->destinations[i];
            if (!is_valid_port(destination)) { // invalid destination port
                // Send that invalid port to mapper to get the valid one
                fprintf(writeFile, "?%s\n", destination);
                fflush(writeFile);
                char* buffer = malloc(sizeof(char) * defaultBufferSize);
                if (!read_line(readFile, buffer, &defaultBufferSize)) {
                    exit(handle_error_message(MAPPER_ENTRY));
                }

                if (!strcmp(buffer, ";")) { // there is no corresponding port
                    exit(handle_error_message(MAPPER_ENTRY));
                }
                // copy the result given from mapper to the destinations
                strcpy(destination, buffer);
            }
        }

        // close connections
        fclose(writeFile);
        fclose(readFile);
        close(client);

    } else {
        if (!strcmp(mapperPort, "-")) {
            for (int i = 0; i < numOfDestinations; i++) {
                // for any destination port is invalid
                if (!is_valid_port(rocData->destinations[i])) {
                    exit(handle_error_message(MAPPER_REQUIRED));
                }
            }
        } else {
            exit(handle_error_message(INVALID_MAPPER));
        }
    }
}

// Connect to each destination ports that get from 'rocData'
// if the port cannot be connected, ignore it, connect to the others and
// exit with exit code.
// After each connection is success, write the 'planeId' to that destination
// return void;
void connect_ports(RocData* rocData, char* planeId) {
    int numOfDestinations = rocData->numOfDestinations;
    bool connectionError = false;
    for (int i = 0; i < numOfDestinations; i++) {
        // Connect to each port
        char* destinationPort = rocData->destinations[i];

        int client = set_up_socket(destinationPort, NULL); // for write
        int client2 = dup(client); // for read

        if (!client) { // cannot connect to at least 1 destination
            connectionError = true;
            continue;
        }

        FILE* writeFile = fdopen(client, "w");
        FILE* readFile = fdopen(client2, "r");

        fprintf(writeFile, "%s\n", planeId);
        fflush(writeFile);

        char* message = malloc(sizeof(char) * defaultBufferSize);
        read_line(readFile, message, &defaultBufferSize);
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
    const int minimumAgrc = 3; // including argv[0], plane id and mapperPort

    if (argc < minimumAgrc) {
        return handle_error_message(NUMS_OF_ARGS);
    }

    // Get args
    char* planeId = argv[1];
    char* mapperPort = argv[2];
    int numOfDestinations = argc - minimumAgrc;

    RocData* rocData = malloc(sizeof(RocData));
    initialize_roc(rocData, numOfDestinations, minimumAgrc, argv);

    // Handle list of ports
    handle_ports(rocData, mapperPort);

    // Connect
    connect_ports(rocData, planeId);

    return 0;
}