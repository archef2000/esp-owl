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
//#include "lwip/ip6_addr.h"
//#include "esp_netif.h"
#include "lwip/netif.h"

#define	LED_GPIO_PIN			GPIO_NUM_4
#define	WIFI_CHANNEL_MAX		(13)
#define	WIFI_CHANNEL_SWITCH_INTERVAL	(500)

static wifi_country_t wifi_country = {.cc="US", .schan=1, .nchan=13, .policy=WIFI_COUNTRY_POLICY_AUTO};

//static esp_err_t event_handler(void *ctx, system_event_t *event);
//static void wifi_sniffer_init(void);
static void wifi_sniffer_set_channel(uint8_t channel);
/*
static const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type);
*/
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

//void
//app_main(void)
//{
//	uint8_t level = 0, channel = 6;
//	/* setup */
//	wifi_sniffer_init();
//	//gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
//	wifi_sniffer_set_channel(channel);
//	///* loop */
//	//while (true) {
//	//	gpio_set_level(LED_GPIO_PIN, level ^= 1);
//	//	vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
//	//	channel = (channel % WIFI_CHANNEL_MAX) + 1;
//    //	}
//}

//esp_err_t
//event_handler(void *ctx, system_event_t *event)
//{
//	
//	return ESP_OK;
//}
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

// Function to send data
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

#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/prot/tcp.h"

void send_raw_tcp_packet(esp_netif_t *netif) {
	/*
    // Destination IP address
    ip_addr_t dest_ip;
    IP_ADDR4(&dest_ip, 192, 168, 1, 100);
    // Data to send
    char *data = "hello";
    size_t data_len = strlen(data);
    // Create a new pbuf for our data
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, data_len, PBUF_RAM);
    if (!p) {
        ESP_LOGE(TAG, "Failed to allocate pbuf");
        return;
    }
    // Copy data to pbuf
    pbuf_take(p, data, data_len);
    // Create a raw TCP header
    struct tcp_hdr *tcphdr = p->payload;
    tcphdr->src = htons(12345); // Source port
    tcphdr->dest = htons(12234); // Destination port
    tcphdr->seqno = htonl(1); // Sequence number
    tcphdr->ackno = 0; // Acknowledgment number
    TCPH_HDRLEN_SET(tcphdr, 5); // Header length
    //tcphdr->flags = TCP_SYN; // TCP flags
    tcphdr->wnd = htons(TCP_WND); // Window size
    tcphdr->chksum = 0; // Checksum (to be filled in by lwIP)
    tcphdr->urgp = 0; // Urgent pointer
	*/


uint8_t tcp_packet[] = {
	// ether header
//// src
//0x80, 0x65, 0x99, 0xc7, 0xae, 0x98,
//// dst
//0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
//// ether type
//0x0800,

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

	//data
0x68, 0x65, 
0x6c, 0x6c, 
0x6f
};
/*
uint8_t tcp_packet2[] = {
	// ether header
//// src
//0x80, 0x65, 0x99, 0xc7, 0xae, 0x98,
//// dst
//0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
//// ether type
//0x0800,

// ip header
0x45, 
0x00, 
0x00, 0x28, 
0x00, 0x00, 
0x40, 0x00, 
0x80, 
0x06, 
0x00, 0x00, 
0xc0, 0xa8, 0x01, 0x01, 
0xdc, 0x4f, 0x01, 0x64, 
// tcp header
0x00, 0x14, 
0x04, 0xd2, 
0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 
0x50, 0x02, 
0x71, 0x10, 
0x00, 0x00, 
0x00, 0x00, 
0x68, 0x65, 
0x6c, 0x6c, 
0x6f
};
*/
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
		send_raw_tcp_packet(lowpan6_ble_netif);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

#define PORT 1234
// Define your callback function
err_t my_tcp_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
	ESP_LOGI(TAG, "Received packet %d", ntohs(p->tot_len));
	for (int i = 0; i < p->tot_len; i++) {
		printf("%02x ", ((uint8_t *)p->payload)[i]);
	}
	printf("\n");
	pbuf_free(p);
    return ERR_OK; // Return 0 to indicate that the packet has not been consumed
}
/*
// user-provided receive callback function raw pcb for a netif
void setup_raw_recv_callback(struct netif *netif) {
    struct tcp_pcb *pcb = tcp_new(); // Create a new raw PCB for ICMP protocol
	ip_addr_t *ipaddr  = IPADDR4_INIT_BYTES(0,0,0,0);
	IP_ADDR4(ipaddr, 192, 168, 1, 100);
	//IP4_ADDR(ip_2_ip4(ipaddr), 192, 168, 1, 101);
    if (pcb != NULL) {
        tcp_recv(pcb, my_tcp_recv_callback); // Set the receive callback function
        tcp_bind(pcb, ipaddr, 1234); // Bind the PCB to the netif's IP address and a specific port
    }
}
*/
/*
void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {
        ESP_LOGI(TAG, "Waiting for a connection...");

        struct sockaddr_in source_addr;
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
		ESP_LOGI(TAG, "Connection accepted");

        // Handle received data
        do {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
            } else if (len == 0) {
                ESP_LOGW(TAG, "Connection closed");
            } else {
                rx_buffer[len] = 0;
                ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
            }
        } while (1);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}
*/

