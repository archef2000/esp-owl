#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "taskinfo.h"

void vTaskGetRunTimeStats2(char *pcWriteBuffer) {
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime, ulRunTimePercentage;
    // Make sure the write buffer does not contain a string.
    *pcWriteBuffer = 0x00;
    // Get the number of tasks currently running
    uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray == NULL) {
        printf("Failed to allocate memory for task status array\n");
        return;
    }
    // Get the system state
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);
    // Convert total run time to a percentage scale
    ulTotalRunTime /= 100UL;

    sprintf(pcWriteBuffer,"TotalRunTime: %lu TaskCount: %d\r\n", ulTotalRunTime, uxArraySize);
    pcWriteBuffer += strlen(pcWriteBuffer);

    // get length of longest task name
    int namelength = 0;
    for (x = 0; x < uxArraySize; x++) {
        TaskStatus_t *ts = &pxTaskStatusArray[x];
        if (strlen(ts->pcTaskName) > namelength) {
            namelength = strlen(ts->pcTaskName);
        }
    }
    namelength++;
    
    sprintf(pcWriteBuffer,"%-*.*s State\t\tPrio\tStack\tRunTime%%  RunTime\r\n",namelength,namelength,"Task Name");
    pcWriteBuffer += strlen(pcWriteBuffer);
    if (ulTotalRunTime > 0) {
        // Format the data for each task
        for (x = 0; x < uxArraySize; x++) {
            TaskStatus_t *ts = &pxTaskStatusArray[x];
            ulRunTimePercentage = ts->ulRunTimeCounter / ulTotalRunTime;

            sprintf(pcWriteBuffer, "%-*.*s %s   \t%u\t%lu\t%s%ld%%\t  %lu\r\n",
                    namelength,
                    namelength,
                    ts->pcTaskName,
                    ts->eCurrentState == eRunning ? "Running" :    /**< A task is querying the state of itself, so must be running. */
                    ts->eCurrentState == eReady ? "Ready" :      /**< The task being queried is in a ready or pending ready list. */
                    ts->eCurrentState == eBlocked ? "Blocked" :    /**< The task being queried is in the Blocked state. */
                    ts->eCurrentState == eSuspended ? "Suspended" :  /**< The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */ 
                    "Deleted",  /**< The task being queried has been deleted, but its TCB has not yet been freed. */
                    ts->uxCurrentPriority,                 // Task priority
                    ts->usStackHighWaterMark,              // Minimum stack space left
                    ulRunTimePercentage > 0 ? "" : "<",
                    ulRunTimePercentage > 0 ? ulRunTimePercentage : 1,
                    ts->ulRunTimeCounter
                    );
            pcWriteBuffer += strlen(pcWriteBuffer);
        }
    }

    // Free allocated memory
    vPortFree(pxTaskStatusArray);
}

void list_tasks(void) {
    // Allocate a buffer for the runtime stats
    char *runtime_stats_buffer = malloc(1024*2);
    if (runtime_stats_buffer == NULL) {
        printf("Failed to allocate memory for runtime stats buffer\n");
        return;
    }
    // Get and print runtime stats
    vTaskGetRunTimeStats2(runtime_stats_buffer);
    printf("Task Runtime Statistics:\n%s", runtime_stats_buffer);
    // Free the buffer
    free(runtime_stats_buffer);
}