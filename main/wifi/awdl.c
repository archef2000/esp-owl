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

static const char* TAG = "awdl";

#define BIT_TX_UNSTALLED (1 << 0)

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
}

static esp_err_t awdl_transmit(void* h, void* buffer, size_t len)
{
    // send data over the network interface
    struct awdl_driver* driver = (struct awdl_driver*)h;
    struct daemon_state *state = (struct daemon_state *)driver->userdata;
	if (state->next || circular_buf_full(state->tx_queue_multicast)) {
		printf("send_data: queue full\n");
		return ESP_ERR_NO_MEM; // queue full: ESP_ERR_TIMEOUT
	}
    struct in6_addr *dst_address = mem_malloc(sizeof(struct ip6_addr));
    memcpy(dst_address, buffer + 24, 16);
    struct ether_addr dst_mac = in6_addr_to_ether_addr(dst_address);
    // multicast address 33:33:80:00:00:fb
	bool is_multicast;
    if (ip6_addr_ismulticast((ip6_addr_t *)dst_address)) {
        is_multicast = true;
        dst_mac.ether_addr_octet[0] = 0x33;
        dst_mac.ether_addr_octet[1] = 0x33;
        dst_mac.ether_addr_octet[2] = 0x80;
        dst_mac.ether_addr_octet[3] = 0x00;
        dst_mac.ether_addr_octet[4] = 0x00;
        //dst_mac.ether_addr_octet[5] = 0xfb;
    }
	struct buf *buf = NULL;
	buf = buf_new_owned(ETHER_LENGTH+len);
	write_ether_addr(buf, ETHER_DST_OFFSET, &dst_mac);
	write_bytes(buf, ETHER_LENGTH, buffer, len);
    printf("%d\n",buf_len(buf));
    if (is_multicast) {
        printf("awdl_transmit: is_multicast len=%i\n", len);
        for (int i = 0; i < len; i++)
            printf("%02x ", ((uint8_t *)buffer)[i]);
        printf("\n");
		circular_buf_put(state->tx_queue_multicast, buf);
        esp_timer_start_once(state->timer_state.tx_mcast_timer.handle, 0*1000*1000);
	} else { // unicast 
		state->next = buf;
        esp_timer_start_once(state->timer_state.tx_timer.handle, 0);
	}
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