void
wifi_sniffer_init(void)
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

	//xTaskCreate(awdl_packets_print_ps, "awdl_packets_print_ps", 2048, NULL, 1, NULL);
	esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
	

	//ESP_ERROR_CHECK(esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE));
	//ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_BELOW));
    //esp_wifi_set_channel(11, WIFI_SECOND_CHAN_ABOVE);
	//esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
	wifi_sniffer_set_channel(6);
	
	//int packet_len = sizeof(answer_query);
	//handle_mdns_packet2(answer_query, &packet_len);

	//awdl netif

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_AWDL();
    esp_netif_config_t cfg_awdl = {
        .base   = &base,
        .driver = NULL,
        .stack  = netstack_default_awdl,
    };
    esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg_awdl);

    awdl_driver_handle lowpan6_ble_driver = awdl_create();
    if (lowpan6_ble_driver != NULL)
    {
        ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
    }
	
	// Finally, register our `lowpan6_ble_driver` as an L2CAP server. This will hook L2CAP events
    // into the required driver events.
    awdl_create_server(lowpan6_ble_driver, NULL, NULL);

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

	//xTaskCreate(send_data_loop, "send_data_loop", 8096, lowpan6_ble_netif, 1, NULL);
	
    //xTaskCreate(tcp_server_task, "tcp_server", 8096, NULL, 5, NULL);
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
/*

const char *
wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type)
{
	switch(type) {
	case WIFI_PKT_MGMT: return "MGMT";
	case WIFI_PKT_DATA: return "DATA";
	default:	
	case WIFI_PKT_MISC: return "MISC";
	}
}
*/

#include "owl/rx.h"
#include <string.h>
#include "owl/ieee80211.h"
// from owl/deamon/core.c

struct buf {
	const uint8_t *orig;
	uint8_t *data;
	int len;
	int owned;
};

//#include "lwip/ip6.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

struct iphdr {
    uint8_t traffic_class:4;
    uint8_t version:4;
    uint8_t traffic_class2:4;
    uint32_t flow_label:20;
    uint16_t len;
    uint8_t type; // next header (UDP, TCP, etc.)
    uint8_t ttl; // hop limit
    uint32_t source;
    uint32_t dest;
};
// 70
struct tcphdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
	union {
		struct {
    		uint16_t res1:4;
    		uint16_t doff:4;
		};
		uint8_t doff_res;
	};
	union {
		uint8_t flags;
		struct {
			uint16_t fin:1;
			uint16_t syn:1;
			uint16_t rst:1;
			uint16_t psh:1;
			uint16_t ack:1;
			uint16_t urg:1;
			uint16_t res:2;
		};
	};
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
};

//#include <stdio.h>
//#include <stdint.h>
//#include <arpa/inet.h> // For ntohl, ntohs, and inet_ntop
//
//// Define your ip6_hdr structure
//struct ip6_hdr {
//    uint32_t _v_tc_fl; // version / traffic class / flow label
//    uint16_t _plen;    // payload length
//    uint8_t _nexth;    // next header
//    uint8_t _hoplim;   // hop limit
//    struct in6_addr src; // source address
//    struct in6_addr dest; // destination address
//};
//// Define the TCP header structure
//struct tcphdr {
//    uint16_t source;   // source port
//    uint16_t dest;     // destination port
//    uint32_t seq;      // sequence number
//    uint32_t ack_seq;  // acknowledgment number
//    uint8_t doff_res;  // data offset and reserved
//    uint8_t flags;     // flags
//    uint16_t window;   // window size
//    uint16_t check;    // checksum
//    uint16_t urg_ptr;  // urgent pointer
//};

