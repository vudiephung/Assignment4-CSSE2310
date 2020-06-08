#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include "utils.h"
#include "socket.h"

// default size to allocate a buffer
int defaultBufferSize = 80;

// Struct that contains args to pass to thread function that connects to
// mapper and send 'id' and 'controlPort' of current control
typedef struct {
    char* id;
    char* controlPort;
    char* mapperPort;
} MapperArgs;

// Essential information of a control
typedef struct {
    char* info; // info of the control
    int capacity; // Capacity of 'planes' array, could be extended
    int numberOfPlanes;
    char** planes;
} ControlData;

// When a client connect to the control server, create a new thread,
// run a function with the parameter of this struct
typedef struct {
    int connectFile; // fd of socket endpoint
    ControlData* controlData;
    sem_t* lock; // semaphore to handle race condition
} ThreadData;

// Define error exit code
typedef enum {
    NUMS_OF_ARGS = 1,
    INVALID_CHAR = 2,
    INVALID_PORT = 3,
    INVALID_MAPPER = 4
} Error;

// Print to stderr with the corresponding message of the 'type'
// and return that error code
Error handle_error_message(Error type) {
    const char* errorMessage = "";
    switch (type) {
        case NUMS_OF_ARGS:
            errorMessage = "Usage: control2310 id info [mapper]";
            break;
        case INVALID_CHAR:
            errorMessage = "Invalid char in parameter";
            break;
        case INVALID_PORT:
            errorMessage = "Invalid port";
            break;
        case INVALID_MAPPER:
            errorMessage = "Can not connect to map";
            break;
        default:
            break;
    }
    fprintf(stderr, "%s\n", errorMessage);
    return type;
}

// Sort the 'planes' with its 'length' in lexicographic order
// return void;
void lexicographic_order(char** planes, int length) {
    char tempBuffer[defaultBufferSize];
    for (int i = 0; i < length; i++) {
        for (int j = i + 1; j < length; j++) {
            //swapping strings if they are not in the lexicographical order
            if (strcmp(planes[i], planes[j]) > 0) {
                strcpy(tempBuffer, planes[i]);
                strcpy(planes[i], planes[j]);
                strcpy(planes[j], tempBuffer);
            }
        }
    }
}

// Add a new plane id getting from the 'buffer' to 'controlData' (including
// lexicographical sorting). Use the 'lock' to access critical section
// Return void;
void handle_add(char* buffer, ControlData* controlData, sem_t* lock) {
    int* numberOfPlanes = &controlData->numberOfPlanes;
    int* capacity = &controlData->capacity;

    // calc the length of the Plane ID
    size_t length = strlen(buffer);

    char* plane = malloc(sizeof(char) * length);
    for (int i = 0; i < length + 1; i++) {
        plane[i] = buffer[i];
    }

    sem_wait(lock);
    if (*numberOfPlanes + 2 > *capacity) {
        int biggerSize = (*capacity) * 1.5;
        char** newPlanes = (char**)realloc(controlData->planes,
                sizeof(char*) * biggerSize);
        if (newPlanes == 0) {
            return;
        }
        *capacity = biggerSize;
        controlData->planes = newPlanes;
    }

    controlData->planes[(*numberOfPlanes)++] = plane;
    lexicographic_order(controlData->planes, *numberOfPlanes);
    sem_post(lock);
}

// Getting command from 'buffer', handle with it via info from 'controlData'
// and write to socket end point with 'writeFile'
// Pass the 'lock' into its inner function to handle with sentitive data
// Return void;
void handle_command(char* buffer, ControlData* controlData, FILE* writeFile,
        sem_t* lock) {
    if (!strcmp(buffer, "log")) {
        // send list
        for (int i = 0; i < controlData->numberOfPlanes; i++) {
            fprintf(writeFile, "%s\n", controlData->planes[i]);
            fflush(writeFile);
        }
        fprintf(writeFile, ".\n");
        fflush(writeFile);
        return;
    }

    handle_add(buffer, controlData, lock);
    fprintf(writeFile, "%s\n", controlData->info);
    fflush(writeFile);
}

