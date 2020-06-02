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

//
bool is_valid_text(char* text) {
    for (int i = 0; i < strlen(text); i++) {
        if (text[i] == ':' || text[i] == '\n' || text[i] == '\r') {
            return false;
        }
    }
    return true;
}

//
bool is_valid_port(char* port) {
    const int maxPortValue = 65535;

    int portNumber;

    if (!is_digits_only(port, &portNumber)) {
        return false;
    }

    return (portNumber > 0 && portNumber < maxPortValue);
}

//
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