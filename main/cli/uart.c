

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cli/command.h"
#include "freertos/queue.h"
#include "cli/tasks.h"
#include "esp_intr_alloc.h"
#include "driver/uart.h"
#include "cli/backtrace.h"
#include "utils/ringbuffer.h"

#define CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG 1
#define CONFIG_ESP_CONSOLE_USB_SERIAL_UART 1
#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG

#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include <fcntl.h>

#endif

static QueueHandle_t uart_queue;

int trim_command(char *command, int command_len) {
    if (command == NULL || command_len <= 0) {
        return 0;
    }
    while (command[command_len - 1] == ' ') {
        command_len--;
    }
    for (int i = command_len - 1; i >= 0; --i) {
        if (command[i] == ' ') {
            while (i > 0 && command[i - 1] == ' ') {
                i--;
            }
            command[i++] = ' ';
            return i;
        }
    }
    return 0;
}

void uart_event_task(void *pvParameters) {
    struct systemInfo *sysinfo = (struct systemInfo *)pvParameters;
    RingBuffer command_history;
    initRingBuffer(&command_history);
    char *command = malloc(BUF_SIZE);
    uint8_t command_len = 0;
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
        int len = 0;
        #if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
            len = usb_serial_jtag_read_bytes( data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        #endif
        #if CONFIG_ESP_CONSOLE_USB_SERIAL_UART
            if (len == 0)
                len = uart_read_bytes(UART_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        #endif
        if (len > 0) {
            switch (*data) {
                case 3: // Ctrl+C
                    printf("Ctrl+C clearing console\n");
                    command_len = 0;
                    bzero(command, BUF_SIZE);
                    break;
                case 18: // Ctrl+R
                    printf("Ctrl+R rebooting\n");
                    esp_restart();
                    break;
                case 127: // Backspace
                    if (command_len > 0) {
                        command_len--;
                        bzero(command+command_len, 1);
                        printf("Backspace removing last character\n");
                        printf("%.*s\n", command_len, command);
                    }
                    break;
                case 23: // Strg+Backspace
                    printf("Strg+Backspace deleting word\n");
                    command_len = trim_command(command, command_len);
                        printf("%.*s\n", command_len, command);
                    break;
                case 27: // Up arrow
                    printf("arrows\n");
                    if (data[1] == 91) {
                        switch (data[2]) {
                            case 65: // Up arrow
                                printf("Up arrow\n");
                                printBuffer(&command_history);
                                char *new_command = getRingBufferIndex(&command_history, 0);
                                memcpy(command,new_command,strlen(new_command) );
                                //command = getRingBufferIndex(&command_history, 0);
                                command_len = strlen(command);
                                printf("len: %d command: %s\n", command_len, command);
                                printf("%.*s\n", command_len, command);
                                break;
                            case 66: // Down arrow
                                printf("Down arrow\n");
                                break;
                            case 67: // Right arrow
                                printf("Right arrow\n");
                                break;
                            case 68: // Left arrow
                                printf("Left arrow\n");
                                break;
                        }
                    }
                    break;

                default:
                    memcpy(command+command_len, data, len);
                    command_len += len;
                    for (int i = 0; i < len; i++) {
                        if (data[i] == 13) {
                            printf("len %d command: %.*s\n", command_len, command_len, command);
                            printBuffer(&command_history);
                            char *buffer = malloc(command_len);
                            memcpy(buffer, command, command_len);
                            buffer[command_len] = '\0';
                            putRingBuffer(&command_history, buffer);
                            printBuffer(&command_history);
                            main_cmd(command, sysinfo);
                            command_len = 0;
                            bzero(command, BUF_SIZE);
                            break;
                        }
                    }
                    printf("%.*s\n", command_len, command);
                    break;
            }
        }
        len = 0;
        vTaskDelay(0);
    }
    free(data);
    vTaskDelete(NULL);
}

bool check_semaphore_status() {
    if(uart_wait_tx_done(UART_NUM,0)==ESP_OK)
    {
        return false;
    } else {
        return true;
    }
}

void uart_write_watcher() {
    int times = 0;
    while (1) {
        if (check_semaphore_status()) {
            printf("Taken\n");
            uart_wait_tx_done(UART_NUM,portMAX_DELAY);
            vTaskDelay(1);
        } else {
            if (++times%1000 == 0) {
                vTaskDelay(1);
            }
        }
    }
}

// jtag vid: 12346 pid: 4097 -> hex vid: 0x303A pid: 0x1001
// uart vid: 6790 pid: 21971 -> hex vid: 0x1AA2 pid: 0x5623

void init_uart(struct systemInfo *sysinfo) {

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_driver_config_t usb_serial_jtag_config;
    usb_serial_jtag_config.rx_buffer_size = BUF_SIZE;
    usb_serial_jtag_config.tx_buffer_size = BUF_SIZE;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
#endif
#if CONFIG_ESP_CONSOLE_USB_SERIAL_UART
    uart_config_t uart_config = {
        .baud_rate = CONFIG_MONITOR_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#endif
    xTaskCreate(uart_event_task, "uart_event_task", 2048*4, sysinfo, 11, NULL);
}