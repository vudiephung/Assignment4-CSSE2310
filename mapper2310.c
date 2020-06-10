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

        if (strcmp(id, airports[mid]->id) > 0) { // 'id' is at the half right
            left = mid + 1; // set the left next to the mid
        } else {
            right = mid - 1; 
        }
    }

    // Cannot find the 'id'
    return -1; 
}

// Handle the case when client send ? + 'id'
// Find the airport with 'id' in 'mapData' then write the result to the
// 'writeFile'.
// Return void;
void handle_send(char* id, MapData* mapData, FILE* writeFile) {
    Airport** airports = mapData->airports;
    int numberOfAirports = mapData->numberOfAirports;
    if (is_valid_text(id)) {
        // Search for existing id
        int index = find_index_of_id(id, airports, numberOfAirports);
        if (index != -1) { // index is found
            // write the port of that airport
            fprintf(writeFile, "%s\n", airports[index]->port);
            fflush(writeFile);
        } else { // index is not found
            fprintf(writeFile, ";\n");
            fflush(writeFile);
        }
    }
}

// Sort the 'airports' with size of 'length' in lexicographic order
// Return void;
void lexicographic_order(Airport** airports, int length) {
    char tempId[defaultBufferSize]; // buffer to store id temporarily
    char tempPort[defaultBufferSize]; // buffer to store port temporarily
    for (int i = 0; i < length; i++) {
        for (int j = i + 1; j < length; j++) {
            //swapping strings if they are not in the lexicographical order
            if (strcmp((airports[i]->id), (airports[j]->id)) > 0) {
                strcpy(tempId, (airports[i]->id));
                strcpy(tempPort, (airports[i]->port));

                strcpy((airports[i]->id), (airports[j]->id));
                strcpy((airports[i]->port), (airports[j]->port));

                strcpy((airports[j]->id), tempId);
                strcpy((airports[j]->port), tempPort);
            }
        }
    }
}

// Add an 'airport' with id and port to the 'mapData'
// Return void;
void add_to_list(MapData* mapData, Airport* airport) {
    // if the allocated memory is nearly full
    if (mapData->numberOfAirports + 2 > mapData->capacity) {
        int biggerSize = (mapData->capacity) * 1.5;
        // Reallocate memory with bigger size
        Airport** newAirports = (Airport**)realloc(mapData->airports,
                sizeof(Airport*) * biggerSize);
        if (newAirports == 0) { // Cannot be reallocated
            return;
        }
        // Update the value of 'capacity' and and the 'airports'
        mapData->capacity = biggerSize;
        mapData->airports = newAirports;
    }

    // cannot find the existing id
    if (find_index_of_id(airport->id, mapData->airports,
            mapData->numberOfAirports) == -1) {
        // append that airport to the array
        mapData->airports[(mapData->numberOfAirports)++] = airport;
    }

    // Sort in lexicographic order
    lexicographic_order(mapData->airports, mapData->numberOfAirports);
}

// From given 'buffer' after emliminating the '!' characterr
// e.g: 'BNE:123', get id and port from it and add those to the airports
// of 'mapData'. Use the 'lock' to limit access to critical section.
// return void
void handle_add(char* buffer, MapData* mapData, sem_t* lock) {
    int colonPosition;
    // Find the position of colon and replace it with '\0'
    for (int i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == ':') {
            colonPosition = i;
            buffer[i] = '\0';
            break;
        }
    }

    // Get ID
    int lengthOfId = colonPosition + 1; // 1 more space for '\0'
    char* id = malloc(sizeof(char) * lengthOfId);
    memcpy(id, buffer, lengthOfId);

    // Get port
    buffer = buffer + colonPosition + 1;
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
        add_to_list(mapData, airport);
        sem_post(lock);
    }
}

// Handle the case when client send @
// Send the list of ids and ports of the airports (get from 'mapData')
// to the file pointer 'writeFile'
// return void;
void send_list(MapData* mapData, FILE* writeFile) {
    int numberOfAirports = mapData->numberOfAirports;
    Airport** airports = mapData->airports;
    for (int i = 0; i < numberOfAirports; i++) {
        fprintf(writeFile, "%s:%s\n", airports[i]->id, airports[i]->port);
        fflush(writeFile);
    }
}

// With the string get from socket endpoint 'buffer' (e.g: !add:123)
// Handle the command relates to that string and return void;
// with the data get from 'mapData'. After process the command,
// send the corresponding output to 'writeFile' and use the 'lock'
// to minimize the access to sensitive data.
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
            handle_add(buffer + 1, mapData, lock);
            break;
        default:
            break;
    }
}

// Thread function to continuously get the input and
// handle with it. If reading EOF from the other endpoint,
// clean up and return NULL pointer;
// arg: 'threadData' includes the struct ThreadData which contains
// the variables to handle a command
void* handle_request(void* threadData) {
    ThreadData* myThreadData = (ThreadData*)threadData;
    // socket endpoint file descriptor
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

    // Set up the socket and get the 'mapperPort' to display to stdout
    int server = set_up_socket(0, &mapperPort);
    fprintf(stdout, "%u\n", mapperPort);
    fflush(stdout);

    // Init lock
    sem_t lock;
    sem_init(&lock, 0, 1);

    // Initialise struct mapData
    int capacity = 10;
    MapData* mapData = malloc(sizeof(MapData));
    Airport** airports = malloc(sizeof(Airport*) * capacity);
    mapData->capacity = capacity;
    mapData->numberOfAirports = 0;
    mapData->airports = airports;

    pthread_t threadId;
    int connectFile; // file descriptor from return value of accept()
    while (connectFile = accept(server, 0, 0), connectFile >= 0) {
        ThreadData* threadData = malloc(sizeof(threadData));
        threadData->connectFile = connectFile;
        threadData->mapData = mapData;
        threadData->lock = &lock;
        pthread_create(&threadId, 0, handle_request, (void*)threadData);
    }

    sem_destroy(&lock);
    return 0;
}
