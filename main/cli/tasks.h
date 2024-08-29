#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef TASKS_H
#define TASKS_H
struct availabeTasks {
    TaskHandle_t *mdns;
    bool mdns_enabled;
};
#endif /* TASKS_H */