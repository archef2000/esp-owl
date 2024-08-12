/**
 * Copyright (c) 2017, Łukasz Marcin Podkalicki <lpodkalicki@gmail.com>
 * ESP32/016
 * WiFi Sniffer.
 */

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "owl/ethernet.h"
#include "owl/channel.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "owl/rx.h"
#include "capture.h"
#include "owl/peers.h"

#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/err.h"
#include "esp_netif_types.h"
#include "lwip/netif.h"

#include "owl/rx.h"
#include <string.h>
#include "owl/ieee80211.h"

#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/prot/tcp.h"

#define	LED_GPIO_PIN			GPIO_NUM_4
#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

static wifi_country_t wifi_country = {.cc="US", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

static void wifi_sniffer_set_channel(uint8_t channel);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

#include "core.h"
struct daemon_state state;
#include <stdatomic.h>

static atomic_int counter = 0;

void IRAM_ATTR increase_counter(void) {
    atomic_fetch_add(&counter, 1);
}
int read_and_reset_counter(void) {
    //return atomic_exchange(&counter, 0);
    return atomic_fetch_add(&counter, 0);
}

void awdl_packets_print_ps(void *pvParameters) {
    int cnt = 0;
	while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
		cnt = read_and_reset_counter();
		if (cnt==0) continue;
        ESP_LOGI("awdl packets/s","\x1B[33mcounter: %d", cnt);
    }
}

const uint8_t answer_query[46] = {
	0x08, 0x5F, 0x61, 0x69, 0x72, 0x64, 0x72, 0x6F,
	0x70, 0x04, 0x5F, 0x74, 0x63, 0x70, 0x05, 0x6c,
	0x6F, 0x63, 0x61, 0x6C, 0x00, 0x00, 0x0C, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0C, 0x39, 
	0x30, 0x39, 0x34, 0x30, 0x62, 0x34, 0x32, 0x30, 
	0x34, 0x36, 0x37, 0xC0, 0x0C
};

// void handle_mdns_packet2(uint8_t *queries, const uint16_t *packet_len);
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "awdl.h"
#include "awdl_netif.h"

#define TAG "custom_netif_example"

void send_data(esp_netif_t *netif) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 
                      0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 
                      0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 
                      0x1F, 0x20, 0x21, 0x22};

    esp_err_t err = esp_netif_transmit(netif, data, sizeof(data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send data: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Data sent successfully");
    }
}

void send_raw_tcp_packet(esp_netif_t *netif) {
	uint8_t tcp_packet[] = {
	// IPv4 Header
	0x45, // Version (4) + Internet Header Length (5)
	0x00, // Type of Service
	0x00, 0x2d, // Total Length = 20 bytes (header) + 8 bytes (TCP) + 5 bytes (data) = 0x21
	0x00, 0x00, // Identification
	0x40, 0x00, // Flags (Don't Fragment) + Fragment Offset
	0x80, // Time to Live
	0x06, // Protocol (TCP)
	0x00, 0x00, // Header Checksum (Placeholder)
	0xc0, 0xa8, 0x01, 0x64, // Source IP (192.168.1.1)
	0xc0, 0xa8, 0x01, 0x64, // Destination IP (192.168.1.100)
	// TCP Header
	0x00, 0x14, // Source Port
	0x04, 0xD2, // Destination Port
	0x00, 0x00, 0x00, 0x00, // Sequence Number
	0x00, 0x00, 0x00, 0x00, // Acknowledgment Number
	0x50, 0x02, // Data Offset (5) + Reserved + Flags (SYN)
	0x71, 0x10, // Window Size
	0x6a, 0xac, // Checksum (Placeholder)
	0x00, 0x00, // Urgent Pointer
	// data
	0x68, 0x65, 
	0x6c, 0x6c, 
	0x6f
	};
	size_t tcp_packet_len = sizeof(tcp_packet);

	struct pbuf *p2 = pbuf_alloc(PBUF_TRANSPORT, tcp_packet_len, PBUF_RAM);
    if (!p2) {
        ESP_LOGE(TAG, "Failed to allocate pbuf2");
        return;
    }
    // Copy data to pbuf
    pbuf_take(p2, tcp_packet, tcp_packet_len);

	ESP_LOGI(TAG, "send raw tcp packet");
    // Send the packet
    esp_err_t err = esp_netif_receive(netif, p2, tcp_packet_len,NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send packet: %s", esp_err_to_name(err));
    }

    // Free the pbuf
    pbuf_free(p2);
}


