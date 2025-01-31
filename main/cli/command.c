#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cli/tasks.h"
#include "utils/taskinfo.h"
#include "utils/systeminfo.h"
#include "cli/backtrace.h"
#include "drop/cert.h"
#include "owl/log.h"
#include "wifi/core.h"

enum COMMANDS {
    BLE_COMMAND,
    AWDL_COMMAND,
    OPENDROP_COMMAND,
    MDNS_COMMAND,
    TASKS_COMMAND,
    TRACE_COMMAND,
    COMMAND_LENGTH
};

char* commands[COMMAND_LENGTH] = {
    "ble",
    "awdl",
    "drop",
    "mdns",
    "tasks",
    "trace",
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

void handle_opendrop_command(char* str[], int arg_count) {
    printf("handle_opendrop_command\n");
    for (int i = 0; i < arg_count; i++) {
        printf("Argument[%d]: %s\n", i, str[i]);
    }
    if (arg_count >= 1) {
        if (strcmp(str[0], "cert") == 0) {
            if (arg_count >= 2) {
                if (strcmp(str[1], "print") == 0) {
                    print_stored_cert_and_key();
                } else if (strcmp(str[1], "gen") == 0) {
                    generate_and_store_cert_key();
                } 
            }
        } else if (strcmp(str[0], "dis") == 0) {
            if (arg_count == 2) {
                log_error("mac address: %s len: %d\n", str[1], strlen(str[1]));
                if (strlen(str[1]) != 17) {
                    printf("invalid mac address\n");
                    return;
                }
                struct ether_addr *mac;
                parse_mac_address(str[1], &mac);
                char *ipv6 = malloc(sizeof(char) * INET6_ADDRSTRLEN);
                in6_addr_to_string(ipv6, ether_addr_to_in6_addr(mac));
                printf("ipv6: %s\n", ipv6);
                https_request(ipv6);
                // drop dis a9:c2:3d:00:15:c7
            }
        }
    }
}

void hangle_awdl_command(char* str[], int arg_count, struct daemon_state *state) {
    printf("handle_awdl_command\n");
    for (int i = 0; i < arg_count; i++) {
        printf("Argument[%d]: %s\n", i, str[i]);
    }
    if (arg_count == 1) {
        if (strcmp(str[0], "list") == 0) {
            awdl_neighbors_print(state);
        } else if (strcmp(str[0], "disable") == 0) {
            printf("disabeling awdl\n");
            awdl_disable(state);
        }
    }
}

void main_cmd(char* str, struct systemInfo *sysinfo) {
    struct availabeTasks *tasks = sysinfo->tasks;
    struct daemon_state *state = sysinfo->awdl;
    str[strcspn(str, "\r\n")] = 0;
    if (strlen(str) == 0) {
        return;
    }
    char* command;
    char* arguments[20];
    int arg_count = 0;
    command = strtok(str, " ");
    char* token = strtok(NULL, " ");
    while(token != NULL && arg_count < 20) {
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
            log_error("BLE command");
            xTaskCreate(test_cert, "test_cert", 2048*8, NULL, 5, NULL);
            break;
        case TRACE_COMMAND:
            test_backtrace();
            break;
        case AWDL_COMMAND:
            hangle_awdl_command(arguments,arg_count, state);
            break;
        case OPENDROP_COMMAND:
            handle_opendrop_command(arguments,arg_count);
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
        case TASKS_COMMAND:
            list_tasks();
            break;
        case -1:
            printf("error\n");
            break;
        default:
            printf("error\n");
            break;
    }
}
