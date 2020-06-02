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

// Default size to allocate a buffer
int defaultBufferSize = 80;

// Define an airport with id and port
typedef struct {
    char* id;
    char* port;
} Airport;

// A mapper contains data of 
typedef struct {
    int numberOfAirports; // The number of airports in the list
    int capacity; // define sizes of the allocated memory (could be extended)
    Airport** airports; // Array of airports
} MapData;

// The struct that is passed to thread function
typedef struct {
    int connectFile; // file descriptor that are returned from accept()
    MapData* mapData; // Data of the mapper
    sem_t* lock; // semaphore to handle race condition
} ThreadData;

// Find the index of the 'id' in 'Airport' with size of 'length'
// If the index is found, return it. Otherwise, return -1
int find_index_of_id(char* id, Airport** airports, int length) {
    for (int i = 0; i < length; i++) {
        if (!strcmp(id, airports[i]->id)) {
            return i;
        }
    }
    return -1;
}

// Handle the case when client send ? + 'id' of the airport
// Get the airports list from 'mapData' then write the result to the
// 'writeFile'.
// Return void;
void handle_send(char* id, MapData* mapData, FILE* writeFile) {
    Airport** airports = mapData->airports;
    int numberOfAirports = mapData->numberOfAirports;
    if (is_valid_text(id)) {
        // Search for existing id
        int index = find_index_of_id(id, airports, numberOfAirports);
        if (index != -1) { // index is found
            fprintf(writeFile, "%s\n", airports[index]->port);
            fflush(writeFile);
        } else { // index is not found
            fprintf(writeFile, ";\n");
            fflush(writeFile);
        }
    } else {
        return;
    }
}

// With 
void lexicographic_order(Airport** airports, int length) {
    char tempBufferId[defaultBufferSize];
    char tempPort[defaultBufferSize];
    for (int i = 0; i < length; i++) {
        for (int j = i + 1; j < length; j++) {
            //swapping strings if they are not in the lexicographical order
            if (strcmp((airports[i]->id), (airports[j]->id)) > 0) {
                strcpy(tempBufferId, (airports[i]->id));
                strcpy(tempPort, (airports[i]->port));

                strcpy((airports[i]->id), (airports[j]->id));
                strcpy((airports[i]->port), (airports[j]->port));

                strcpy((airports[j]->id), tempBufferId);
                strcpy((airports[j]->port), tempPort);
            }
        }
    }
}

//
void add_to_list(MapData* mapData, char* id, char* port) {
    int* numberOfAirports = &mapData->numberOfAirports;
    int* capacity = &mapData->capacity;
    // the allocated memory nearly full
    if (*numberOfAirports + 2 > *capacity) {
        int biggerSize = (*capacity) + 10;
        Airport** newFlights = (Airport**)realloc(mapData->airports,
                sizeof(Airport*) * biggerSize);
        if (newFlights == 0) { // Cannot allocated
            return;
        }
        *capacity = biggerSize;
        mapData->airports = newFlights;
    }

    Airport* flight = malloc(sizeof(Airport));
    flight->id = id;
    flight->port = port;
    mapData->airports[(*numberOfAirports)++] = flight;
}

//
void handle_add(char* buffer, MapData* mapData) {
    int* numberOfAirports = &mapData->numberOfAirports;
    int semicolonPosition;

    for (int i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ':') {
            semicolonPosition = i;
            buffer[i] = '\0';
            break;
        }
    }

    // Get ID
    int lengthOfId = semicolonPosition + 1; // 1 more space for '\0'
    char* id = malloc(sizeof(char) * lengthOfId);
    memcpy(id, buffer, lengthOfId);
    // strcpy(id, buffer);
    // for (int i = 0; i < lengthOfId; i++) {
    //     id[i] = buffer[i];
    // }
    // strcpy(buffer, id);
    // Get port
    buffer = buffer + semicolonPosition + 1;
    size_t lengthOfPort = strlen(buffer) + 1;
    char* port = malloc(sizeof(char) * lengthOfPort);
    // for (int i = 0; i < lengthOfPort; i++) {
    //     port[i] = buffer[i];
    // }
    memcpy(port, buffer, lengthOfPort);
    // strcpy(id, buffer);

    if (is_valid_text(id) &&
            is_valid_port(port) &&
            find_index_of_id(id, mapData->airports, *numberOfAirports) == -1) {
        // Add to the list
        add_to_list(mapData, id, port);
        // Sort in lexicographic order
        lexicographic_order(mapData->airports, *numberOfAirports);
    }
}

//
void send_list(MapData* mapData, FILE* writeFile) {
    int numberOfAirports  = mapData->numberOfAirports;
    Airport** airports = mapData->airports;
    for (int i = 0; i < numberOfAirports; i++) {
        fprintf(writeFile, "%s:%s\n", airports[i]->id, airports[i]->port);
        fflush(writeFile);
    }
}

//
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

//
void* handle_request(void* threadData) {
    ThreadData* myThreadData = (ThreadData*)threadData;
    int connectFile = myThreadData->connectFile;
    MapData* mapData = myThreadData->mapData;
    sem_t* lock = myThreadData->lock;

    FILE* readFile = fdopen(connectFile, "r");
    FILE* writeFile = fdopen(connectFile, "w");
    char* buffer = malloc(sizeof(char) * defaultBufferSize);

    // Get command
    while (true) {
        if (read_line(readFile, buffer, &defaultBufferSize)) {
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
    unsigned int mapperPort;
    int server = set_up_socket(0, &mapperPort);

    fprintf(stdout, "%u\n", mapperPort);
    fflush(stdout);
    int connectFile;
    sem_t lock;
    sem_init(&lock, 0, 1);

    // Set up Struct
    int capacity = 10;
    MapData* mapData = malloc(sizeof(MapData));
    Airport** airports = malloc(sizeof(Airport*) * capacity);
    mapData->capacity = capacity;
    mapData->numberOfAirports = 0;
    mapData->airports = airports;

    pthread_t threadId;
    ThreadData* threadData = malloc(sizeof(ThreadData));

    while (connectFile = accept(server, 0, 0), connectFile >= 0) {
        threadData->connectFile = connectFile;
        threadData->mapData = mapData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }

    sem_destroy(&lock);
    return 0;
}
