#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include "utils.h"
#include "server.h"

// default size to allocate a buffer
int defaultBufferSize = 80;

//
typedef struct {
    char* id;
    char* controlPort;
    char* mapperPort;
} MapperArgs;

//
typedef struct {
    char* info;
    int capacity;
    int numberOfPlanes;
    char** planes;
} ControlData;

//
typedef struct {
    int conn_fd;
    ControlData* controlData;
    sem_t* lock;
} ThreadData;

//
typedef enum {
    NUMS_OF_ARGS = 1,
    INVALID_CHAR = 2,
    INVALID_PORT = 3,
    INVALID_MAPPER = 4
} Error;

//
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

//
void* handle_client_job(void* data) {
    MapperArgs* mapperArgs = (MapperArgs*)data;
    char* id = mapperArgs->id;
    char* controlPort = mapperArgs->controlPort;
    char* mapperPort = mapperArgs->mapperPort;

    // set up as client connect to mapperPort
    int client = set_up_socket(mapperPort, NULL);
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

//
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

//
void handle_add(char* buffer, ControlData* controlData) {
    int* numberOfPlanes = &controlData->numberOfPlanes;
    int* capacity = &controlData->capacity;

    // calc the length of the Plane ID
    int length = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        length += 1;
    }

    char* plane = malloc(sizeof(char) * length);
    for (int i = 0; i < length + 1; i++) {
        plane[i] = buffer[i];
    }

    if (*numberOfPlanes + 2 > *capacity) {
        int biggerSize = (*capacity) + 10;
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
}

//
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
    sem_wait(lock);
    handle_add(buffer, controlData);
    // send back info
    fprintf(writeFile, "%s\n", controlData->info);
    fflush(writeFile);
    sem_post(lock);
}

void* handle_request(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    int conn_fd = threadData->conn_fd;
    ControlData* controlData = threadData->controlData;
    sem_t* lock = threadData->lock;

    FILE* readFile = fdopen(conn_fd, "r");
    FILE* writeFile = fdopen(conn_fd, "w");
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

//
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
    pthread_create(&threadId, 0, handle_client_job, (void*)mapperArgs);
    pthread_join(threadId, 0);
}

//
void accept_clients(char* info, int server) {
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
    ThreadData* threadData = malloc(sizeof(ThreadData));

    int conn_fd;
    while (conn_fd = accept(server, 0, 0), conn_fd >= 0) {
        threadData->conn_fd = conn_fd;
        threadData->controlData = controlData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) { // only accept 2 args (without mapper)
                                  // or 3 args (argc = 4) (with mapper)
        return handle_error_message(NUMS_OF_ARGS);
    }

    char* id = argv[1];
    char* info = argv[2];

    if (!is_valid_id(id) || !is_valid_id(info)) {
        return handle_error_message(INVALID_CHAR);
    }

    // Server
    unsigned int controlPort;
    int server = set_up_socket(0, &controlPort);

    if (argv[3]) {
        char* mapperPort = argv[3];
        handle_mapper(mapperPort, id, controlPort);
    }

    fprintf(stdout, "%u\n", controlPort);
    fflush(stdout);

    accept_clients(info, server);

    return 0;
}