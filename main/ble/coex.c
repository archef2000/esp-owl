#include "wifi/core.h"
#include <stdint.h>
#include "owl/schedule.h"
#include "owl/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils/systeminfo.h"
#include "esp_coexist.h"

// when there is no eaw between the aw there will be a 3*16TU window (=3*16*1024us=49ms)

void coex_task(void *pvParameters) {
    struct systemInfo *sysinfo = (struct systemInfo *)pvParameters;
    struct awdl_state *awdl_state = &sysinfo->awdl->awdl_state;
    log_error("awdl pointer task1: %p\n", awdl_state);
    vTaskDelay(pdMS_TO_TICKS(2000));
    log_error("awdl pointer task2: %p\n", awdl_state);
    double next_inactive = 0,next_active = 0;
    while (1) {
        log_info("coex_task: next_inactive: %f", next_inactive);
        // printf("presence_mode: %d\n",awdl_state->sync.presence_mode); // 4
        // printf("aw_period: %d\n",awdl_state->sync.aw_period); // 16
        // printf("eaw_period: %lu\n",ieee80211_tu_to_usec(awdl_state->sync.presence_mode * awdl_state->sync.aw_period)); // 4 * 16 = 64TU = 65536us
        printf("time: %llu\n",clock_time_us());
        next_inactive = awdl_inactive_in_us(awdl_state, clock_time_us());
        log_info("coex_task: next_inactive: %fus", next_inactive);
        vTaskDelay(pdMS_TO_TICKS(next_inactive/1000));
        ESP_ERROR_CHECK(esp_coex_preference_set(ESP_COEX_PREFER_BT));
        log_info("coex_task begin");
        next_active = awdl_active_in(awdl_state, clock_time_us());
        printf("active_in: %f\n", next_active);
        vTaskDelay(pdMS_TO_TICKS(next_active/1000));
        ESP_ERROR_CHECK(esp_coex_preference_set(ESP_COEX_PREFER_WIFI));
        log_info("coex_task end");
    }
}

void init_coex(struct systemInfo *sysinfo) {
    log_error("awdl pointer inti: %p\n", &sysinfo->awdl->awdl_state);
    vTaskDelay(pdMS_TO_TICKS(4000));
    log_error("awdl pointer inti: %p\n", &sysinfo->awdl->awdl_state);
    //xTaskCreate(coex_task, "coex_task", 8096*20, sysinfo, 5, NULL);
}

