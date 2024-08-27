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
    printf("awdl_netif_linkoutput -> esp_netif_transmit -> driver->transmit (awdl_transmit)\n"); 
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
    printf("awdl_output ->linkoutput; q->len=%i\n", q->len);
    for (int i = 0; i < q->len; i++)
    {
        printf("%02x ", ((uint8_t*)q->payload)[i]);
    }
    printf("\n");
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
    //struct pbuf* p_buffer = (struct pbuf*)buffer;
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
    if (netif->input(p, netif) != ERR_OK) {
        ESP_LOGE(TAG,"wlanif_input: IP input error\n");
        pbuf_free(p);
        return;
    }
    return;
    /*


    //ip6_input(p, netif);
    //ip4_input(buffer, netif);
    return;

    struct os_mbuf* sdu_rx = (struct os_mbuf*)eb;

    size_t rx_len = (size_t)OS_MBUF_PKTLEN(sdu_rx);

    struct pbuf* p = pbuf_alloc(PBUF_RAW, rx_len, PBUF_POOL);
    if (p == NULL)
    {
        ESP_LOGE(TAG, "(%s) failed to allocate memory for pbuf", __func__);
        return;
    }

    // TODO: is there a better way here... ideally we wouldn't have to _copy_ this data just to turn
    // it into a format LwIP understands (i.e., to go from mbuf to pbuf). Better would be if the
    // pbuf just referred to data in the mbuf. Not sure if that's possible though.
    int rc = os_mbuf_copydata(sdu_rx, 0, rx_len, p->payload);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to copy mbuf into pbuf", __func__);
        pbuf_free(p);
        return;
    }

    // TODO: determine if we need to do this...
    // It's _possible_ that the os_mbuf used for rx is managed by NimBLE for us, but I'm not
    // actually sure. Some discussion here but unclear what the expected behaviour is.
    //   https://github.com/espressif/esp-idf/issues/9044
    //
    // esp_netif_t* esp_netif = netif->state;
    // esp_netif_free_rx_buffer(esp_netif, eb);

    p->len = rx_len;
    for (int i = 0; i < p->len; i++)
    {
        printf("%02x ", ((uint8_t*)p->payload)[i]);
    }
    rfc7668_input(p, netif);
    */
}

void nimble_addr_to_eui64(ble_addr_t const* addr, uint8_t* eui64)
{
    // NimBLE stores addresses in _reverse_ order. We need to reverse these
    // before doing the EUI64 conversion, otherwise we get incorrect
    // addresses in our IPv6 headers.
    uint8_t addr_reversed[6];
    for (int i = 0; i < 6; ++i)
    {
        addr_reversed[i] = addr->val[6 - 1 - i];
    }

    bool is_public_addr = addr->type == BLE_ADDR_PUBLIC || addr->type == BLE_ADDR_PUBLIC_ID;
    ble_addr_to_eui64(eui64, addr_reversed, is_public_addr);
}

void ipv6_create_link_local_from_eui64(uint8_t const* eui64_addr, ip6_addr_t* dst)
{
    IP6_ADDR_PART(dst, 0, 0xFE, 0x80, 0x00, 0x00);
    IP6_ADDR_PART(dst, 1, 0x00, 0x00, 0x00, 0x00);
    IP6_ADDR_PART(dst, 2, eui64_addr[0] ^ 0x02, eui64_addr[1], eui64_addr[2], eui64_addr[3]);
    IP6_ADDR_PART(dst, 3, eui64_addr[4], eui64_addr[5], eui64_addr[6], eui64_addr[7]);
}

static inline void configure_netif_addresses(struct netif* netif, ble_addr_t* addr, bool is_peer)
{
    ESP_LOGD(
        TAG,
        "(%s) setting %s address ",
        __func__,
        is_peer ? "peer" : "local"
    );
        //debug_print_ble_addr(addr)

    uint8_t eui64_addr[8];// TODO: IPv6 is mac
    nimble_addr_to_eui64(addr, eui64_addr);

    if (is_peer)
    {
        rfc7668_set_peer_addr_eui64(netif, eui64_addr, sizeof(eui64_addr));
    }
    else
    {
        rfc7668_set_local_addr_eui64(netif, eui64_addr, sizeof(eui64_addr));

        ip6_addr_t lladdr;
        ipv6_create_link_local_from_eui64(eui64_addr, &lladdr);

        ESP_LOGD(
            TAG,
            "(%s) adding link-local address %s to netif %p",
            __func__,
            ip6addr_ntoa(&lladdr),
            netif
        );

        ip_addr_copy_from_ip6(netif->ip6_addr[0], lladdr);
        ip6_addr_assign_zone(ip_2_ip6(&(netif->ip6_addr[0])), IP6_UNICAST, netif);
        netif_ip6_addr_set_state(netif, 0, IP6_ADDR_PREFERRED);
    }
}
#include "esp_netif_net_stack.h"
void awdl_netif_up(esp_netif_t* esp_netif, ble_addr_t* peer_addr, ble_addr_t* our_addr)
{
    struct netif* netif = esp_netif_get_netif_impl(esp_netif);
    if (netif == NULL || peer_addr == NULL || our_addr == NULL)
    {
        ESP_LOGE(TAG, "(%s) invalid parameters", __func__);
        return;
    }

    configure_netif_addresses(netif, peer_addr, true);
    configure_netif_addresses(netif, our_addr, false);

    netif_set_up(netif);
    netif_set_link_up(netif);

    ESP_LOGD(TAG, "(%s) netif up; esp_netif=%p netif=%p", __func__, esp_netif, netif);
}

void awdl_netif_down(esp_netif_t* esp_netif)
{
    struct netif* netif = esp_netif_get_netif_impl(esp_netif);
    if (netif == NULL)
    {
        ESP_LOGE(TAG, "(%s) invalid parameters", __func__);
        return;
    }

    netif_set_down(netif);
    netif_set_link_down(netif);
}