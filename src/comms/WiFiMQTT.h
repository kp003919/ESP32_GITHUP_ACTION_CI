#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "spi_driver.h"
#include "uart_driver.h"
#include "I2C_Driver.h"

// -----------------------------------------------------------------------------
// MQTT Topics
// -----------------------------------------------------------------------------
constexpr const char* TOPIC_TELEMETRY   = "esp32/telemetry";
constexpr const char* TOPIC_FAN_CMD     = "esp32/fan/cmd";
constexpr const char* TOPIC_HEATER_CMD  = "esp32/heater/cmd";
constexpr const char* TOPIC_WIFI_CMD    = "esp32/anchor_01/wifi/cmd";
constexpr const char* TOPIC_WIFI_STATUS = "esp32/anchor_01/wifi/status";
constexpr const char* TOPIC_RTLS_CMD    = "esp32/anchor_01/rtls/cmd";
constexpr const char* TOPIC_I2C_CMD     = "esp32/anchor_01/i2c/cmd";
constexpr const char* TOPIC_SPI_CMD     = "esp32/anchor_01/spi/cmd";
constexpr const char* TOPIC_UART_CMD    = "esp32/anchor_01/uart/cmd";
constexpr const char* TOPIC_TEST_CMD    = "esp32/anchor_01/test/cmd";

// -----------------------------------------------------------------------------
// Backend Types
// -----------------------------------------------------------------------------
enum BackendType {
    BACKEND_NODE_RED = 0,
    BACKEND_AWS,
    BACKEND_THINGSBOARD
};

// -----------------------------------------------------------------------------
// WiFi / MQTT Status Enums
// -----------------------------------------------------------------------------
enum WIFI_STATUS {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED
};

enum MQTT_STATUS {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED
};

enum MQTT_PUB_STATUS {
    MQTT_PUB_OK = 0,
    MQTT_PUB_FAIL
};

// -----------------------------------------------------------------------------
// Global Status Variables
// -----------------------------------------------------------------------------
extern BackendType backend;
extern WIFI_STATUS wifi_status;
extern MQTT_STATUS mqtt_status;
extern MQTT_PUB_STATUS mqtt_pub_status;

// -----------------------------------------------------------------------------
// WiFiMQTT Class
// -----------------------------------------------------------------------------
class WiFiMQTT {
public:
    using CommandCallback = std::function<void(const String&, const String&)>;
    WiFiMQTT();

    void begin();
    void loop();
    void sendTelemetry(const JsonDocument& doc);
    void publish(const char* topic, const JsonDocument& doc);

    void setBackend(BackendType b) { backend = b; }
    void setCommandCallback(CommandCallback cb);

    // Optional FreeRTOS task for MQTT loop
    void startTask(UBaseType_t priority = 1,
                   uint32_t stackSize = 4096,
                   BaseType_t core = 1);

private:
    WiFiClient        _plainClient;
    WiFiClientSecure  _secureClient;
    PubSubClient      _mqtt;

    CommandCallback   _commandCallback = nullptr;

    unsigned long     _lastWifiCheck = 0;
    unsigned long     _lastMqttReconnect = 0;
    uint32_t          _mqttBackoffMs = 1000;

    TaskHandle_t      _taskHandle = nullptr;

    void configureMQTT();
    void ensureWifi();
    void ensureMqtt();
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static void taskEntry(void* pv);

    static WiFiMQTT* instance;
};

#endif
