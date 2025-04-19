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
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"
#include "owl/state.h"

void send_test() {
	nvs_flash_init();
    esp_netif_init();
    static wifi_country_t wifi_country = {.cc="US", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country)); /* set country for channel range [1, 13] */
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_IF_STA));
    ESP_ERROR_CHECK( esp_wifi_start());
    // 1,2,8,
    // no 4
    uint8_t data[] = {
        0x88, 0x00, 0x2c, 0x00, 0x52, 0xe4, 0x90, 0x8a, 0xfe, 0x00, 0x62, 0x50, 0x4f, 0x8e, 0x9a, 0x95,
        0x00, 0x25, 0x00, 0xff, 0x94, 0x73, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0x03, 0x00, 0x17, 0xf2,
        0x08, 0x00, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x86, 0xdd, 0x60, 0x0e, 0x41, 0x5c, 0x00, 0x2c,
        0x06, 0x40, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x60, 0x50, 0x4f, 0xff, 0xfe, 0x8e, 0x9a, 0x95,
        0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0e, 0x49, 0x0f, 0xff, 0xfe, 0x8a, 0xfe, 0x00,
        0xf6, 0x54, 0x22, 0x42, 0x38, 0x99, 0xfd, 0xfc, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x02, 0xff, 0xff,
        0x18, 0x45, 0x00, 0x00, 0x02, 0x04, 0x05, 0xa0, 0x01, 0x03, 0x03, 0x06, 0x01, 0x01, 0x08, 0x0a,
        0x58, 0xee, 0x51, 0xca, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00
    };
    esp_wifi_80211_tx(WIFI_IF_STA, data, sizeof(data), false);
    esp_wifi_internal_tx(WIFI_IF_STA, (void *)data, sizeof(data));
}

void app_main(void)
{
    clock_time_us();
    struct systemInfo sysinfo;
    sysinfo.tasks = malloc(sizeof(struct systemInfo));
    sysinfo.awdl = malloc(sizeof(struct daemon_state));
    sysinfo.tasks->mdns = malloc(sizeof(TaskHandle_t));
    init_btn_restart();
    wifi_sniffer_init(&sysinfo);
    
    //send_test();
    
    init_uart(&sysinfo);
    init_coex(&sysinfo);
    // irgentd wo den pointer überschrieben
    //xTaskCreate(init_uart, "init_uart", 8072, &tasks, 10, NULL);
    //esp_log_set_vprintf(esp_log_default_vprintf);
}
