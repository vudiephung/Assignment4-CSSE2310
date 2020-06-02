#ifndef UTILS_H
#define UTILS_H
#include <stdbool.h>

bool is_digits_only(const char* myString, int* value);
bool is_valid_text (char* id);
bool is_valid_port(char* port);
bool read_line(FILE* file, char* buffer, int* size);
char* number_to_string(unsigned int number);

#endif