void handle_tcp_packet(uint8_t *packet, const uint16_t *packet_len, const struct in6_addr *src, const struct in6_addr *dst) {
	printf("tcp packet\n");
	// Cast the data pointer to a TCP header structure
	const struct tcphdr *tcp_header = (const struct tcphdr *)packet;
	// Print the TCP header fields
	printf("Source Port: %d\n", ntohs(tcp_header->source));
	printf("Destination Port: %d\n", ntohs(tcp_header->dest));
	printf("Sequence Number: %lu\n", ntohl(tcp_header->seq));
	printf("Acknowledgment Number: %lu\n", ntohl(tcp_header->ack_seq));
	printf("Data Offset: %d\n", (tcp_header->doff_res >> 4) & 0xF);
	printf("Flags: %d\n", tcp_header->flags);
	printf("Window Size: %d\n", ntohs(tcp_header->window));
	printf("Checksum: %d\n", ntohs(tcp_header->check));
	printf("Urgent Pointer: %d\n", ntohs(tcp_header->urg_ptr));
	//const struct tcphdr *tcp_header = (const struct tcphdr *)packet->data;
	//*payload_len = ntohs(tcp_header->doff*4) - sizeof(struct tcphdr);
	//*src = packet->src;
	//*dst = packet->dst;
}

struct mdns_packet_t {
	uint16_t id;
	uint16_t flags;
	uint16_t num_questions;
	uint16_t num_answers;
	uint16_t num_authorities;
	uint16_t num_additionals;
	uint8_t queries[0];
};
//struct {
//	uint8_t questions[0];
//	uint8_t answers[0];
//	uint8_t authorities[0];
//	uint8_t additionals[0];
//};

// Function to extract the length of the qname
size_t get_mdns_qname_length(const uint8_t *packet, uint8_t *qname) {
    // Start at the beginning of the packet
    const uint8_t *ptr = packet;
	// check if frist and second byte are 0xc0 and 0x0c
	if (*ptr == 0xc0 && *(ptr+1) == 0x0c) {
		return 2;
	}

    // Initialize the qname length
    size_t qname_length = 0;
    // Loop until we reach the end of the qname (indicated by a zero-length label)
    while (*ptr != 0) {
		if (qname_length!=0) {
			qname [qname_length] = 0x05;
			qname_length++;
		}
        // Read the label length
        size_t label_length = *ptr;
		if (label_length==0xc0) {
			qname[qname_length] = 0x05;
		}
		for (int i = 0; i < label_length; i++) {
			printf("name: %02x\n", packet[i+1]);
			qname[qname_length] = packet[i+1];
			qname_length++;
		}
        // Move to the next label
		printf("label_length: %d\n", label_length);
        ptr += label_length+1; // +1 to skip the length byte
    }
    // Add 1 to account for the final zero-length label
    qname_length++;
    return qname_length;
}
struct mdns_questions_t {
	uint8_t name[256];
	uint16_t type;
	uint16_t class;
};

