#pragma once

#include <stdint.h>

uint16_t sizeof_string(char* string);
uint16_t deserialize_string(char* buffer, char** string);
uint16_t serialize_string(char* buffer, char* string, uint16_t size);
