#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cli/tasks.h"

enum COMMANDS {
    BLE_COMMAND,
    AWDL_COMMAND,
    OPENDROP_COMMAND,
    MDNS_COMMAND,
    COMMAND_LENGTH
};

char* commands[COMMAND_LENGTH] = {
    "ble",
    "awdl",
    "opendrop",
    "mdns"
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

void main_cmd(char* str, struct availabeTasks *tasks) {
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
        case MDNS_COMMAND:
            if (arg_count == 1) {
                if (strcmp(arguments[0], "enable") == 0) {
                    printf("mdns enable bool: %d\n", tasks->mdns_enabled);
                    if (tasks->mdns_enabled) {
                        printf("mdns already enabled\n");
                        break;
                    }
                    vTaskResume(*tasks->mdns);
                } else if (strcmp(arguments[0], "disable") == 0) {
                    printf("mdns disable bool: %d\n", tasks->mdns_enabled);
                    if (tasks->mdns_enabled) {
                        printf("mdns already disabled\n");
                        break;
                    }
                    vTaskSuspend(*tasks->mdns);
                }
            }
            break;
        case -1:
            printf("error\n");
            break;
        default:
            printf("error\n");
            break;
    }
}
