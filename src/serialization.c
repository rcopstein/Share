#include <string.h>
#include <stdlib.h>
#include <printf.h>

#include "serialization.h"

void mmemcpy(void * dst, void * src, size_t size) {

    memcpy(dst, src, size);
    ((char *)dst)[size - 1] = '\0';
    printf("Copied: %s\n", (char *) dst);

}

uint16_t sizeof_string(char* string) {

    return (uint16_t)(strlen(string) + sizeof(uint16_t));

}

uint16_t deserialize_string(char* buffer, char** string) {

    uint16_t size;
    memcpy(&size, buffer, sizeof(uint16_t));

    *string = (char *) malloc(size + 1);
    memcpy(*string, buffer + sizeof(uint16_t), size);
    (*string)[size] = '\0';

    return sizeof(uint16_t) + size;

}

uint16_t serialize_string(char* buffer, char* string, uint16_t size) {

    memcpy(buffer, &size, sizeof(uint16_t));
    memcpy(buffer + sizeof(uint16_t), string, size);

    return sizeof(uint16_t) + size;

}
