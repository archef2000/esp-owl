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

#include "awdl_netif.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/err.h"
#include "netif/lowpan6_ble.h"
#include "lwip/netif.h"
#include "lwip/ip6_addr.h"
#include "wifi/core.h"

static err_t awdl_netif_init(struct netif* netif);
static void awdl_netif_input(void* h, void* buffer, size_t len, void* eb);

// The esp_netif_netstack_config_t struct was not publically exposed until after v5
// If we're running a lower version, we'll define the struct ourselves like in the following
// example:
// https://github.com/david-cermak/eth-ap-nat/blob/2279344e18a0b98b5368999aac9441c59871e6fa/eth-ap-idf4.3/main/ethernet_example_main.c#L90-L96
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

// clang-format off
    #include "lwip/esp_netif_net_stack.h"
    const struct esp_netif_netstack_config s_netif_config_awdl = {
        .lwip = {
            .init_fn  = awdl_netif_init,
            .input_fn = awdl_netif_input,
        }
    };
// clang-format on

#else

// clang-format off
    struct esp_netif_lwip_vanilla_config
    {
        err_t (*init_fn)(struct netif*);
        void (*input_fn)(void* netif, void* buffer, size_t len, void* eb);
    };

    const struct esp_netif_lwip_vanilla_config s_netif_config_awdl = {
        .init_fn  = awdl_netif_init,
        .input_fn = awdl_netif_input,
    };
// clang-format on

#endif

const esp_netif_netstack_config_t* netstack_default_awdl =
    (const esp_netif_netstack_config_t*)&s_netif_config_awdl;

static const char* TAG = "awdl_netif";

err_t awdl_netif_linkoutput(struct netif* netif, struct pbuf* p)
{
    //printf("awdl_netif_linkoutput -> esp_netif_transmit -> driver->transmit (awdl_transmit)\n");
    esp_err_t err = esp_netif_transmit(netif->state, p->payload, p->len);
    if (err != ESP_OK)
    {
        return ERR_IF;
    }

    return ERR_OK;
}

/**
 * @ingroup rfc7668if
 * Compress outgoing IPv6 packet and pass it on to netif->linkoutput
 *
 * @param netif The lwIP network interface which the IP packet will be sent on.
 * @param q The pbuf(s) containing the IP packet to be sent.
 * @param ip6addr The IP address of the packet destination.
 *
 * @return See rfc7668_compress
 */
err_t
awdl_output(struct netif *netif, struct pbuf *q, const ip6_addr_t *ip6addr)
{
    // awdl_output ->linkoutput
    err_t err;
    err = netif->linkoutput(netif, q);
    return err;
}
/**
 * @ingroup rfc7668if
 * Initialize the netif
 * 
 * No flags are used (broadcast not possible, not ethernet, ...)
 * The shortname for this netif is "BT"
 *
 * @param netif the network interface to be initialized as RFC7668 netif
 * 
 * @return ERR_OK if everything went fine
 */
err_t
awdl_if_init(struct netif *netif)
{
  netif->name[0] = 'o';
  netif->name[1] = 'w';
  /* local function as IPv6 output */
  netif->output_ip6 = awdl_output;

  /* maximum transfer unit, set according to RFC7668 ch2.4 */
  netif->mtu = IP6_MIN_MTU_LENGTH;

  /* no flags set (no broadcast, ethernet,...)*/
  netif->flags = 0;

  /* everything fine */
  return ERR_OK;
}

static err_t awdl_netif_init(struct netif* netif)
{
    printf("awdl_netif_init\n");
    awdl_if_init(netif);
    netif_set_flags(netif, NETIF_FLAG_BROADCAST);
    netif_set_flags(netif, NETIF_FLAG_MLD6);
    netif->linkoutput = awdl_netif_linkoutput;

    ESP_LOGD(TAG, "(%s) init netif=%p", __func__, netif);

    return ERR_OK;
}

#include "lwip/esp_pbuf_ref.h"
#include "arpa/inet.h" // ntohs, etc.
#include "lwip/prot/ethernet.h" // Ethernet header
static void awdl_netif_input(void* h, void* buffer, size_t len, void* eb)
{
    // from wifi/etc to software netif
    printf("awdl_netif_input\n");

    struct netif* netif    = (struct netif*)h; 
    esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
    struct pbuf *p;

    ESP_LOGI(TAG, "awdl_netif_input len=%i", len);
    p = esp_pbuf_allocate(esp_netif, buffer, len, NULL);
    for (int i = 0; i < p->len; i++)
    {
        printf("%02x ", ((uint8_t*)p->payload)[i]);
    }
    printf("\n\n");
    if (p == NULL) {
        ESP_LOGE(TAG, "(%s) failed to allocate memory for pbuf", __func__);
        esp_netif_free_rx_buffer(esp_netif, buffer);
        return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_ERR_NO_MEM);
    }
    /* full packet send to tcpip_thread to process */
    // netif->input
    if (netif->input(p, netif) != ERR_OK) {
        ESP_LOGE(TAG,"wlanif_input: IP input error\n");
        pbuf_free(p);
        return;
    }
    return;
}
