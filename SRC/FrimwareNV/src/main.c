#include <stdio.h>
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "gpio.h"
#include "mpu6050.h"
#include "config.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// Función requerida para la calibración RF
uint32_t user_rf_cal_sector_set(void) {
    return 127;
}

// Variables globales
static xQueueHandle sensor_queue = NULL;
static int sock = -1;
static struct sockaddr_in server_addr;

// LED para indicación de estado
#define LED_PIN 2
#define LED_ON() GPIO_OUTPUT_SET(LED_PIN, 0)
#define LED_OFF() GPIO_OUTPUT_SET(LED_PIN, 1)

// Configuración UDP
#define UDP_PORT 1234
#define SERVER_IP "192.168.1.100"  // Cambia esto a la IP de tu servidor

// Inicialización de WiFi
static void wifi_init(void) {
    wifi_set_opmode(STATION_MODE);
    
    struct station_config sta_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };
    
    wifi_station_set_config(&sta_config);
    wifi_station_connect();
}

// Inicialización de UDP
static bool udp_init(void) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Error creando socket\n");
        return false;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    return true;
}

// Tarea de lectura del sensor
void sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    
    // Inicializar MPU6050
    if (!mpu6050_init()) {
        printf("Error inicializando MPU6050\n");
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        if (mpu6050_read_data(&sensor_data)) {
            // Parpadear LED en cada lectura
            LED_ON();
            
            // Enviar datos a la cola
            if (xQueueSend(sensor_queue, &sensor_data, 0) != pdTRUE) {
                printf("Cola llena!\n");
            }
            
            LED_OFF();
        }
        
        // Esperar para la siguiente lectura
        vTaskDelay(1000 / SAMPLE_RATE_HZ / portTICK_RATE_MS);
    }
}

// Tarea de envío UDP
void udp_send_task(void *pvParameters) {
    sensor_data_t sensor_data;
    char buffer[128];
    
    while (1) {
        if (xQueueReceive(sensor_queue, &sensor_data, portMAX_DELAY) == pdTRUE) {
            // Formatear datos en buffer
            snprintf(buffer, sizeof(buffer), 
                    "{\"node\":\"%s\",\"ts\":%u,\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f}",
                    NODE_ID, 
                    sensor_data.timestamp,
                    sensor_data.accel.x,
                    sensor_data.accel.y,
                    sensor_data.accel.z);
            
            // Enviar por UDP
            sendto(sock, buffer, strlen(buffer), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr));
        }
    }
}

// Función principal
void user_init(void) {
    // Configurar LED
    GPIO_AS_OUTPUT(LED_PIN);
    LED_OFF();
    
    // Crear cola para datos del sensor
    sensor_queue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(sensor_data_t));
    
    // Inicializar WiFi y UDP
    wifi_init();
    if (!udp_init()) {
        printf("Error inicializando UDP\n");
        return;
    }
    
    // Crear tareas
    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 2, NULL);
    xTaskCreate(udp_send_task, "udp_task", 2048, NULL, 1, NULL);
}