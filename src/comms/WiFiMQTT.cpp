/*
 * WiFiMQTT.cpp - WiFi + MQTT communication for ESP32
 * ---------------------------------------------------
 * This module provides:
 *   - WiFi auto‑reconnect
 *   - MQTT reconnect with exponential backoff
 *   - Optional TLS (WiFiClientSecure)
 *   - JSON command parsing (with fallback to key=value)
 *   - Optional FreeRTOS task for MQTT loop
 *
 * It is designed to be robust for real IoT deployments.
 */

#include "WiFiMQTT.h"
#include "../config.h"
#include "../secrets_new.h"

// -----------------------------------------------------------------------------
// Default MQTT broker settings (can be overridden in config.h)
// -----------------------------------------------------------------------------
#ifndef MQTT_BROKER
#define MQTT_BROKER "192.168.0.21"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef USE_MQTT_TLS
#define USE_MQTT_TLS 0
#endif

// Global backend selection
BackendType backend = BACKEND_NODE_RED;




// Static instance pointer for static callback trampoline
WiFiMQTT* WiFiMQTT::instance = nullptr;

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
WiFiMQTT::WiFiMQTT()
: _plainClient()
, _secureClient()
, _mqtt(_plainClient)        // default client (can switch to TLS later)
, _commandCallback(nullptr)
, _lastWifiCheck(0)
, _lastMqttReconnect(0)
, _mqttBackoffMs(3000)       // start with 3s reconnect delay
, _taskHandle(nullptr)
{
}



WIFI_STATUS wifi_status = WIFI_STATUS_DISCONNECTED;
MQTT_STATUS mqtt_status = MQTT_STATUS_DISCONNECTED;
MQTT_PUB_STATUS mqtt_pub_status = MQTT_PUB_FAIL;


// -----------------------------------------------------------------------------
// Static MQTT callback → forwards to instance method
// -----------------------------------------------------------------------------
void WiFiMQTT::mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->handleMqttMessage(topic, payload, length);
    }
}

// -----------------------------------------------------------------------------
// begin() — Connect to WiFi and configure MQTT
// -----------------------------------------------------------------------------
void WiFiMQTT::begin() {
    instance = this;

    Serial.println("[WiFi] Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Block until WiFi connects
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }

    Serial.print("\n[WiFi] Connected. IP=");
    Serial.println(WiFi.localIP());

    

    configureMQTT();
}

// -----------------------------------------------------------------------------
// configureMQTT() — Select client (TLS or plain) and set callback
// -----------------------------------------------------------------------------
void WiFiMQTT::configureMQTT() {
#if USE_MQTT_TLS
    Serial.println("[MQTT] Using TLS");
    _secureClient.setInsecure();   // or setCACert(MQTT_CA_CERT)
    _mqtt.setClient(_secureClient);
#else
    Serial.println("[MQTT] Using plain TCP");
    _mqtt.setClient(_plainClient);
#endif

    _mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    _mqtt.setCallback(WiFiMQTT::mqttCallbackStatic);

    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
}

// -----------------------------------------------------------------------------
// ensureWifi() — Auto‑reconnect WiFi every 5 seconds if disconnected
// -----------------------------------------------------------------------------
void WiFiMQTT::ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) {
         wifi_status = WIFI_STATUS_CONNECTED;
         return;
    }

    wifi_status = WIFI_STATUS_CONNECTING;
    unsigned long now = millis();
    if (now - _lastWifiCheck < 5000) return;
    _lastWifiCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        wifi_status = WIFI_STATUS_CONNECTED;

    } else {
        wifi_status = WIFI_STATUS_DISCONNECTED;
    }
}

// -----------------------------------------------------------------------------
// ensureMqtt() — Reconnect MQTT with exponential backoff
// -----------------------------------------------------------------------------
void WiFiMQTT::ensureMqtt() {
    if (_mqtt.connected())
    { 
        mqtt_status = MQTT_STATUS_CONNECTED;
        return; 

    } 
     
    mqtt_status = MQTT_STATUS_CONNECTING;
    unsigned long now = millis();
    if (now - _lastMqttReconnect < _mqttBackoffMs) return;
    _lastMqttReconnect = now;

    Serial.println("[MQTT] Reconnecting...");

    configureMQTT();

    // Unique client ID
    String clientId = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    bool ok = _mqtt.connect(clientId.c_str()
#ifdef MQTT_USERNAME
                            , MQTT_USERNAME, MQTT_PASSWORD
#endif
    );

    if (ok) {
        Serial.println("[MQTT] Connected.");
        mqtt_status = MQTT_STATUS_CONNECTED;

        // Subscribe to command topics
        _mqtt.subscribe(TOPIC_FAN_CMD);
        _mqtt.subscribe(TOPIC_HEATER_CMD);
        _mqtt.subscribe(TOPIC_WIFI_CMD);

        // Reset backoff on success
        _mqttBackoffMs = 3000;
    } else {
        Serial.printf("[MQTT] Failed. State=%d\n", _mqtt.state());
            mqtt_status = MQTT_STATUS_DISCONNECTED;

        // Exponential backoff (max 60s)
        _mqttBackoffMs = min<uint32_t>(_mqttBackoffMs * 2, 60000);
    }
}

