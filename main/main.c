#include <string.h>
#include <stddef.h>
#include "stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include "utils/utils.h"
#include "wifi/capture.h"
#include "cli/uart.h"

void app_main(void)
{
    wifi_sniffer_init();
    init_uart();
}