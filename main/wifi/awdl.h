#include "esp_idf_version.h"
#include "esp_compiler.h"
#include "esp_netif.h"
#include "esp_err.h"


/** Maximum concurrent IPSP channels */
#define LOWPAN6_BLE_IPSP_MAX_CHANNELS 1

/** Maximum Transmit Unit on an IPSP channel.
 *
 * This is required by the specification to be 1280 (it's the minimum MTU for
 * IPv6).
 */
#define LOWPAN6_BLE_IPSP_MTU 1280

/** Maximum data size that can be received.
 *
 * This value can be modified to be lower than the MTU set on the channel.
 */
#define LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE 1280

/** Maximum number of receive buffers.
 *
 * Each receive buffer is of size LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE. Tweak this
 * value to modify the number of Service Data Units (SDUs) that can be received
 * while an SDU is being consumed by the application.
 */
#define LOWPAN6_BLE_IPSP_RX_BUFFER_COUNT 4

/** The IPSP L2CAP Protocol Service Multiplexer number.
 *
 * Defined by the Bluetooth Low Energy specification. See:
 *    https://www.bluetooth.com/specifications/assigned-numbers/
 */
#define LOWPAN6_BLE_IPSP_PSM 0x0023




// clang-format off
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define ESP_NETIF_INHERENT_DEFAULT_AWDL() {                      \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)     \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
     .get_ip_event  = 0,                                        \
     .lost_ip_event = 0,                                        \
     .if_key        = "AWDL_DEF",                               \
     .if_desc       = "awdl",                                   \
     .route_prio    = 16,                                       \
     .bridge_info   = NULL                                      \
}
#else
#define ESP_NETIF_INHERENT_DEFAULT_AWDL() {              \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)     \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
     .get_ip_event  = 0,                                        \
     .lost_ip_event = 0,                                        \
     .if_key        = "LOWPAN6_BLE_DEF",                        \
     .if_desc       = "lowpan6_ble",                            \
     .route_prio    = 16,                                       \
}
#endif
// clang-format on


/** A LoWPAN6 BLE event handler
 *
 * @param[in] handle Handle to the lowpan6_ble driver processing this event.
 * @param[in] event Pointer to a lowpan6_ble_event struct.
 * @param[in] userdata (Optional) Arbitrary data provided by the user during callback registration.
 */

enum lowpan6_ble_event_type
{
    LOWPAN6_BLE_EVENT_GAP_CONNECTED,
    LOWPAN6_BLE_EVENT_GAP_DISCONNECTED
};


//! Event struct for LoWPAN6 BLE events.
struct awdl_event
{
    //! Discriminator for the event data included in this event.
    enum lowpan6_ble_event_type type;

    union
    {
        //! Data available for type LOWPAN6_BLE_EVENT_GAP_CONNECTED.
        struct
        {
            //! The underlying GAP event.
            struct ble_gap_event* event;
        } gap_connected;

        //! Data available for type LOWPAN6_BLE_EVENT_GAP_DISCONNECTED.
        struct
        {
            //! The underlying GAP event.
            struct ble_gap_event* event;
        } gap_disconnected;
    };
};


typedef struct awdl_driver* awdl_driver_handle;
typedef void (*awdl_event_handler)(
    awdl_driver_handle handle,
    struct awdl_event* event,
    void* userdata
);

esp_err_t awdl_create_server(awdl_driver_handle handle, awdl_event_handler cb, void* userdata);

extern esp_netif_netstack_config_t* netstack_default_awdl;

awdl_driver_handle awdl_create(void);

esp_err_t awdl_init();