void send_data_loop(void* arg) {
	esp_netif_t* lowpan6_ble_netif = (esp_netif_t*)arg;
    while (1) {
		printf("\n\n\n\n\n");
        send_data(lowpan6_ble_netif);
		//send_raw_tcp_packet(lowpan6_ble_netif);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void wifi_sniffer_init(void)
{
	nvs_flash_init();
    	esp_netif_init();
    	//ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country)); /* set country for channel range [1, 13] */
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM));
    	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
    	ESP_ERROR_CHECK( esp_wifi_start() );
	wifi_promiscuous_filter_t wifi_sniffer_filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
        };
	esp_wifi_set_promiscuous_filter(&wifi_sniffer_filter);
	esp_wifi_set_promiscuous_ctrl_filter(&wifi_sniffer_filter);
	esp_wifi_set_promiscuous(true);
	struct ether_addr mac;
	esp_err_t ret = esp_wifi_get_mac(ESP_IF_WIFI_STA, (uint8_t *)&mac);
	if (ret != ESP_OK) {
		ESP_LOGE("awdl", "esp_wifi_get_mac failed; err=%d", ret);
		return;
	}
	ESP_LOGI("awdl", "mac: %02x:%02x:%02x:%02x:%02x:%02x", mac.ether_addr_octet[0], mac.ether_addr_octet[1], mac.ether_addr_octet[2], mac.ether_addr_octet[3], mac.ether_addr_octet[4], mac.ether_addr_octet[5]);

	awdl_init_state(&state.awdl_state, "test", &mac, CHAN_OPCLASS_6, clock_time_us());

	state.awdl_state.peer_cb = awdl_neighbor_add;
	state.awdl_state.peer_remove_cb = awdl_neighbor_remove;

	// ieee80211_init_state
	state.ieee80211_state.sequence_number = 0;
	state.ieee80211_state.fcs = 0;

	state.next = NULL;
	state.tx_queue_multicast = circular_buf_init(16);
	state.dump = 0;

	state.awdl_state.filter_rssi = 0;

	awdl_schedule(&state);

	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
	wifi_sniffer_set_channel(6);
	
	//awdl netif

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_AWDL();
    esp_netif_config_t cfg_awdl = {
        .base   = &base,
        .driver = NULL,
        .stack  = netstack_default_awdl,
    };
    esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg_awdl);

    awdl_driver_handle lowpan6_ble_driver = awdl_create(&state.awdl_state);
    if (lowpan6_ble_driver != NULL)
    {
        ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
    }
	
    struct netif* netif = esp_netif_get_netif_impl(lowpan6_ble_netif);
	netif_set_up(netif);
    netif_set_link_up(netif);
	while (!esp_netif_is_netif_up(lowpan6_ble_netif))
    {
        ESP_LOGI(TAG, "netif not up, waiting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // Set IPv4 address
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 100);    // Set your desired IP here
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);      // Set your desired Gateway here
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Set your desired Netmask here
    esp_netif_set_ip_info(lowpan6_ble_netif, &ip_info);

	xTaskCreate(send_data_loop, "send_data_loop", 8096, lowpan6_ble_netif, 1, NULL);
	
    // xTaskCreate(tcp_server_task, "tcp_server", 8096, NULL, 5, NULL);
	// https://github.com/geonavo/lowpan6_ble/blob/main/examples/echo/server/main/main.c#L197
	// print data received from the custom netif
	// bind to port etc it is filtered by netstack
	// test mdns or simple tcp packets
	// esp_netif_receive(netif, data, len); is for the driver side of the netif to send data from air to software


    esp_log_level_set("lwip", ESP_LOG_DEBUG);
	//setup_raw_recv_callback(netif);
}

