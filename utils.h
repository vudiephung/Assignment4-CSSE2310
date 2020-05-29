#ifndef UTILS_H
#define UTILS_H
#include <stdbool.h>

bool is_valid_id (char* id);
bool is_valid_port(char* port, int* portNumber);
bool read_line(FILE* file, char* buffer, int* size);

#endif