struct mdns_answers_t {
	uint8_t name[256];
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t data_len;
	uint8_t data[256];
};
/*
//#include <lwip/udp.h>
void handle_mdns_packet2(uint8_t *queries, const uint16_t *packet_len) {
	printf("mdns packet\n");
	printf("packet_len: %d\n", *packet_len);
	for (int j = 0; j < *packet_len; j++) {
		printf("%02x ", queries[j]); 
	}
	printf("\n");
	struct mdns_answers_t answers[1];
	for (int i = 0; i < 1; i++) {
		int len = get_mdns_qname_length(queries,answers[i].name);
		printf("len: %d\n", len);
		for (int o = 0; o < len; o++) {
			printf("%02x ", answers[i].name[o]);
		}
		memcpy(&answers[i].type, queries+len, 2);
		answers[i].type = ntohs(answers[i].type);
		memcpy(&answers[i].class, queries+len+2, 2);
		answers[i].class = ntohs(answers[i].class);
		memcpy(&answers[i].ttl, queries+len+4, 4);
		answers[i].ttl = ntohs(answers[i].ttl);
		memcpy(&answers[i].data_len, queries+len+8, 2);
		answers[i].data_len = ntohs(answers[i].data_len);
		for (int o = 0; o < answers[i].data_len; o++) {
			answers[i].data[o] = queries[len+10+o];
		}
		printf("question: name= %s, type=%d, class=%d, ttl=%ld, data_len=%d, data= %s\n", answers[i].name, answers[i].type, answers[i].class, answers[i].ttl, answers[i].data_len, answers[i].data);
		queries += len+10+answers[i].data_len;
	}
	printf("finish answers\n");
}

void handle_mdns_packet(uint8_t *packet, const uint16_t *packet_len) {
	// 00 00 00 00 00 01 00 00 00 00 00 00 08 5f 61 69 72 64 72 6f 70 04 5f 74 63 70 05 6c 6f 63 61 6c 00 00 0c 00 01
	printf("mdns packet\n");
	printf("packet_len: %d\n", *packet_len);
	//for (int i = 0; i < *packet_len-sizeof(struct udp_hdr); i++) {
	//	printf("%02x ", packet[i]);
	//}
	const struct mdns_packet_t *mdns_packet = (const struct mdns_packet_t *)packet;
	printf("id: %d\n", ntohs(mdns_packet->id));
	printf("flags: %d\n", ntohs(mdns_packet->flags));
	printf("num_questions: %d\n", ntohs(mdns_packet->num_questions));
	printf("num_answers: %d\n", ntohs(mdns_packet->num_answers));
	printf("num_authorities: %d\n", ntohs(mdns_packet->num_authorities));
	printf("num_additionals: %d\n", ntohs(mdns_packet->num_additionals));
	struct mdns_questions_t questions[ntohs(mdns_packet->num_questions)];
	for (int i = 0; i < ntohs(mdns_packet->num_questions); i++) {
		printf("%02x ", mdns_packet->queries[i]);
	}
	uint8_t *queries = (uint8_t *)mdns_packet->queries;

	for (int j = 0; j < *packet_len-sizeof(struct udp_hdr)-12; j++) {
		printf("%02x ", queries[j]); 
	}
	printf("\n");
	for (int i = 0; i < ntohs(mdns_packet->num_questions); i++) {
		int len = get_mdns_qname_length(queries,questions[i].name);
		printf("len: %d\n", len);
		//memcpy(&questions[i].name, queries, len);
		//for (int o = 0; o < len; o++) {
		//	if (questions[i].name[o] == 0x05) {
		//		questions[i].name[o] = 0x2E;
		//	}
		//	//printf("%02x ", questions[i].name[o]);
		//}
		printf("\n");
		memcpy(&questions[i].type, queries+len, 2);
		questions[i].type = ntohs(questions[i].type);
		memcpy(&questions[i].class, queries+len+2, 2);
		questions[i].class = ntohs(questions[i].class);
		printf("question: name= %s, type=%d, class=%d\n", questions[i].name, questions[i].type, questions[i].class);
		queries += len+4;
	}
	printf("finish questions\n");

	struct mdns_answers_t answers[ntohs(mdns_packet->num_answers)];
	for (int i = 0; i < ntohs(mdns_packet->num_answers); i++) {
		int len = get_mdns_qname_length(queries,answers[i].name);
		printf("len: %d\n", len);
		//memcpy(&answers[i].name, queries, len);
		for (int o = 0; o < len; o++) {
			//if (answers[i].name[o] == 0x05) {
			//	answers[i].name[o] = 0x2E;
			//}
			printf("%02x ", answers[i].name[o]);
		}
		memcpy(&answers[i].type, queries+len, 2);
		answers[i].type = ntohs(answers[i].type);
		memcpy(&answers[i].class, queries+len+2, 2);
		answers[i].class = ntohs(answers[i].class);
		memcpy(&answers[i].ttl, queries+len+4, 4);
		answers[i].ttl = ntohs(answers[i].ttl);
		memcpy(&answers[i].data_len, queries+len+8, 2);
		answers[i].data_len = ntohs(answers[i].data_len);
		for (int o = 0; o < answers[i].data_len; o++) {
			answers[i].data[o] = queries[len+10+o];
		}
		printf("question: name= %s, type=%d, class=%d, ttl=%ld, data_len=%d, data= %s\n", answers[i].name, answers[i].type, answers[i].class, answers[i].ttl, answers[i].data_len, answers[i].data);
		queries += len+10+answers[i].data_len;
	}
	printf("finish answers\n");
}

void handle_udp_packet(uint8_t *packet, const uint16_t *packet_len, const struct in6_addr *src, const struct in6_addr *dst) {
	printf("udp packet\n");
	printf("len: %d\n", *packet_len);
	for (int i = 0; i < sizeof(struct udp_hdr); i++) {
		printf("%02x ", packet[i]);
	}
	const struct udp_hdr *udp_header = (const struct udp_hdr *)packet;

	printf("Source Port: %d\n", ntohs(udp_header->src));
	printf("Destination Port: %d\n", ntohs(udp_header->dest));
	printf("Length: %d\n", ntohs(udp_header->len));
	printf("Checksum: %04x\n", ntohs(udp_header->chksum));
	if (ntohs(udp_header->src)!=5353||ntohs(udp_header->dest)!=5353) {
		ESP_LOGE("awdl", "udp packet not MDNS");
		return;
	}
	printf("udp_hdr size: %d\n", sizeof(struct udp_hdr));
	handle_mdns_packet(packet+sizeof(struct udp_hdr), packet_len);
	
	//// Cast the data pointer to a UDP header structure
	//const struct udphdr *udp_header = (const struct udphdr *)packet;
	//// Print the UDP header fields
	//printf("Source Port: %d\n", ntohs(udp_header->source));
	//printf("Destination Port: %d\n", ntohs(udp_header->dest));
	//printf("Length: %d\n", ntohs(udp_header->len));
	//printf("Checksum: %d\n", ntohs(udp_header->check));
}

void handle_ip_packet(struct awdl_packet *packet) {
	printf("ip packet\n");

	const struct ip6_hdr *ipv6_header = (const struct ip6_hdr *)&packet->data;
    uint32_t v_tc_fl = ntohl(ipv6_header->_v_tc_fl);
    uint8_t version = (v_tc_fl >> 28) & 0xF;
    uint8_t traffic_class = (v_tc_fl >> 20) & 0xFF;
    uint32_t flow_label = v_tc_fl & 0xFFFFF;
	uint16_t payload_len = ntohs(ipv6_header->_plen);
    // Access fields of the IPv6 header
    printf("Version: %d\n", version);
    printf("Traffic Class: %d\n", traffic_class);
    printf("Flow Label: %ld\n", flow_label); 
    printf("Payload Length: %d\n", ntohs(ipv6_header->_plen));
    printf("Next Header: %d\n", ipv6_header->_nexth);
    printf("Hop Limit: %d\n", ipv6_header->_hoplim);

    char src_addr[INET6_ADDRSTRLEN];
    char dst_addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ipv6_header->src, src_addr, sizeof(src_addr));
    inet_ntop(AF_INET6, &ipv6_header->dest, dst_addr, sizeof(dst_addr));

    printf("Source Address: %s\n", src_addr);
    printf("Destination Address: %s\n", dst_addr);

	switch (ipv6_header->_nexth) {
		case IPPROTO_UDP:
			printf("UDP\n");
			handle_udp_packet(packet->data+sizeof(struct ip6_hdr), &payload_len, &ipv6_header->src, &ipv6_header->dest);
			break;
		case IPPROTO_TCP:
			printf("TCP\n");
			handle_tcp_packet(packet->data+sizeof(struct ip6_hdr), &payload_len, &ipv6_header->src, &ipv6_header->dest);
			break;
		default:
			printf("unknown protocol\n");
			break;
	}
	//uint8_t *payload = packet->data + sizeof(struct iphdr);
	//uint16_t payload_len = packet->len - sizeof(struct iphdr);
	//if (ip_header->protocol == IPPROTO_UDP) {
	//	struct udphdr *udp_header = (struct udphdr *)payload;
	//	uint16_t udp_len = ntohs(udp_header->len);
	//	if (udp_len > payload_len) {
	//		ESP_LOGE("awdl", "UDP packet too short");
	//		return;
	//	}
	//	payload += sizeof(struct udphdr);
	//	payload_len -= sizeof(struct udphdr);
	//	if (udp_header->source == 1234) {
	//		ESP_LOGI("awdl", "UDP packet from port 1234");
	//	}
	//}
}
*/
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
	printf("\n1\n");
	//for (int i=0; i<250; i++) {
	//	printf("%02x ", ((uint8_t *)buf_data(*data_start))[i]);
	//}
	//printf("\n2\n");
	//const uint8_t *data = buf_data(*data_start);
	//for (int i=0; i<250; i++) {
	//	printf("%02x ", data[i]);
	//}
	//struct awdl_packet *packet2 = (struct awdl_packet *)data;
	//packet2.dst = (struct ether_addr *)data;
	//packet2.src = (struct ether_addr *)(data+6);
	//packet2.ether_type = EndianConvert16(*(uint16_t *)(data+12));
	//packet2.len = EndianConvert16(*(uint16_t *)(data+14));
	//packet2.data = data+16;
	struct awdl_packet *packet = (struct awdl_packet *)buf_data(*data_start); // (uint8_t *)data;
	//printf("\n3\n");
	////for (int i=0; i<1500; i++) {
	////	printf("%02X:", ((uint8_t *)buf_data(*data_start))[i]);
	////}
	//for (int i=0; i<250; i++) {
	//	printf("%02x ", ((uint8_t *)&packet2)[i]);
	//}
	//for (int i = 0; i < 250; i++) {
    //	printf("%02x ", ((uint8_t *)&packet2)[i]);
	//}
	//printf("\n3.5\n");
	packet->ether_type = EndianConvert16(packet->ether_type);
	packet->len = EndianConvert16(packet->len);
	for (int i = 0; i < packet->len; i++) {
    	printf("%02x ", packet->data[i]);
	}
	//printf("\n4\n");
	//for (int i=0; i<250; i++) {
	//	printf("%02x ", ((uint8_t *)packet2->data)[i]);
	//}
	

	printf("src: %s; ", ether_ntoa(&packet->src));
	printf("dst: %s; ", ether_ntoa(&packet->dst));
	printf("ether type: %04x; ", packet->ether_type);
	printf("len: %i; data: \n",packet->len);
	//for (int i=0; i<packet.len; i++) {
	//	printf("%02X ", ((uint8_t *)&packet.data)[i]);
	//}
	//handle_ip_packet(packet);
	buf_free(*data_start);
	}


	//buf_free(*data_start);


	//struct buf *buf_ptr = *data;
	//struct awdl_packet *packet_ptr = (struct awdl_packet *)buf_ptr->data;
	//printf("ether type: %04x\n", packet_ptr->ether_type);
	//int i;
	////print content of packet_ptr as hex
	////for (i=0; i<buf_ptr->len; i++) {
	////	printf("%02x ", buf_ptr->data[i]);
	////}
	//for (struct buf **data_start2 = &data_arr[0]; data_start2 < data; data_start2++) {
	//	buf_free(*data_start2);
	//	for (i=0; i<buf_len(*data_start2); i++) {
	//		printf("%02x ", buf_data(*data_start2)[i]);
	//	}
	//	printf("\n");
	//}
	//}


	//struct ip_header {
    //	uint8_t version; // IP version (always 4 for IPv4)
    //	uint8_t ihl;     // Header length in 32-bit words
    //	// Other fields (e.g., total length, identification, flags, etc.)
    //	// Add them as needed
    //	uint32_t source_ip; // Source IP address
    //	uint32_t dest_ip;   // Destination IP address
	//};
	//const struct ip6_hdr *ipv6 = (struct ip6_hdr*) (packet + ETH_HLEN);
	//const struct tcphdr *tcp = (struct tcphdr*) (packet + ETH_HLEN + sizeof(struct ip6_hdr));

	//if (result == RX_OK) {
	//	for (struct buf **data_start = &data_arr[0]; data_start < data; data_start++) {
	//		//host_send(&state->io, buf_data(*data_start), buf_len(*data_start));
	//		buf_free(*data_start);
	//	}
	//} else if (result < RX_OK) {
	//	ESP_LOGW("wifi", "unhandled frame (%d)", result);
	//	//log_warn("unhandled frame (%d)", result);
	//	//dump_frame(state->dump, hdr, buf);
	//	//state->awdl_state.stats.rx_unknown++;
	//}
	//buf_free(frame);
}

