#include <stdio.h>
#include "io.h"
#include "esp_wifi.h"

int wlan_send(const struct io_state *state, const uint8_t *buf, int len) {
	esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false);
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
