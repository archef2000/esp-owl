#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cli/command.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

void uart_receive_task() {
    char command[BUF_SIZE];
    size_t command_len = 0;  // Initial command length

    uint8_t length = 0;
    int index = 0;
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 10/portTICK_PERIOD_MS);
        if (len > 0) {
            printf("Received: %s len: %d\n", data, len);
            memcpy(command + command_len, data, len);
            command_len += len;
            printf("command_len: %d", command_len);
            command[command_len] = '\0';
            printf("command: %s", command);
        }
    }
}

void init_uart() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    xTaskCreate(uart_receive_task, "uart_receive_task", 1024*3, NULL, 5, NULL);
}
