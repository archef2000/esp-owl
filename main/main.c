
#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "utils/utils.h"
#include "wifi/capture.h"

CMD_cb ble_callbacks;
bool updating = false;
SemaphoreHandle_t update_mutex;

#define MAX_TOKENS 100

#define BLE_COMMAND 0
#define AWDL_COMMAND  1
#define OPENDROP_COMMAND  2

int COMMAND_LENGTH = 3;
char* commands[COMMAND_LENGTH] = {
    "ble",
    "awdl",
    "opendrop"
};

int getIndexOfString(char* array[], int size, char* str) {
    printf("str: %s \n", str);
    printf("str: \"%s\" \n", str);
    for(int i = 0; i < size; i++){
        printf("array[%d]: \"%s\"\n", i, array[i]);
        if(strcmp(array[i], str) == 0)
            return i;
    }
    return -1;
}

void main_cmd(char* str) {
    str[strcspn(str, "\r\n")] = 0;
    char* command;
    char* arguments[10];
    int arg_count = 0;
    command = strtok(str, " ");
    char* token = strtok(NULL, " ");
    while(token != NULL) {
        arguments[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    for(int i = 0; command[i]; i++){
        command[i] = tolower((unsigned char)command[i]);
    }
    printf("Command: %s\n", command);
    printf("argument len: %d\n", arg_count);
    for(int i = 0; i < arg_count; i++) {
        printf("Argument[%d]: %s\n", i, arguments[i]);
    }
    int command_index = getIndexOfString(commands, COMMAND_LENGTH, command);
    printf("command index: %d\n", command_index);
    switch (command_index) {
        case BLE_COMMAND:
            break;
        case AWDL_COMMAND:
            break;
        case OPENDROP_COMMAND:
            break;
        case -1:
            printf("error\n");
            break;
        default:
            printf("error\n");
            break;
    }
}

void app_main(void)
{
    wifi_sniffer_init();
}