// -----------------------------------------------------------------------------
// loop() — Must be called frequently (or run inside FreeRTOS task)
// -----------------------------------------------------------------------------
void WiFiMQTT::loop() {
    ensureWifi();
    ensureMqtt();

    if (_mqtt.connected()) {
        _mqtt.loop();
    }
}

// -----------------------------------------------------------------------------
// startTask() — Run MQTT loop inside a dedicated FreeRTOS task
// -----------------------------------------------------------------------------
void WiFiMQTT::startTask(UBaseType_t priority,
                         uint32_t stackSize,
                         BaseType_t core) {
    if (_taskHandle) return;

    xTaskCreatePinnedToCore(
        WiFiMQTT::taskEntry,
        "mqttTask",
        stackSize,
        this,
        priority,
        &_taskHandle,
        core
    );
}

void WiFiMQTT::taskEntry(void* pv) {
    WiFiMQTT* self = static_cast<WiFiMQTT*>(pv);
    for (;;) {
        self->loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// -----------------------------------------------------------------------------
// setCommandCallback() — Register user callback
// -----------------------------------------------------------------------------
void WiFiMQTT::setCommandCallback(CommandCallback cb) {
    _commandCallback = cb;
}

// -----------------------------------------------------------------------------
// handleMqttMessage() — Parse JSON or key=value commands
// -----------------------------------------------------------------------------
void WiFiMQTT::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    String msg;
    msg.reserve(length + 1);

    for (unsigned int i = 0; i < length; ++i) {
        msg += (char)payload[i];
    }

    Serial.printf("\n[MQTT] Topic: %s\n", t.c_str());
    Serial.printf("[MQTT] Payload: %s\n", msg.c_str());

    if (!_commandCallback) {
        Serial.println("[MQTT] No command callback set.");
        return;
    }

    // -------------------------
    // 1) Try JSON format
    // -------------------------
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
        String key, value;

        // Format A: {"key":"fan","value":"on"}
        if (doc.containsKey("key") && doc.containsKey("value")) {
            key   = doc["key"].as<String>();
            value = doc["value"].as<String>();
        }
        // Format B: {"fan":"on"}
        else {
            for (JsonPair kv : doc.as<JsonObject>()) {
                key   = kv.key().c_str();
                value = kv.value().as<String>();
                break;
            }
        }

        key.trim();
        value.trim();

        if (key.length() == 0) {
            Serial.println("[MQTT] JSON has no usable key");
            return;
        }

        _commandCallback(key, value);
        return;
    }

    // -------------------------
    // 2) Fallback: key=value
    // -------------------------
    int sep = msg.indexOf('=');
    if (sep == -1) {
        Serial.println("[MQTT] Invalid format. Expected JSON or key=value");
        return;
    }

    String key   = msg.substring(0, sep);
    String value = msg.substring(sep + 1);
    key.trim();
    value.trim();

    _commandCallback(key, value);
}

// -----------------------------------------------------------------------------
// sendTelemetry() — Publish JSON telemetry
// -----------------------------------------------------------------------------
void WiFiMQTT::sendTelemetry(const JsonDocument& doc) {
    if (!_mqtt.connected()) {
        Serial.println("[TX] MQTT not connected, dropping telemetry");
        return;
    }

    char buf[512];
    if (serializeJson(doc, buf, sizeof(buf)) == 0) {
        Serial.println("[TX] JSON too large");
        return;
    }

    Serial.println("[TX] Telemetry -> esp32/telemetry");
    _mqtt.publish(TOPIC_TELEMETRY, buf);
}

// -----------------------------------------------------------------------------
// publish() — Generic JSON publish
// -----------------------------------------------------------------------------
void WiFiMQTT::publish(const char* topic, const JsonDocument& doc) {
    if (!_mqtt.connected()) {
        Serial.println("[TX] MQTT not connected, dropping publish");
        mqtt_pub_status = MQTT_PUB_FAIL;
        return;
    }

    char buf[512];
    if (serializeJson(doc, buf, sizeof(buf)) == 0) {
        Serial.println("[TX] JSON too large");
        mqtt_pub_status = MQTT_PUB_FAIL;
        return;
    }
    
    mqtt_pub_status = MQTT_PUB_OK;
    Serial.printf("[TX] %s -> %s\n", topic, buf);
    _mqtt.publish(topic, buf);
}
