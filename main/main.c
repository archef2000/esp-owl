#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "utils/utils.h"
#include "wifi/capture.h"
#include "cli/uart.h"
#include "cli/tasks.h"

void app_main(void)
{
    struct availabeTasks tasks;
    tasks.mdns = malloc(sizeof(TaskHandle_t));
    wifi_sniffer_init(&tasks);
    init_uart(&tasks);
}