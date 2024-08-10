#include "esp_netif_types.h"
#include "lwip/ip6_addr.h"
#include "nimble/ble.h"

/** Bring up the netif upon connection.
 *
 * @param[in] esp_netif The netif to mark up.
 * @param[in] peer_addr The BLE address of the peer we've connected to.
 * @param[in] our_addr Our BLE address for this connection.
 */
void awdl_netif_up(esp_netif_t* esp_netif, ble_addr_t* peer_addr, ble_addr_t* our_addr);

/** Bring down the netif upon disconnection.
 *
 * @param[in] esp_netif The netif to mark down.
 */
void awdl_netif_down(esp_netif_t* netif);
