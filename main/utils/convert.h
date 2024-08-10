
#include <string.h>
#ifndef CMD_cb_H
#define CMD_cb_H
typedef struct {
    void (*ble_write)(char* str);
    void (*ble_send)(const char* str);
} CMD_cb;
#endif //CMD_cb_H

char* bt_hex(const void *buf, size_t len);
char* hex_to_string(const char *hex);

typedef void (*usb_callback_t)(char* str);