#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "socket.h"
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

// Using Binary Search to find the index of the 'id' in 'Airport' with
// size of 'length'. If the index is found, return it. Otherwise, return -1
int find_index_of_id(char* id, Airport** airports, int length) {
    int left = 0;
    int right = length - 1;

    while (left <= right) { 
        int mid = left + (right - left) / 2;

        if (!strcmp(id, airports[mid]->id)) { // found
            return mid; 
        }

        if (strcmp(id, airports[mid]->id) > 0) { // id one the half right
            left = mid + 1; // set the right next to the mid
        }

        else {
            right = mid - 1; 
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
    }
}

// Sort the 'airports' with size of 'length' in lexicographic order
// Return void
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

// Add an airport with 'id' and 'port' to the 'mapData'
// Return void;
void add_to_list(MapData* mapData, Airport* airport, sem_t* lock) {
    // sem_wait(lock);
    // int* numberOfAirports = &mapData->numberOfAirports;
    // int* capacity = &mapData->capacity;

    // if the allocated memory is nearly full
    if (mapData->numberOfAirports + 2 > mapData->capacity) {
        int biggerSize = (mapData->capacity) * 1.5;
        // Reallocate memory with bigger size
        Airport** newAirports = (Airport**)realloc(mapData->airports,
                sizeof(Airport*) * biggerSize);
        if (newAirports == 0) { // Cannot be allocated
            return;
        }
        // Update the value of 'capacity' and and the 'airports'
        mapData->capacity = biggerSize;
        mapData->airports = newAirports;
    }

    // cannot find the existing index
    if (find_index_of_id(airport->id, mapData->airports,
            mapData->numberOfAirports) == -1) {
        // append that airport to the array
        mapData->airports[(mapData->numberOfAirports)++] = airport;
    }

    // Sort in lexicographic order
    lexicographic_order(mapData->airports, mapData->numberOfAirports);

    // sem_post(lock);
}

// From given 'buffer' after emliminated the '!' characterr
// e.g: 'BNE:123', get id and port from it and add it to the airports
// from 'mapData'
// Then sort the list of airports with lexicographic order
// return void
void handle_add(char* buffer, MapData* mapData, sem_t* lock) {
    int semicolonPosition;

    for (int i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ':') {
            semicolonPosition = i;
            buffer[i] = '\0';
            break;
        }
    }

    // char* id = malloc(sizeof(char) * defaultBufferSize);
    // char* port = malloc(sizeof(char) * defaultBufferSize);

    // strcpy(id, buffer);
    // strcpy(port, buffer + semicolonPosition + 1);

    // Get ID
    int lengthOfId = semicolonPosition + 1; // 1 more space for '\0'
    char* id = malloc(sizeof(char) * lengthOfId);
    memcpy(id, buffer, lengthOfId);

    // Get port
    buffer = buffer + semicolonPosition + 1;
    size_t lengthOfPort = strlen(buffer) + 1;
    char* port = malloc(sizeof(char) * lengthOfPort);
    memcpy(port, buffer, lengthOfPort);

    if (is_valid_text(id) && is_valid_port(port)) {
        // allocate new airport with 'id' and 'port'
        Airport* airport = malloc(sizeof(Airport));
        airport->id = id;
        airport->port = port;
        // Add to the list
        sem_wait(lock);
        add_to_list(mapData, airport, lock);
        sem_post(lock);
    }
}

// Send the list (get from 'mapData') of the airports with its id and port
// to the file pointer 'writeFile'
// return void; 
void send_list(MapData* mapData, FILE* writeFile) {
    int numberOfAirports  = mapData->numberOfAirports;
    Airport** airports = mapData->airports;
    for (int i = 0; i < numberOfAirports; i++) {
        fprintf(writeFile, "%s:%s\n", airports[i]->id, airports[i]->port);
        fflush(writeFile);
    }
}

// With the string get from socket endpoint 'buffer' (e.g: !add:123)
// Handle the command relates to that string and return void;
// Args: 'mapData', 'writeFile' and 'lock' is passed down to its
// inner function
void handle_command(char* buffer, MapData* mapData, FILE* writeFile,
        sem_t* lock) {
    if (!strcmp(buffer, "@")) {
        sem_wait(lock);
        send_list(mapData, writeFile);
        sem_post(lock);
        return;
    }

    switch (buffer[0]) {
        case '?':
            sem_wait(lock);
            handle_send(buffer + 1, mapData, writeFile);
            sem_post(lock);
            break;
        case '!':
            handle_add(buffer + 1, mapData, lock);
            break;
        default:
            break;
    }
}

// When a client is accept() by the server, create a new thread, 
// each thread will run this function to continuously get the input and
// handle it until reading eof from the other end point.
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

    // Set up socket and get the 'mapperPort' to display to stdout
    int server = set_up_socket(0, &mapperPort);

    fprintf(stdout, "%u\n", mapperPort);
    fflush(stdout);

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

    int connectFile; // file descriptor from return value of accept()
    while (connectFile = accept(server, 0, 0), connectFile >= 0) {
        threadData->connectFile = connectFile;
        threadData->mapData = mapData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }

    sem_destroy(&lock);
    return 0;
}
