#include <stdio.h>
#include "io.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"

int wlan_send(const struct io_state *state, const uint8_t *buf, int len) {
    if (!(buf[0] == 0xD0 && (buf[1] == 0x00 || buf[1] == 0x10))) {
        printf("wlan_send: len = %d\n", len);
        for (int i = 0; i < len; i++) {
            printf("%02X ", buf[i]);
        }
    }
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false));
    //ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_internal_tx(WIFI_IF_STA, (void *)buf, len));
	return 0;
}

int host_send(const struct io_state *state, const uint8_t *buf, int len)
{
    printf("host_send: len = %d\n", len);
    for (int i = 0; i < len; i++) {
        printf("buf[%d] = %d\n", i, buf[i]);
    }
    printf("\nhost_send: end\n");
	return 0;
}