// Thread function to handle request from a client with given parameter
// Return NULL pointer
void* handle_request(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    int connectFile = threadData->connectFile;
    ControlData* controlData = threadData->controlData;
    sem_t* lock = threadData->lock;

    FILE* readFile = fdopen(connectFile, "r");
    FILE* writeFile = fdopen(connectFile, "w");
    char* buffer = malloc(sizeof(char) * defaultBufferSize);

    // Get command
    while (true) {
        if (read_line(readFile, buffer, &defaultBufferSize)) {
            handle_command(buffer, controlData, writeFile, lock);
        } else {
            break;
        }
    }

    // clean up
    free(buffer);
    fclose(readFile);
    fclose(writeFile);

    return 0;
}

// Set up the controlData with the 'info' and accept client connections
// via the socket fd 'server'. Return void;
void accept_clients(char* info, int server) {
    // Set up struct
    ControlData* controlData = malloc(sizeof(ControlData));
    int capacity = 10; // default value, could be extended if overloading
    char** planes = malloc(sizeof(char*) * capacity);
    controlData->info = info;
    controlData->capacity = capacity;
    controlData->numberOfPlanes = 0;
    controlData->planes = planes;

    sem_t lock;
    sem_init(&lock, 0, 1);

    pthread_t threadId;
    int connectFile;
    while (connectFile = accept(server, 0, 0), connectFile >= 0) {
        ThreadData* threadData = malloc(sizeof(ThreadData));
        threadData->connectFile = connectFile;
        threadData->controlData = controlData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }
}

// Thread function that does the job of connecting to mapper with given
// args. Return NULL pointer
void* connect_mapper(void* data) {
    MapperArgs* mapperArgs = (MapperArgs*)data;
    char* id = mapperArgs->id;
    char* controlPort = mapperArgs->controlPort;
    char* mapperPort = mapperArgs->mapperPort;

    // set up as client connect to mapperPort
    int client = set_up_socket(mapperPort, 0);
    if (!client) { // cannot connect with given port
        exit(handle_error_message(INVALID_MAPPER));
    }

    FILE* writeFile = fdopen(client, "w");

    fprintf(writeFile, "!%s:%s\n", id, controlPort);
    fflush(writeFile);

    // close the connection
    fclose(writeFile);
    close(client);

    return 0;
}

// Send the 'id' and 'controlPort' to the mapper via 'mapperPort'
// return void;
void handle_mapper(char* mapperPort, char* id, unsigned int controlPort) {
    if (!is_valid_port(mapperPort)) {
        exit(handle_error_message(INVALID_PORT));
    }
    // Client job
    MapperArgs* mapperArgs = malloc(sizeof(mapperArgs));
    mapperArgs->id = id;
    mapperArgs->controlPort = number_to_string(controlPort);
    mapperArgs->mapperPort = mapperPort;

    pthread_t threadId;
    pthread_create(&threadId, 0, connect_mapper, (void*)mapperArgs);
    pthread_join(threadId, 0);
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) { // only accept 2 args (without mapper)
                                  // or 3 args (argc = 4) (with mapper)
        return handle_error_message(NUMS_OF_ARGS);
    }

    char* id = argv[1];
    char* info = argv[2];

    if (!is_valid_text(id) || !is_valid_text(info)) {
        return handle_error_message(INVALID_CHAR);
    }

    // Set up socket and get the control port to pass to mapper and
    // fprintf to stdout
    unsigned int controlPort;
    int server = set_up_socket(0, &controlPort);

    if (argv[3]) { // if the mapper port is given
        char* mapperPort = argv[3];
        handle_mapper(mapperPort, id, controlPort);
    }

    fprintf(stdout, "%u\n", controlPort);
    fflush(stdout);

    accept_clients(info, server);

    return 0;
}