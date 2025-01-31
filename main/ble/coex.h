#include <stdint.h>
#include "owl/schedule.h"
#include "owl/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils/systeminfo.h"
#include "wifi/core.h"

void init_coex(struct systemInfo *sysinfo);