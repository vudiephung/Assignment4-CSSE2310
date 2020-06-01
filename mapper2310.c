#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "server.h"
#include "utils.h"

int defaultBufferSize = 80;

typedef struct {
    char* id;
    int port;
} Flight;

typedef struct {
    int numbersOfMapping;
    int capacity;
    Flight** flights;
} MapData;

typedef struct {
    int conn_fd;
    MapData* mapData;
    sem_t* lock;
} ThreadData;

int find_index_of_id(char* id, Flight** flights, int length) {
    for (int i = 0; i < length; i++) {
        if (!strcmp(id, flights[i]->id)) {
            return i;
        }
    }
    return -1;
}

void handle_send(char* id, MapData* mapData, FILE* writeFile) {
    Flight** flights = mapData->flights;
    int numbersOfMapping = mapData->numbersOfMapping;
    if (is_valid_id(id)) {
        // Search for existing id
        int index = find_index_of_id(id, flights, numbersOfMapping);
        if (index != -1) {
            fprintf(writeFile, "%d\n", flights[index]->port);
            fflush(writeFile);
        } else {
            fprintf(writeFile, ";\n");
            fflush(writeFile);
        }
    } else {
        return;
    }
}

void lexicographic_order(Flight** flights, int length) {
    char tempBufferId[defaultBufferSize];
    int tempPort;
    for (int i = 0; i < length; i++) {
        for (int j = i + 1; j < length; j++) {
            //swapping strings if they are not in the lexicographical order
            if (strcmp((flights[i]->id), (flights[j]->id)) > 0) {
                strcpy(tempBufferId, (flights[i]->id));
                tempPort = flights[i]->port;

                strcpy((flights[i]->id), (flights[j]->id));
                flights[i]->port = flights[j]->port;

                strcpy((flights[j]->id), tempBufferId);
                flights[j]->port = tempPort;
            }
        }
    }
}

void add_to_list(MapData* mapData, char* id, int port) {
    int* numbersOfMapping = &mapData->numbersOfMapping;
    int* capacity = &mapData->capacity;
    // the allocated memory nearly full
    if (*numbersOfMapping + 2 > *capacity) {
        int biggerSize = (*capacity) + 10;
        Flight** newFlights = (Flight**)realloc(mapData->flights,
                sizeof(Flight*) * biggerSize);
        if (newFlights == 0) { // Cannot allocated
            return;
        }
        *capacity = biggerSize;
        mapData->flights = newFlights;
    }

    Flight* flight = malloc(sizeof(Flight));
    flight->id = id;
    flight->port = port;
    mapData->flights[(*numbersOfMapping)++] = flight;
}

void handle_add(char* buffer, MapData* mapData) {
    int* numbersOfMapping = &mapData->numbersOfMapping;
    int semicolonPosition;
    int port;

    for (int i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ':') {
            semicolonPosition = i;
            break;
        }
    }

    char* portString = buffer + semicolonPosition + 1;

    // Get ID
    int lengthOfIdBuffer = semicolonPosition + 2; // 1 more space for '\0'
    char* id = malloc(lengthOfIdBuffer);
    for (int i = 0; i < lengthOfIdBuffer; i++) {
        id[i] = buffer[i];
    }

    if (is_valid_id(id) &&
            is_valid_port(portString, &port) &&
            find_index_of_id(id, mapData->flights, *numbersOfMapping) == -1) {
        // Add to the list
        add_to_list(mapData, id, port);
        // Sort in lexicographic order
        lexicographic_order(mapData->flights, *numbersOfMapping);
    } else {
        return;
    }
}

void send_list(MapData* mapData, FILE* writeFile) {
    int numbersOfMapping  = mapData->numbersOfMapping;
    Flight** flights = mapData->flights;
    for (int i = 0; i < numbersOfMapping; i++) {
        fprintf(writeFile, "%s:%d\n", flights[i]->id, flights[i]->port);
        fflush(writeFile);
    }
}

void handle_command(char* buffer, MapData* mapData, FILE* writeFile,
        sem_t* lock) {
    if (!strcmp(buffer, "@")) {
        send_list(mapData, writeFile);
        return;
    }
    switch (buffer[0]) {
        case '?':
            handle_send(buffer + 1, mapData, writeFile);
            break;
        case '!':
            sem_wait(lock);
            handle_add(buffer + 1, mapData);
            sem_post(lock);
            break;
        default:
            break;
    }
}

void* handle_request(void* threadData) {
    ThreadData* myThreadData = (ThreadData*)threadData;
    int conn_fd = myThreadData->conn_fd;
    MapData* mapData = myThreadData->mapData;
    sem_t* lock = myThreadData->lock;

    FILE* readFile = fdopen(conn_fd, "r");
    FILE* writeFile = fdopen(conn_fd, "w");
    char* buffer = malloc(sizeof(char) * defaultBufferSize);

    // Get command
    while (true) {
        if (read_line(readFile, buffer, &defaultBufferSize, 0)) {
            handle_command(buffer, mapData, writeFile, lock);
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

int main(int argc, char** argv) {
    int server = set_up_socket(0);
    // Which port did we get?
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(server, (struct sockaddr*)&ad, &len)) {}
    unsigned int port = ntohs(ad.sin_port);
    printf("%u\n", port);
    int conn_fd;
    sem_t lock;
    sem_init(&lock, 0, 1);

    // Set up Struct
    int capacity = 10;
    MapData* mapData = malloc(sizeof(MapData));
    Flight** flights = malloc(sizeof(Flight*) * capacity);
    mapData->capacity = capacity;
    mapData->numbersOfMapping = 0;
    mapData->flights = flights;

    pthread_t threadId;
    ThreadData* threadData = malloc(sizeof(ThreadData));

    while (conn_fd = accept(server, 0, 0), conn_fd >= 0) { // change 0, 0 to get info about other end
        threadData->conn_fd = conn_fd;
        threadData->mapData = mapData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }

    sem_destroy(&lock);
    return 0;
}
