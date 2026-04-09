#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <ArduinoJson.h>
#include <functional>   // REQUIRED for std::function
#include "spi_driver.h"
#include "uart_driver.h"
#include "I2C_Driver.h"

// Backend types
enum BackendType {
    BACKEND_NODE_RED = 0,
    BACKEND_AWS = 1,
    BACKEND_THINGSBOARD = 2
};

// Global backend state
extern BackendType backend;

class WiFiMQTT {
public:
    // ---- Core API ----
    void begin();
    void loop();
    void sendTelemetry(const JsonDocument& doc);
    void publish(const char* topic, const JsonDocument& doc);
    void setBackend(BackendType b);

    // ---- Command callback type ----
    using CommandCallback = std::function<void(const String&, const String&)>;

    // ---- Register callback ----
    void setCommandCallback(CommandCallback cb);

private:
    void configureMQTT();
};

#endif
