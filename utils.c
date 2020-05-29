#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#define MAX_PORT_VALUE 65536

// return true iff given 'myString' contains only digit(s)
// and change the value of 'value' into the number that got from 'myString'
bool is_digits_only(const char* myString, int* value) {
    char temporary;
    return (sscanf(myString, "%d%c", value, &temporary) == 1);
}

bool is_valid_id (char* id) {
    for (int i = 0; i < strlen(id); i++) {
        if (id[i] == ':' || id[i] == '\n' || id[i] == '\r') {
            return false;
        }
    }
    return true;
}

bool is_valid_port(char* port, int* portNumber) {
    if (!is_digits_only(port, portNumber)) {
        return false;
    }
    return (*portNumber > 0 && *portNumber < MAX_PORT_VALUE);
}

bool read_line(FILE* file, char* buffer, int* size) {
    int count = 0;
    buffer[0] = '\0';
    int next;

    while (true) {
        next = fgetc(file);
        if (next == EOF || next == '\n') {
            buffer[count] = '\0';
            break;
        } else {
            if (count + 2 > *size) {
                int biggerSize = *size * 1.5;
                void* newBuffer = realloc(buffer, biggerSize);
                if (newBuffer == 0) {
                    return false;
                }
                *size = biggerSize;
                buffer = newBuffer;                
            }
            buffer[count++] = (char)next;
        }
    }

    return next != EOF;
}