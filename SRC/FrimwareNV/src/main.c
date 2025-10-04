#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Configuración del LED (GPIO2 en ESP07)
#define LED_PIN 2

// Tarea para el parpadeo
void blink_task(void *pvParameters) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    while (1) {
        gpio_set_level(LED_PIN, 0);  // Encender (activo en bajo en muchos ESP)
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 1);  // Apagar
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

// Función principal (equivalente a app_main() en ESP-IDF)
void app_main() {
    xTaskCreate(blink_task, "blink_task", 1024, NULL, 1, NULL);
}