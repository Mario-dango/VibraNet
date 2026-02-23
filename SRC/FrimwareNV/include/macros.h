#ifndef MACROS_H
#define MACROS_H

#include <stdint.h>

// ================= DEFINICIÓN DE ESTRUCTURAS =================

struct DeviceConfig {
    int mode; 
    int sleep_time_s; 
    int mpu_threshold; 
    int sampling_freq; 
    uint32_t burst_size;
};

// ============================================================================
// Algunas constantes útiles
// ============================================================================

// VERSIONES
#define HW_VER "1.5"
#define SW_VER "6.1" // Actualizado para reflejar el parche de energía

// Configuración del sensor
// MPU1 siempre en 0x68. MPU2 (opcional) debe tener el pin AD0 a 3.3V para ser 0x69
#define MPU1_ADDR 0x68 
#define MPU2_ADDR 0x69

// Configuración WiFi
// #define WIFI_SSID "PAPETTI"
// #define WIFI_PASS "Chulito$26"

// Configuración WiFi
#define WIFI_SSID "bawy"
#define WIFI_PASS "Bawbaaw42"

// Configuración WiFi
#define WIFI_SSID "momantai"
#define WIFI_PASS "42425640"

// Configuración MQTT
// #define MQTT_SERVER "192.168.100.68" // ¡PON TU IP DE DOCKER/MOSQUITTO!

#define MQTT_SERVER "10.81.207.250" // ¡PON TU IP DE DOCKER/MOSQUITTO!

#define MQTT_PORT 1883
#define MQTT_TOPIC "vibranet/data"


#define LED_PIN 2
#define BATTERY_PIN A0
// #define NODE_ID "dango_node_001"   // Vibranet on protoboard
#define NODE_ID "dango_node_002"   // Vibranet on PCB
// #define NODE_ID "dango_node_003"   // Vibranet on protoboard

/*
docker run cloudflare/cloudflared:latest tunnel --no-autoupdate run --token eyJhIjoiMTI0ODYyYzg3MDM1NzI0Y2ZhNmI3NjYwYjIyZTA2MTEiLCJ0IjoiNmJkMjllMTEtMTM4Ny00ZTliLWFhYmItMjdmODBkZmJhYWI4IiwicyI6Ik1UUXdZMll4TUdFdFlUVm1NUzAwTWpnd0xXSTRaVGd0WXpFMk9HTmlOR1F6TURCaiJ9
*/

// ============================================================================
// 1. REGISTROS DE CONTROL DE GPIO (Bloque 0x60000300)
// ============================================================================
#define GPIO_BASE_ADDR          0x60000300

// Registros de Dirección (Configuran el modo: Salida o Entrada)
// Set: 1 = Salida
// Clear: 1 = Entrada

#define GPIO_ENABLE_W1TS        (*(volatile uint32_t *)(GPIO_BASE_ADDR + 0x0C))
#define GPIO_ENABLE_W1TC        (*(volatile uint32_t *)(GPIO_BASE_ADDR + 0x10)) 

// Registros de Salida (Escriben voltaje en el pin)
// Set: 1 = HIGH (3.3V)
// Clear: 1 = LOW (GND)

#define GPIO_OUT_W1TS           (*(volatile uint32_t *)(GPIO_BASE_ADDR + 0x04))
#define GPIO_OUT_W1TC           (*(volatile uint32_t *)(GPIO_BASE_ADDR + 0x08)) 

// Registro de Lectura (Lee el voltaje actual del pin)
#define GPIO_IN                 (*(volatile uint32_t *)(GPIO_BASE_ADDR + 0x18))

// ============================================================================
// 1.1 REGISTROS DE UART
// ============================================================================

#define UART0_BASE_ADDR        0x60000000
#define UART0_FIFO             (*(volatile uint32_t *)(UART0_BASE_ADDR + 0x00))
#define UART0_STATUS           (*(volatile uint32_t *)(UART0_BASE_ADDR + 0x1C))

#define UART_TX_FIFO_MASK      0x000000FF 
#define UART_TX_FIFO_SHIFT     16

// ============================================================================
// 2. REGISTROS IOMUX (Bloque 0x60000800)
// ============================================================================
#define MUX_BASE_ADDR           0x60000800

// Registros específicos por Pin (Basados en el Datasheet)
#define CONF_D4_GPIO2           (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x04))
#define CONF_D5_GPIO14          (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x10))
#define CONF_D6_GPIO12          (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x08))
#define CONF_D7_GPIO13          (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x0C))
#define CONF_SDA                (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x3C))
#define CONF_SCL                (*(volatile uint32_t *)(MUX_BASE_ADDR + 0x40))

// Bits de configuración dentro de los registros MUX
#define MUX_PULLUP_BIT          (1 << 7)
#define MUX_FUNC_GPIO           0x00    // Simplificado para la mayoría de pines

// ============================================================================
// 3. DEFINICIÓN DE PINES Y MÁSCARAS
// ============================================================================
#define LED_BOARD   2
#define LED_YELLOW  12
#define LED_GREEN   13
#define LED_RED     14
#define SDA_PIN     4
#define SCL_PIN     5

#define LED_BOARD_MASK  (1 << LED_BOARD)

#define LED_YELLOW_MASK (1 << LED_YELLOW)
#define LED_GREEN_MASK  (1 << LED_GREEN)
#define LED_RED_MASK    (1 << LED_RED)
#define ALL_STATUS_LEDS (LED_YELLOW_MASK | LED_GREEN_MASK | LED_RED_MASK)

#define SDA_MASK        (1 << SDA_PIN)
#define SCL_MASK        (1 << SCL_PIN)

// ============================================================================
// 4. MACROS DE ACCESO RÁPIDO
// ============================================================================

// Dirección
#define setOUTPUT(pin)        (GPIO_ENABLE_W1TS = (1 << (pin)))
#define setINPUT(pin)         (GPIO_ENABLE_W1TC = (1 << (pin)))

// Estado
#define setHIGH(pin)          (GPIO_OUT_W1TS = (1 << (pin)))
#define setLOW(pin)           (GPIO_OUT_W1TC = (1 << (pin)))

// Lectura
#define READ_PIN(pin)         ((GPIO_IN >> (pin)) & 1)

// Pull-up (Actúa sobre el registro MUX, no sobre el registro GPIO)
#define setPULLUP(reg)         (reg |= MUX_PULLUP_BIT)
#define clearPULLUP(reg)       (reg &= ~MUX_PULLUP_BIT)

// ============================================================================
// 5. FUNCIONES
// ============================================================================

void resetESP(){
    (*(volatile uint32_t *)(0x60000700 + 0x30)) = 0x10; 
    while(1);
}

#endif // MACROS_H