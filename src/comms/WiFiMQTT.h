#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <ArduinoJson.h>
#include <functional>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include "spi_driver.h"
#include "uart_driver.h"
#include "I2C_Driver.h"

// Backend types
enum BackendType {
    BACKEND_NODE_RED = 0,
    BACKEND_AWS = 1,
    BACKEND_THINGSBOARD = 2
};

extern BackendType backend;

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
    // WiFi / MQTT
    WiFiClient        _plainClient;
    WiFiClientSecure  _secureClient;
    PubSubClient      _mqtt;

    CommandCallback   _commandCallback;

    // Timing
    unsigned long     _lastWifiCheck;
    unsigned long     _lastMqttReconnect;
    uint32_t          _mqttBackoffMs;

    // Task
    TaskHandle_t      _taskHandle;

    // Internal helpers
    void configureMQTT();
    void ensureWifi();
    void ensureMqtt();
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static void taskEntry(void* pv);

    static WiFiMQTT* instance;
};

#endif
