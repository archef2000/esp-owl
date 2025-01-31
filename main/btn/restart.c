#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUTTON_GPIO 9

void IRAM_ATTR button_isr_handler(void* arg) {
    esp_restart();
}

void init_btn_restart(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    if (gpio_get_level(BUTTON_GPIO) == 0) {
        while (gpio_get_level(BUTTON_GPIO) == 0) {
            vTaskDelay(2);
        }
    }
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
    printf("Interrupt configured. Press the button to restart the chip.\n");
}
