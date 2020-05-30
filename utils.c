#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

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
    const int maxPortValue = 65536;

    int temp;
    if (portNumber == NULL) {
        portNumber = &temp;
    }

    if (!is_digits_only(port, portNumber)) {
        return false;
    }

    return (*portNumber > 0 && *portNumber < maxPortValue);
}

bool read_line(FILE* file, char* buffer, int* size, bool* sighubHappen) {
    int count = 0;
    buffer[0] = '\0';
    int next;

    while (true) {
        // if (*sighubHappen == true) {
        //     return false;
        // }
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

// From given 'number', count how many digits and return it
// e.g: count_number_digit(1234) returns 4
int count_number_digit(int number) {
    int count = 0;
    do {
        count++;
        number /= 10;
    } while (number != 0);
    return count;
}

// Convert given 'number' into string and return it
// e.g: number_to_string(123) returns "123"
char* number_to_string(unsigned int number) {
    int ditgitsCount = count_number_digit(number);
    char* numberAsString = malloc(sizeof(char) * ditgitsCount);
    sprintf(numberAsString, "%d", number);
    return numberAsString;
}

// void lexicographic_order(Flight** flights, int length) {
//     char tempBufferId[defaultBufferSize];
//     int tempPort;
//     for (int i = 0; i < length; i++) {
//         for (int j = i + 1; j < length; j++) {
//             //swapping strings if they are not in the lexicographical order
//             if (strcmp((flights[i]->id), (flights[j]->id)) > 0) {
//                 strcpy(tempBufferId, (flights[i]->id));
//                 tempPort = flights[i]->port;

//                 strcpy((flights[i]->id), (flights[j]->id));
//                 flights[i]->port = flights[j]->port;

//                 strcpy((flights[j]->id), tempBufferId);
//                 flights[j]->port = tempPort;
//             }
//         }
//     }
// }