// typedef struct {
//   unsigned frame_control:16;
//   unsigned duration_id:16;
//   uint8_t addr1[6]; /* receiver address */
//   uint8_t addr2[6]; /* sender address */
//   uint8_t addr3[6]; /* filtering address */
//   unsigned sequence_ctrl:16;
//   uint8_t addr4[6]; /* optional */
// } wifi_ieee80211_hdr_t;
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
	const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
	const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
	const wifi_ieee80211_hdr_t *hdr = &ipkt->hdr;
	uint8_t awdl_data_mac[6] = { 0x00, 0x25, 0x00, 0xff, 0x94, 0x73 };
	if (memcmp(hdr->addr3,awdl_data_mac, sizeof(struct ether_addr))!=0) {
		return;
	}
	// subtype 
	//printf("subtype: %i\n", ipkt->payload[06]);
	
	// if address is not broadcast, print it
	if (hdr->addr1[0]!=0xff) {
		printf("ADDR1=%02x:%02x:%02x:%02x:%02x:%02x ",
			hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
			hdr->addr1[3],hdr->addr1[4],hdr->addr1[5]
		);
	}
	//printf("ADDR3=%02x:%02x:%02x:%02x:%02x:%02x ",
	//	hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
	//	hdr->addr2[2],hdr->addr2[4],hdr->addr2[5]
	//);
	
	//if (ppkt->rx_ctrl.rssi<-30){
	//	return;
	//}

	//int length = ppkt->rx_ctrl.sig_len-sizeof(wifi_ieee80211_hdr_t);
	//printf("payload len=%d; ipkt=%d\n", ppkt->rx_ctrl.sig_len, length);
	//printf("fc=0x%02X ", hdr->frame_control);


	///for (int i=0; i<length; i++) {
	///	printf("%02X:", ipkt->payload[i]);
	///}
	///printf("\n");
    ///for (int i=0; i<ppkt->rx_ctrl.sig_len; i++) {
	///	printf("%02X:", ppkt->payload[i]);
	///}
    ///for (int i=0; i<ppkt->rx_ctrl.sig_len; i++) {
	///	printf("%02X:", ppkt->payload[i]);
	///}

	//printf("payload\n");
    //for (int i=0; i<(sizeof(wifi_pkt_rx_ctrl_t)+ppkt->rx_ctrl.sig_len); i++) {
	//	if (i==sizeof(wifi_pkt_rx_ctrl_t)) {
	//		printf(" rx_ctrl\n");
	//	}
	//	printf("%02X:", ((uint8_t *)ppkt)[i]);
	//}
	//printf("\n");

	//printf("PACKET TYPE=%s, CHAN=%02d, RSSI=%02d,"
	//	//" ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
	//	//" ADDR2=%02x:%02x:%02x:%02x:%02x:%02x,"
	//	//" ADDR3=%02x:%02x:%02x:%02x:%02x:%02x\n"
	//	,
	//	wifi_sniffer_packet_type2str(type),
	//	ppkt->rx_ctrl.channel,
	//	ppkt->rx_ctrl.rssi
	//	//,
	//	///* ADDR1 */
	//	//hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
	//	//hdr->addr1[3],hdr->addr1[4],hdr->addr1[5],
	//	///* ADDR2 */
	//	//hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
	//	//hdr->addr2[3],hdr->addr2[4],hdr->addr2[5],
	//	///* ADDR3 */
	//	//hdr->addr3[0],hdr->addr3[1],hdr->addr3[2],
	//	//hdr->addr3[3],hdr->addr3[4],hdr->addr3[5]
	//);

	//const wifi_pkt_t *wifi_pkt = (wifi_pkt_t *)buff;


	awdl_receive_frame((const uint8_t *)buff,sizeof(wifi_pkt_rx_ctrl_t)+ppkt->rx_ctrl.sig_len);




	//uint8_t flags;
	//#include "owl/rx.h"
	//#include <string.h>
	//#include "owl/ieee80211.h"
