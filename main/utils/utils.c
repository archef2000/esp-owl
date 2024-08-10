#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "driver/gpio.h"

//char* concat_array_old(char* array[], int start, int end) {
//    char* result = malloc(sizeof(char*) * (end - start + 1));
//    result[0] = '\0';
//    for (int i = start; i < end; i++) {
//        strncat(result, array[i], strlen(array[i]));
//        if (i != end - 1) {
//            strcat(result, " ");
//        }
//        }
//    result[strcspn(result, "\r\n")] = 0;
//    return result;
//}
char* concat_array(char* array[], int start, int end) {
    // Calculate the total length needed for the concatenated string
    size_t total_length = 0;
    for (int i = start; i < end; i++) {
        total_length += strlen(array[i]);
    }

    // Allocate memory for the result (including space for null-terminator)
    char* result = (char*)malloc(total_length + 1);
    if (!result) {
        // Handle memory allocation failure
        return NULL;
    }

    // Initialize the result string
    result[0] = '\0';

    // Concatenate the strings
    for (int i = start; i < end; i++) {
        strcat(result, array[i]);
        if (i != end - 1) {
            strcat(result, " ");
        }
    }

    // Remove any trailing newline or carriage return
    result[strcspn(result, "\r\n")] = '\0';

    return result;
}
void gpioSetup(int gpioNum, int gpioMode, int gpioVal) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << 48);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(48, 0);
}

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))
#define floor(a)   ((int)(a))
#define ceil(a)    ((int)((int)(a) < (a) ? (a+1) : (a)))
