#include <stdio.h>
#include "io.h"
#include "esp_wifi.h"

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
 */

int wlan_send(const struct io_state *state, const uint8_t *buf, int len) {
	//printf("wlan_send: len = %d\n", len);
    //for (int i = 0; i < len; i++) {
    //    printf("%02x ", buf[i]);
    //}
	esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false);
	//printf("\n");
    //printf("\nwlan_send: end\n");
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

// read the data from the netif so ev_io_init 
int host_recv(const struct io_state *state, uint8_t *buf, int *len) {
	printf("host_recv");
	//long nread;
	//if (!state || !state->host_fd)
	//	return -EINVAL;
	//nread = read(state->host_fd, buf, *len);
	//if (nread < 0) {
	//	if (errno != EWOULDBLOCK)
	//		log_error("tun: error reading from device");
	//	return -errno;
	//}
	//*len = nread;
    *len = 0;
	return 0;
}