//
	////int awdl_rx(const struct buf *frame, struct buf ***data_frame, struct awdl_state *state) {
	//struct awdl_state *state;
	//struct buf *data_arr[MAX_NUM_AMPDU];
	//struct buf **data = &data_arr[0];
	//struct buf ***data_frame = &data;
	//
	//const struct ieee80211_hdr *ieee80211;
	//const struct ether_addr *from, *to;
	//uint16_t fc, qosc; /* frame and QoS control */
	//signed char rssi;
	//uint64_t tsft;
	//uint8_t flags;
	//rssi = 
//
	//tsft = clock_time_us(); /* TODO Radiotap TSFT is more accurate but then need to access TSF in clock_time_us() */
	////if (radiotap_parse(frame, &rssi, &flags, NULL /* &tsft */) < 0)
	////	return RX_UNEXPECTED_FORMAT;
	////BUF_STRIP(frame, le16toh(((const struct ieee80211_radiotap_header *) buf_data(frame))->it_len));
//
	////if (check_fcs(frame, flags)) /* note that if no flags are present (flags==0), frames will pass */
	////	return RX_IGNORE_FAILED_CRC;
//
	////READ_BYTES(frame, 0, NULL, sizeof(struct ieee80211_hdr));
//
	//ieee80211 = (const struct ieee80211_hdr *) hdr;
	//from = &ieee80211->addr2;
	//to = &ieee80211->addr1;
	//fc = le16toh(hdr->frame_control);