void
wifi_sniffer_set_channel(uint8_t channel)
{
	
	esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

// from owl/deamon/core.c
struct buf {
	const uint8_t *orig;
	uint8_t *data;
	int len;
	int owned;
};

void awdl_receive_frame(const uint8_t *buf, int len) {
#define MAX_NUM_AMPDU 16 /* TODO lookup this constant from the standard */
	int result;
	const struct buf *frame = buf_new_const(buf, len);
	struct buf *data_arr[MAX_NUM_AMPDU];
	struct buf **data = &data_arr[0];
	result = awdl_rx(frame, &data, &state.awdl_state);
	if (result == RX_OK) {
		//ESP_LOGI("wifi", "awdl_receive_frame");
	} else if (result < RX_OK) {
		ESP_LOGW("awdl core", "unhandled frame (%d)", result);
		state.awdl_state.stats.rx_unknown++;
	} else {
		ESP_LOGE("awdl core", "awdl_receive_frame: unidentified frame %i",result);
	}
	buf_free(frame);

	// ether type https://en.wikipedia.org/wiki/EtherType#Values
	// ipv6 header https://github.com/espressif/esp-idf/blob/e7070e777a079695f69720ffb3c631c5fe620cc6/components/openthread/src/port/esp_openthread_udp.c#L153
	
	struct buf **data_start = &data_arr[0];
	if (data_start < data) {
		ESP_LOGI("awdl", "received data packet:"); 
		struct awdl_packet *packet = (struct awdl_packet *)buf_data(*data_start);
		packet->ether_type = EndianConvert16(packet->ether_type);
		packet->len = EndianConvert16(packet->len);
		for (int i = 0; i < packet->len; i++) {
    		printf("%02x ", packet->data[i]);
		}
		printf("src: %s; ", ether_ntoa(&packet->src));
		printf("dst: %s; ", ether_ntoa(&packet->dst));
		printf("ether type: %04x; ", packet->ether_type);
		printf("len: %i; data: \n",packet->len);
		//for (int i=0; i<packet.len; i++) {
		//	printf("%02X ", ((uint8_t *)&packet.data)[i]);
		//}
		buf_free(*data_start);
	}
}

#include "owl/rx.h"

// example: https://github.com/pulkin/esp8266-injection-example
// filter: https://github.com/espressif/esp-idf/issues/1266
// send: https://gist.github.com/shekkbuilder/768189e59575b9ec80664b69242afffd
// linux interfaces: https://github.com/lattera/glibc/blob/master/inet/netinet/ether.h

//static const char *pkt_types[] = {
//	"Management frame",
//	"Control frame",
//	"Data frame",
//	"Other type, such as MIMO etc",
//};

void
wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
	if (type<0 || type>3) {
		printf("Unknown type %d\n", type);
		return;
	}
	//printf("type: %s\n", pkt_types[type]);
	
	// https://github.com/espressif/esp-idf/blob/0479494e7abe5aef71393fba2e184b3a78ea488f/components/esp_wifi/include/local/esp_wifi_types_native.h#L86
	//if (type != WIFI_PKT_MGMT)
	//	return;

	const wifi_pkt_t *wifi_pkt = (wifi_pkt_t *)buff;
	uint8_t awdl_data_mac[6] = { 0x00, 0x25, 0x00, 0xff, 0x94, 0x73 };
	if (memcmp(wifi_pkt->hdr.addr3,awdl_data_mac, sizeof(struct ether_addr))!=0) {
		return;
	}
	//subtype
	//printf("subtype: %i\n", ipkt->payload[06]);
	
	const uint8_t *addr1 = wifi_pkt->hdr.addr1;
	if (addr1[0]!=0xff) {
		printf("ADDR1=%02x:%02x:%02x:%02x:%02x:%02x ",
			addr1[0],addr1[1],addr1[2],
			addr1[3],addr1[4],addr1[5]
		);
	}
	awdl_receive_frame((const uint8_t *)buff,sizeof(wifi_pkt_rx_ctrl_t)+wifi_pkt->rx_ctrl.sig_len);
	return;
}
