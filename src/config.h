#pragma once

// Communication modes
#define COMMS_WIFI_MQTT  0 
#define COMMS_LORAWAN    1

// Select active mode
#define COMMS_MODE COMMS_WIFI_MQTT

// Device ID
#define DEVICE_ID "ANCHOR_01"

// MQTT broker
#define MQTT_BROKER "192.168.0.21"
#define MQTT_PORT   1883

// DHT config
#define DHTPIN  3        // GPIO 3 is safe
#define DHTTYPE DHT22   // DHT22 sensor for better accuracy

// LoRaWAN keys (placeholder)
// These should be replaced with actual keys for LoRaWAN operation  
// For testing, you can use dummy keys. In production, use secure key management practices. 

#define LORAWAN_DEVEUI {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}
#define LORAWAN_APPEUI {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}
#define LORAWAN_APPKEY {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}

// More GPIOs
#define RESET_WIFI_PIN 2  // GPIO for WiFi reset button (example)
#define RESET_ESP_PIN 15  // GPIO for ESP32 reset button (example)
#define FAN_PIN 38        // GPIO for fan control (example)
#define GPS_RX_PIN 16    // GPIO for GPS RX (to ESP32 TX)
#define GPS_TX_PIN 17    // GPIO for GPS TX (to ESP32 RX)  
#define I2C_SDA_PIN 21  // GPIO for I2C SDA
#define I2C_SCL_PIN 22  // GPIO for I2C SCL  
#define HEATER_PIN 5   // GPIO for heater control (example)

// RTLS (BLE) config
#define RTLS_SCAN_INTERVAL 5000 // 5 seconds    

