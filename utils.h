#ifndef UTILS_H
#define UTILS_H
#include <stdbool.h>

// int defaultBufferSize = 80;

bool is_valid_id (char* id);
bool is_valid_port(char* port, int* portNumber);
bool read_line(FILE* file, char* buffer, int* size, bool* sighub_happen);
char* number_to_string(unsigned int number);
// void lexicographic_order(Flight** flights, int length);

#endif