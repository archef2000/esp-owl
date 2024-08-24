
#include "esp_wifi_types.h"
#include "owl/ethernet.h"
#include "esp_netif.h"

#ifndef WIFI_CAPTURE_H
#define WIFI_CAPTURE_H

typedef struct {
	unsigned frame_control:16;
	unsigned duration_id:16;
	uint8_t addr1[6]; /* receiver address */
	uint8_t addr2[6]; /* sender address */
	uint8_t addr3[6]; /* filtering address */
	unsigned sequence_ctrl:16;
	//uint8_t addr4[6]; /* optional */
} wifi_ieee80211_hdr_t;

typedef struct {
	wifi_ieee80211_hdr_t hdr;
	uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

typedef struct wifi_pkt_t {
    wifi_pkt_rx_ctrl_t rx_ctrl; // https://github.com/espressif/esp-idf/blob/0479494e7abe5aef71393fba2e184b3a78ea488f/components/esp_wifi/include/local/esp_wifi_types_native.h#L86
    wifi_ieee80211_hdr_t hdr;
    uint8_t payload[0];
} wifi_pkt_t;

struct __attribute__((scalar_storage_order("little-endian"))) awdl_packet {
	struct ether_addr dst;
	struct ether_addr src;
	uint16_t ether_type;
	uint8_t len;
	uint8_t data[0];
};

#define EndianConvert16(w) ((w>>8)|(w<<8))
#define EndianConvert32(dw) ((dw<<24)|((dw>>8)&0xFF00)|((dw<<8)&0xFF0000)|(dw>>24))

#endif /* WIFI_CAPTURE_H */

void wifi_sniffer_init(void);

