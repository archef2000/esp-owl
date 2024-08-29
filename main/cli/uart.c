#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cli/command.h"
#include "freertos/queue.h"
#include "cli/tasks.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

static QueueHandle_t uart_queue;

void uart_event_task(void *pvParameters) {
    struct availabeTasks *tasks = (struct availabeTasks *)pvParameters;
    char *command = malloc(BUF_SIZE);
    uint8_t command_len = 0;
    uart_event_t event;
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    while (1) {
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
            bzero(data, BUF_SIZE);
            switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(UART_NUM, data, event.size, portMAX_DELAY);
                    memcpy(command+command_len, data, event.size);
                    command_len += event.size;
                    printf("%.*s\n", command_len, command);
                    for (int i = 0; i < command_len; i++) {
                        if (command[i] == '\n'|| command[i] == '\r') {
                            if (command_len == 1 && command[0] == 13) {
                                command_len = 0;
                                bzero(command, BUF_SIZE);
                                break;
                            }
                            printf("len %d command: %.*s\n", command_len, command_len, command);
                            main_cmd(command, tasks);
                            command_len = 0;
                            bzero(command, BUF_SIZE);
                        }
                    }
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGE("uart","hw fifo overflow\n");
                    uart_flush_input(UART_NUM);
                    xQueueReset(uart_queue);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGE("uart","ring buffer full\n");
                    uart_flush_input(UART_NUM);
                    xQueueReset(uart_queue);
                    break;
                default:
                    ESP_LOGE("uart", "event type %d", event.type);
                    break;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void init_uart(struct availabeTasks *tasks) {
    uart_config_t uart_config = {
        .baud_rate = 122500,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(uart_event_task, "uart_event_task", 2048, tasks, 12, NULL);
}