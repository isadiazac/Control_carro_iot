// ====== config.h ======
#pragma once

// ====== CONFIG WI-FI ======
#define WIFI_SSID       "Isa"
#define WIFI_PASS       "12345678"

// ====== CONFIG MQTT ======
// Dominio con el que funciona tu certificado TLS
#define MQTT_BROKER       "172.20.10.3"
#define MQTT_PORT         8883
#define MQTT_CLIENT_ID    "esp32_car"

// Si NO usas autenticación MQTT, déjalos vacíos
#define MQTT_USERNAME     ""
#define MQTT_PASSWORD     ""

// Temas MQTT
#define MQTT_TOPIC_MOVE     "esp32car/http"
#define MQTT_TOPIC_SENSOR   "esp32car/distance"

// ====== PINES MOTORES ======
#define ENA 25
#define IN1 26
#define IN2 27
#define ENB 33
#define IN3 32
#define IN4 14

// ====== PINES LEDS ======
#define LED_IZQ    18
#define LED_DER    23
#define LED_STOP   19
#define LED_REV    22
#define LED_AZULES 21

// ====== HC-SR04 CONFIG ======
#define USE_HCSR04 1
#define HCSR04_TRIG_PIN  4
#define HCSR04_ECHO_PIN  5
#define HCSR04_MAX_CM    400

// ====== SENSOR PUBLISH ======
#define SENSOR_PUBLISH_INTERVAL_MS 2000

// ====== PWM / CONTROL ======
#define PWM_FREQ_HZ  20000
#define PWM_RES_BITS 10
#define PWM_MAX      ((1 << PWM_RES_BITS) - 1)

// ====== OTHER ======
#define REV_BLINK_MS 250
