/* Copyright 2024 Tenera Care
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "awdl.h"
#include "awdl_netif.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "wifi/core.h"
#include "owl/ethernet.h"
//#include "host/ble_hs.h"
//#include "lowpan6_ble_netif.h"
//#include "nimble/ble.h"
//#include "os/os_mempool.h"

/** The chunk of defines below are used to initialize mbufs later in this module.
 *
 * The MyNewt documentation explains what these are:
 *  https://mynewt.apache.org/latest/os/core_os/mbuf/mbuf.html
 */

static const char* TAG = "awdl";

/*
#define MBUF_PKTHDR_OVERHEAD   sizeof(struct os_mbuf_pkthdr)
#define MBUF_MEMBLOCK_OVERHEAD sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD
#define MBUF_NUM_MBUFS         (LOWPAN6_BLE_IPSP_MAX_CHANNELS * LOWPAN6_BLE_IPSP_RX_BUFFER_COUNT)
#define MBUF_PAYLOAD_SIZE      LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE
#define MBUF_BUF_SIZE          OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE     (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE      OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)


//! Pool of mbufs, shared by all the channels in this module.
static struct os_mbuf_pool s_mbuf_pool;

//! Memory pool used to initialize our mbuf pool, above.
static struct os_mempool s_mempool;

//! Memory allocated for our memory pool, above.
static os_membuf_t s_membuf[MBUF_MEMPOOL_SIZE];

*/
#define BIT_TX_UNSTALLED (1 << 0)
//static StaticEventGroup_t s_lowpan6_event_group_buffer;
//static EventGroupHandle_t s_lowpan6_event_group;

/** LoWPAN6 BLE driver
 *
 * This struct provides glue logic between esp_netif and the BLE channel used as a transport.
 */
struct awdl_driver
{
    // esp_netif driver base
    esp_netif_driver_base_t base;

    // Connection handle for our GAP connection. BLE_HS_CONN_HANDLE_NONE if not connected.
    uint16_t conn_handle;

    // Pointer to L2CAP channel used for LoWPAN6-BLE
    struct ble_l2cap_chan* chan;

    // (Optional) event handler provided by the user.
    awdl_event_handler cb;

    // (Optional) event handler data provided by the user.
    void* userdata;
};

static void awdl_free_rx_buffer(void* h, void* buffer)
{
    ESP_LOGE(TAG, "awdl_free_rx_buffer");
    //int rc = os_mbuf_free_chain(buffer);
    //if (rc != 0)
    //{
    //    ESP_LOGW(TAG, "(%s) failed to free os_mbuf; om=%p rc=%d", __func__, buffer, rc);
    //}
}

static esp_err_t awdl_transmit(void* h, void* buffer, size_t len)
{
    printf("awdl_transmit\n");
    struct awdl_driver* driver = (struct awdl_driver*)h;
    struct daemon_state *state = (struct daemon_state *)driver->userdata;
	if (state->next || circular_buf_full(state->tx_queue_multicast)) {
		printf("send_data: queue full\n");
		return ESP_ERR_NO_MEM; // queue full: ESP_ERR_TIMEOUT
	}
    for (int i = 0; i < len; i++) {
        printf("%02x ", ((uint8_t*)buffer)[i]);
    }
    printf("\n");
    struct in6_addr *dst_address = mem_malloc(sizeof(struct ip6_addr));
    struct ether_addr dst_mac = in6_addr_to_ether_addr(dst_address);
    memcpy(dst_address, buffer + 24, 16);
    // multicast address 33:33:80:00:00:fb
	bool is_multicast;
    if (ip6_addr_ismulticast((ip6_addr_t *)dst_address)) {
        // TODO: check if the packet is a multicast packet
        printf("multicast packet\n");
        is_multicast = true;
        printf("dst_mac: %s\n", ether_ntoa(&dst_mac));
        dst_mac.ether_addr_octet[0] = 0x33;
        dst_mac.ether_addr_octet[1] = 0x33;
        dst_mac.ether_addr_octet[2] = 0x80;
        dst_mac.ether_addr_octet[3] = 0x00;
        dst_mac.ether_addr_octet[4] = 0x00;
        dst_mac.ether_addr_octet[5] = 0xfb;
    }
    // print ipv6 dst address
	char *ipv6_addr_str = malloc(sizeof(char) * INET6_ADDRSTRLEN);
	in6_addr_to_string(ipv6_addr_str, *dst_address);
	printf("pv6_addr: %s\n", ipv6_addr_str);

    printf("dst_mac: %s\n", ether_ntoa(&dst_mac));

	struct buf *buf = NULL;
	buf = buf_new_owned(ETHER_MAX_LEN);
	write_ether_addr(buf, ETHER_DST_OFFSET, &dst_mac);
	struct ether_addr *src = &state->awdl_state.self_address;
	write_ether_addr(buf, ETHER_SRC_OFFSET, src);
	write_bytes(buf, ETHER_LENGTH, buffer, len);

	if (is_multicast) {
		circular_buf_put(state->tx_queue_multicast, buf);
		awdl_send_multicast(&state->timer_state.tx_mcast_timer);
	} else { // unicast 
		state->next = buf;
		awdl_send_unicast(&state->timer_state.tx_timer);
	}
    // send data over the network interface
    return ESP_OK;
}

