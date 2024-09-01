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
#include "esp_netif.h"

#include "owl/rx.h"
#include <string.h>
#include "owl/ieee80211.h"

#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/prot/tcp.h"
#include "netdb.h"
#include "mdns.h"
#include "esp_netif_ip_addr.h"
#include "esp_mac.h"
#include "protocol_examples_common.h"
#include "cli/tasks.h"

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

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "awdl.h" 
#include "awdl_netif.h"

#define TAG "custom_netif_example"

void send_data_test(esp_netif_t *netif) {
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
		printf("\n");
        send_data_test(lowpan6_ble_netif);
		//send_raw_tcp_packet(lowpan6_ble_netif);
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
}

static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};
static void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
	printf("mdns_print_results\n");
	while (r) {
        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}

static void query_mdns_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 5000, 20,  &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }
	ESP_LOGE("awdl", "mdns_print_results\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    mdns_print_results(results);

	esp_ip6_addr_t rcv_addr;
	if (results->addr) {
        mdns_ip_addr_t *a = results->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
				print_esp_ip6_addr(a->addr.u_addr.ip6);
				memcpy(&rcv_addr, &a->addr.u_addr.ip6, sizeof(esp_ip6_addr_t));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
	} else if (results->hostname) {
		err = mdns_query_aaaa(results->hostname, 5000, &rcv_addr);
		printf("error: %s\n", esp_err_to_name(err));
		print_esp_ip6_addr(rcv_addr);
		printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
	} else if (results->instance_name) {
		ESP_LOGE("mdns","only ptr");
    	mdns_query_results_free(results);
		err = mdns_query_srv(results->instance_name, results->service_type, results->proto, 5000,  &results);
		printf("error: %s\n", esp_err_to_name(err));
		mdns_print_results(results);
		if (results->hostname) {
			err = mdns_query_aaaa(results->hostname, 5000, &rcv_addr);
			printf("error: %s\n", esp_err_to_name(err));
			print_esp_ip6_addr(rcv_addr);
		}
	}
    mdns_query_results_free(results);
}


void mdns_query_task(void *pvParameters) {
	while (1) {
		ESP_LOGE("awdl", "mdns_query_task");
		query_mdns_service("_airdrop", "_tcp");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void wifi_sniffer_init(struct availabeTasks *tasks)
{
	nvs_flash_init();
    	esp_netif_init();
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
	state.awdl_state.peer_cb_data = &state;
	state.awdl_state.peer_remove_cb = awdl_neighbor_remove;
	state.awdl_state.peer_remove_cb_data = &state;

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
	//netif_set_flags(lowpan6_ble_netif, NETIF_FLAG_MLD6);
	//lowpan6_ble_netif->flags |= NETIF_FLAG_MLD6;
	esp_netif_create_ip6_linklocal(lowpan6_ble_netif);

    awdl_driver_handle lowpan6_ble_driver = awdl_create(&state);
    if (lowpan6_ble_driver != NULL)
    {
        ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
    }
	
    struct netif* netif = esp_netif_get_netif_impl(lowpan6_ble_netif);
	netif->flags |= NETIF_FLAG_MLD6;
	netif_set_up(netif);
    netif_set_link_up(netif);
	while (!esp_netif_is_netif_up(lowpan6_ble_netif))
    {
        ESP_LOGI(TAG, "netif not up, waiting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
	/*
    // Set IPv4 address
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 100);    // Set your desired IP here
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);      // Set your desired Gateway here
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // Set your desired Netmask here
    esp_netif_set_ip_info(lowpan6_ble_netif, &ip_info);

    esp_netif_ip6_info_t ip6_info;
    ip6addr_aton(STATIC_IPV6_ADDR, &ip6_info.ip);
    ESP_ERROR_CHECK(esp_netif_set_ip6_info(netif, &ip6_info));
	*/
	struct in6_addr ip6_addr = ether_addr_to_in6_addr((struct ether_addr *)&mac);
    ESP_ERROR_CHECK(netif_add_ip6_address(netif, (ip6_addr_t *)&ip6_addr, 0));
    esp_netif_dns_info_t dns_info;
	memcpy(&dns_info.ip.u_addr.ip6.addr, &ip6_addr, sizeof(struct in6_addr));
    dns_info.ip.type = IPADDR_TYPE_V6;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(lowpan6_ble_netif, ESP_NETIF_DNS_MAIN, &dns_info)); 

	//xTaskCreate(send_data_loop, "send_data_loop", 8096, lowpan6_ble_netif, 10, NULL);
	
    // xTaskCreate(tcp_server_task, "tcp_server", 8096, NULL, 5, NULL);
	// https://github.com/geonavo/lowpan6_ble/blob/main/examples/echo/server/main/main.c#L197
	// print data received from the custom netif
	// bind to port etc it is filtered by netstack
	// test mdns or simple tcp packets
	// esp_netif_receive(netif, data, len); is for the driver side of the netif to send data from air to software


    esp_log_level_set("lwip", ESP_LOG_DEBUG);
    esp_log_level_set("awdl", ESP_LOG_DEBUG);

	//setup_raw_recv_callback(netif);
	
    ESP_ERROR_CHECK( mdns_init() );
	mdns_register_netif(lowpan6_ble_netif);

    ESP_ERROR_CHECK(mdns_netif_action(lowpan6_ble_netif, MDNS_EVENT_ENABLE_IP6));
    ESP_ERROR_CHECK(mdns_netif_action(lowpan6_ble_netif, MDNS_EVENT_ANNOUNCE_IP6));
    ESP_ERROR_CHECK(mdns_netif_action(lowpan6_ble_netif, MDNS_EVENT_IP6_REVERSE_LOOKUP));
	xTaskCreate(mdns_query_task, "mdns_query_task", 8096, NULL, 5, tasks->mdns);
	tasks->mdns_enabled = true;
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
	esp_log_level_set("awdl_rx_data", ESP_LOG_VERBOSE);
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
		printf("src: %s; ", ether_ntoa(&packet->src));
		printf("dst: %s; ", ether_ntoa(&packet->dst));
		printf("ether type: %04x; ", packet->ether_type);
		printf("len: %i; data: \n",packet->len);

		esp_netif_t *netif = esp_netif_get_handle_from_ifkey("AWDL_DEF");
		if (netif == NULL) {
			ESP_LOGE("awdl", "Failed to get netif");
			return;
		}
		esp_netif_receive(netif, packet->data, packet->len, NULL);
		buf_free(*data_start);
	}
	// esp_netif_receive() send the data to the network stack
}

#include "owl/rx.h"

// example: https://github.com/pulkin/esp8266-injection-example
// filter: https://github.com/espressif/esp-idf/issues/1266
// send: https://gist.github.com/shekkbuilder/768189e59575b9ec80664b69242afffd
// linux interfaces: https://github.com/lattera/glibc/blob/master/inet/netinet/ether.h

void
wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{
	if (type<0 || type>3) {
		printf("Unknown type %d\n", type);
		return;
	}
	
	const wifi_pkt_t *wifi_pkt = (wifi_pkt_t *)buff;
	uint8_t awdl_data_mac[6] = { 0x00, 0x25, 0x00, 0xff, 0x94, 0x73 };
	if (memcmp(wifi_pkt->hdr.addr3,awdl_data_mac, sizeof(struct ether_addr))!=0) {
		return;
	}
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
