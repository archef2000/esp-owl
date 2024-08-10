
#include "string.h"
#include "stdlib.h"
#include <stdio.h>

char* hex_to_string(const char *hex) {
    size_t len = strlen(hex);
    char *output = (char *)malloc(len / 2 + 1);
    if(output == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    char *output_start = output;
    for(size_t i = 0; i < len; i += 2) {
        char hex_byte[3] = {hex[i], hex[i+1], '\0'};
        int byte = strtol(hex_byte, NULL, 16);
        *(output++) = (char)byte;
    }
    *output = '\0';
    return output_start;
}

char *bt_hex(const void *buf, size_t len) {
    if (!buf || len == 0) {
        return NULL;
    }
    char *hex_str = malloc(len * 2 + 1);
    if (!hex_str) {
        return NULL;
    }
    const unsigned char *bytes = (const unsigned char *)buf;
    for (size_t i = 0; i < len; ++i) {
        // Convert each byte to a 2-character hexadecimal representation
        sprintf(&hex_str[i * 2], "%02x", bytes[i]);
    }
    return hex_str;
}