static esp_err_t awdl_post_attach(esp_netif_t* esp_netif, void* args)
{
    printf("awdl_post_attach\n");
    struct awdl_driver* driver = (struct awdl_driver*)args;

    ESP_LOGD(TAG, "(%s) esp_netif=%p args=%p", __func__, esp_netif, args);

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = awdl_free_rx_buffer,
        .transmit              = awdl_transmit,
        .handle                = driver
    };

    driver->base.netif = esp_netif;
    esp_err_t err      = esp_netif_set_driver_config(esp_netif, &driver_ifconfig);

    esp_netif_action_start(driver->base.netif, 0, 0, NULL);

    return err;
}
/*
esp_err_t awdl_init()
{
    printf("awdl_init\n");
    //int rc =
    //    os_mempool_init(&s_mempool, MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE, s_membuf, "awdl");
    //if (rc != 0)
    //{
    //    ESP_LOGE(TAG, "(%s) failed to initialize mempool; rc=%d", __func__, rc);
    //    return ESP_FAIL;
    //}

    //rc = os_mbuf_pool_init(&s_mbuf_pool, &s_mempool, MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    //if (rc != 0)
    //{
    //    ESP_LOGE(TAG, "(%s) failed to initialize mbuf pool; rc=%d", __func__, rc);
    //    os_mempool_clear(&s_mempool);
    //    return ESP_FAIL;
    //}

    s_lowpan6_event_group = xEventGroupCreateStatic(&s_lowpan6_event_group_buffer);

    // we should _never_ hit this since we're initializing from a static event group. Still, for
    // completeness' sake...
    if (s_lowpan6_event_group == NULL)
    {
        ESP_LOGE(TAG, "(%s) failed to initialize event group", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;
}
*/

awdl_driver_handle awdl_create( struct daemon_state *state)
{
    ESP_LOGI(TAG, "(%s) creating awdl driver", __func__);

    struct awdl_driver* driver = calloc(1, sizeof(struct awdl_driver));
    if (driver == NULL)
    {
        ESP_LOGE(TAG, "(%s) failed to allocate memory for awdl driver", __func__);
        return NULL;
    }

    driver->base.post_attach = awdl_post_attach;

    //driver->conn_handle = BLE_HS_CONN_HANDLE_NONE;
    driver->chan        = NULL;
    driver->userdata    = (void *)state;

    return driver;
}

esp_err_t awdl_destroy(awdl_driver_handle driver)
{
    return ESP_OK;
}

esp_err_t awdl_connect(
    awdl_driver_handle handle,
    struct ether_addr* addr,
    int32_t timeout_ms,
    awdl_event_handler cb,
    void* userdata
)
{
    struct awdl_driver* driver = (struct awdl_driver*)handle;
    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cb)
    {
        driver->cb       = cb;
        driver->userdata = userdata;
    }
    else
    {
        driver->cb       = NULL;
        driver->userdata = NULL;
    }
    return ESP_OK;
}
