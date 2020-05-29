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

int defaultBufferSize = 80;

bool sighupHappen = false;
void sighub_handler(int s) {
    sighupHappen = true;
}

typedef struct {
    char* id;
    char* controlPort;
    char* mapperPort;
} MapperArgs;

typedef struct {
    int capacity;
    int numberOfPlanes;
    char** planes;
} ControlData;

typedef struct {
    int conn_fd;
    ControlData* ControlData;
    sem_t* lock;
} ThreadData;

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
    MapperArgs* controlData = (MapperArgs*)data;
    char* id = controlData->id;
    char* controlPort = controlData->controlPort;
    char* mapperPort = controlData->mapperPort;

    int client = set_up(mapperPort); // set up as client connect to mapperPort
    FILE* writeFile = fdopen(client, "w");
    
    fprintf(writeFile, "!%s:%s\n", id, controlPort);
    fflush(writeFile);

    // close the connection
    close(client);

    return 0;
}

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

void handle_add(char* plane, ControlData* controlData) {
    int* numberOfPlanes = &controlData->numberOfPlanes;
    int* capacity = &controlData->capacity;

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
    // printf("%d\n", controlData->numberOfPlanes);
    // lexicographic_order(controlData->planes, *numberOfPlanes);
    // printf("%s\n", controlData->planes[(*numberOfPlanes - 1)]);
}

void handle_command(char* buffer, ControlData* controlData, FILE* writeFile,
        sem_t* lock) {
    // for (int i = 0; i < controlData->numberOfPlanes; i++) {
    //     fprintf(writeFile, "%s\n", controlData->planes[i]);
    //     fflush(writeFile);
    // }
    if (!strcmp(buffer, "log") || sighupHappen) {
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
    sem_post(lock);
}

void* handle_request(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    int conn_fd = threadData->conn_fd;
    ControlData* controlData = threadData->ControlData;
    sem_t* lock = threadData->lock;

    FILE* readFile = fdopen(conn_fd, "r");
    FILE* writeFile = fdopen(conn_fd, "w");
    char* buffer = malloc(sizeof(char) * defaultBufferSize);

    // Get command
    while (true) {
        if (read_line(readFile, buffer, &defaultBufferSize, &sighupHappen)) {
            handle_command(buffer, controlData, writeFile, lock);
        } else {
            break;
        }
    }
    // printf("Happened\n");
    // clean up
    free(buffer);
    fclose(readFile);
    fclose(writeFile);

    return 0;
}

int main(int argc, char** argv) {
    struct sigaction sighubAction;
    memset(&sighubAction, 0, sizeof(struct sigaction));
    sighubAction.sa_handler = sighub_handler;
    sighubAction.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sighubAction, 0);

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
        MapperArgs* controlData = malloc(sizeof(controlData));
        controlData->id = id;
        controlData->controlPort = number_to_string(controlPort);
        controlData->mapperPort = argv[3];

        pthread_t threadId;
        pthread_create(&threadId, 0, handle_client_job, (void*)controlData);
    }

    ControlData* ControlData = malloc(sizeof(ControlData));
    int capacity = 10;
    char** planes = malloc(sizeof(char*) * capacity);
    for (int i = 0; i < capacity; i++) {
        planes[i] = malloc(sizeof(char) * defaultBufferSize);
    }
    ControlData->capacity = capacity;
    ControlData->numberOfPlanes = 0;
    ControlData->planes = planes;

    sem_t lock;
    sem_init(&lock, 0, 1);
    pthread_t threadId;
    ThreadData* threadData = malloc(sizeof(ThreadData));

    while (conn_fd = accept(server, 0, 0), conn_fd >= 0) { // change 0, 0 to get info about other end
        // if(sighupHappen) {
        //     printf("Happened\n");
        // }
        threadData->conn_fd = conn_fd;
        threadData->ControlData = ControlData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }
    // pthread_exit((void*)0);
    return 0;
}