//
	//if (!memcmp(from, &state->self_address, sizeof(struct ether_addr)))
	//	return;// RX_IGNORE_FROM_SELF; /* TODO ignore frames from self, should be filtered at pcap level */
//
	////if (!(to->ether_addr_octet[0] & 0x01) && memcmp(to, &state->self_address, sizeof(struct ether_addr)))
	////	return RX_IGNORE_NOPROMISC; /* neither broadcast/multicast nor unicast to me */
//
	//BUF_STRIP(frame, sizeof(struct ieee80211_hdr));
	//
	//printf("fc=%x, qosc=%x\n", fc & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE), qosc);
	///* Processing based on frame type and subtype */
	//switch (fc & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) {
	//	case IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION:
	//		awdl_rx_action(frame, rssi, tsft, from, to, state);
	//		return;
	//	case IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA | IEEE80211_STYPE_QOS_DATA:
	//		READ_LE16(frame, 0, &qosc);
	//		BUF_STRIP(frame, IEEE80211_QOS_CTL_LEN);
	//		/* TODO should handle block acks if required (IEEE80211_QOS_CTL_ACK_POLICY_XYZ) */
	//		if (qosc & IEEE80211_QOS_CTL_A_MSDU_PRESENT)
	//			awdl_rx_data_amsdu(frame, data_frame, from, to, state);
	//		return;
	//		/* else fall through */
	//	case IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA:
	//		awdl_rx_data(frame, data_frame, from, to, state);
	//		return;
	//	default:
	//		//ESP_LOGE("ieee80211: cannot handle type %x and subtype %x of received frame from %s",
	//		//         fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE, ether_ntoa(from));
	//		return;// RX_UNEXPECTED_TYPE;
	//}
	//wire_error:
	return; // RX_TOO_SHORT;


	//printf("channel: primary=%d, secondary=%d\n", ppkt->rx_ctrl.channel, ppkt->rx_ctrl.secondary_channel);
	//if (!memcmp(hdr->addr3, AWDL_BSSID, sizeof(struct ether_addr)))
	//	return RX_IGNORE_FROM_SELF;
	//uint16_t fc, qosc; /* frame and QoS control */
	//fc = le16toh(hdr->frame_control);
	///* Processing based on frame type and subtype */
	//switch (fc & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) {
	//	case IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION:
	//		return awdl_rx_action(frame, rssi, tsft, from, to, state);
	//	//case IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA | IEEE80211_STYPE_QOS_DATA:
	//	//	READ_LE16(frame, 0, &qosc);
	//	//	BUF_STRIP(frame, IEEE80211_QOS_CTL_LEN);
	//	//	/* TODO should handle block acks if required (IEEE80211_QOS_CTL_ACK_POLICY_XYZ) */
	//	//	if (qosc & IEEE80211_QOS_CTL_A_MSDU_PRESENT)
	//	//		return awdl_rx_data_amsdu(frame, data_frame, from, to, state);
	//	//	/* else fall through */
	//	//case IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA:
	//	//	return awdl_rx_data(frame, data_frame, from, to, state);
	//	default:
	//		printf("ieee80211: cannot handle type %x and subtype %x of received frame from %s",
	//		         fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE, ether_ntoa(from));
	//		//return RX_UNEXPECTED_TYPE;
	//}
}
