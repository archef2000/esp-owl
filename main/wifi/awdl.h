#include "esp_idf_version.h"
#include "esp_compiler.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "owl/state.h"
#include "wifi/core.h"

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

typedef struct awdl_driver* awdl_driver_handle;
typedef void (*awdl_event_handler)(
    awdl_driver_handle handle,
    void* userdata
);

extern esp_netif_netstack_config_t* netstack_default_awdl;

awdl_driver_handle awdl_create( struct daemon_state *state);
