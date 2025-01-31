#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "utils/utils.h"
#include "wifi/capture.h"
#include "cli/uart.h"
#include "cli/tasks.h"
#include "wifi/core.h"
#include "utils/systeminfo.h"
#include "ble/coex.h"
#include "btn/restart.h"

void app_main(void)
{
    struct systemInfo sysinfo;
    sysinfo.tasks = malloc(sizeof(struct systemInfo));
    sysinfo.awdl = malloc(sizeof(struct daemon_state));
    sysinfo.tasks->mdns = malloc(sizeof(TaskHandle_t));
    init_btn_restart();
    wifi_sniffer_init(&sysinfo);
    init_uart(&sysinfo);
    init_coex(&sysinfo);
    // irgentd wo den pointer überschrieben
    //xTaskCreate(init_uart, "init_uart", 8072, &tasks, 10, NULL);
    //esp_log_set_vprintf(esp_log_default_vprintf);
}