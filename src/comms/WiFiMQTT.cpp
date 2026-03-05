/**
 * WiFiMQTT.cpp - Implementation of WiFiMQTT class for ESP32 MQTT communication 
 * This class manages WiFi connectivity and MQTT communication using the PubSubClient library. 
 * It connects to a specified MQTT broker, subscribes to relevant topics, and handles incoming 
 * messages to control actuators (e.g., fan). It also provides methods to send telemetry data as 
 * JSON payloads to the MQTT broker. The implementation is designed for use in an IoT project 
 * where the ESP32 collects sensor data and allows remote control via MQTT messages.  
 *  
 * Note: This implementation assumes a local MQTT broker (e.g., Node-RED) is running at the 
 * specified IP address and port. The MQTT topics and message formats can be customized as 
 * needed for the specific application. The code also includes basic error handling and logging 
 * for debugging purposes.    
 *  
 * Author: Muhsin Atto 
 * Date: September 18, 2024 
 * 
 * Copyright (c) 2024 Muhsin Atto. All rights reserved.
 * This code is licensed under the MIT License. 
 */

#include "WiFiMQTT.h"
#include "../config.h"
#include "../secrets_new.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// MQTT Broker settings

// ------------------------------------------------------
// NODE-RED ONLY
// ------------------------------------------------------
static WiFiClient plainClient;
static PubSubClient mqtt(plainClient);

static const char* NR_BROKER = "192.168.0.21";
static const int   NR_PORT   = 1883;

// Topics
static const char* TOPIC_TELEMETRY = "esp32/telemetry";
static const char* TOPIC_FAN_CMD   = "esp32/anchor_01/fan/cmd";
static const char * TOPIC_WIFI_CMD  = "esp32/anchor_01/wifi/cmd";
static const char *TOPIC_PROTOCOLS = "device/protocols";




/**
 *  MQTT callback function to handle incoming messages. It parses the topic and payload, 
 * extracts key-value pairs from the payload, and performs actions based on the commands 
 * received (e.g., controlling a fan or WiFi). The expected payload format is "key=value". 
 * The function includes basic error handling for invalid formats and logs the received messages 
 * and actions taken for debugging purposes. Additional commands can be added as needed by 
 * extending the if-else structure.   
 * 
 *@paras topic: The MQTT topic on which the message was received.
 *@paras payload: The raw payload of the MQTT message, which is expected to be a byte array.
 *@paras length: The length of the payload in bytes.        
 *@returns void 
 * 
 */
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    // Convert payload to String
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
   
    // Log received message
    Serial.println("\n[MQTT] Message received:");
    Serial.print("[MQTT RX] ");
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(msg);

    // ------------------------------------------------------
    // PARSE key=value
    // ------------------------------------------------------
    // check for '=' separator (make sure it's there and not at the start or end)
    int sep = msg.indexOf('=');
    if (sep == -1) {
        Serial.println("[MQTT] Invalid format. Expected key=value");
        return;
    }
    // Extract key and value
    String key   = msg.substring(0, sep);
    String value = msg.substring(sep + 1);

    key.trim();
    value.trim();

    Serial.println("[PARSE] Key   = " + key);
    Serial.println("[PARSE] Value = " + value);

    // ------------------------------------------------------
    // FAN CONTROL
    // ------------------------------------------------------
    if (String(topic) == TOPIC_FAN_CMD) {
        if (key == "fan") {
            if (value == "on") {
                digitalWrite(FAN_PIN, HIGH);
                Serial.println("[FAN] Turned ON");
            }
            else if (value == "off") {
                digitalWrite(FAN_PIN, LOW);
                Serial.println("[FAN] Turned OFF");
            }
        }
        return;
    }

    // ------------------------------------------------------
    // WIFI CONTROL (example)
    // ------------------------------------------------------
    if (key == "wifi") {
        if (value == "off") {
            Serial.println("[WIFI] Turning WiFi OFF...");
            WiFi.disconnect(true);
        }
        else if (value == "on") {
            Serial.println("[WIFI] Reconnecting WiFi...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        return;
    }

    // ------------------------------------------------------
    // ADD MORE COMMANDS HERE
    // ------------------------------------------------------
    if (key == "led") {
        if (value == "on") {
            // digitalWrite(LED_PIN, HIGH);
        }
        else if (value == "off") {
            // digitalWrite(LED_PIN, LOW);
        }
        return;
    }
    // Example of publishing a response back to Node-RED 
    // when a specific command is received (e.g., protocols)
    if(key == "protocol") {
        StaticJsonDocument<200> doc;
        Serial.println("Protocol request received. Publishing supported protocols...");  
        doc["type"] = value; // e.g., "supported_protocols": spi,uart or i2c.
        doc["id"]   = DEVICE_ID;
        unsigned long now = millis();
       

    // For receiving, you would typically check if data is available and read it
    // Here we just simulate a received string for demonstration
       

        doc["ts"]   = now;
        if (value == "uart") {
            const char* uartReceivedData = "Received UART Data";
            const char* uartDataToSend = "Hello UART";
            doc["send"] = uartDataToSend;
            doc["recv"] = uartReceivedData;            
            UART_SendString(uartDataToSend);
        } else if (value == "spi") {
            // Example SPI read/write (replace with actual commands for your peripheral)
            uint8_t spiDataToSend = 0xAB; // Example data to send
            uint8_t spiReceivedData = SPI_Transfer(spiDataToSend);
            doc["send"] = spiDataToSend;
            doc["recv"] = spiReceivedData;
        } else if (value == "i2c") {
            doc["send"] = "1234"; // Example data to send
            const char* i2cReceivedData = "Received I2C Data";
            doc["recv"] = i2cReceivedData;
        } else {
            doc["type"] = "unknown_request";
        }       
        char buf[256];
        serializeJson(doc, buf, sizeof(buf));
        mqtt.publish(TOPIC_PROTOCOLS, buf);
        return;
    }       
}

/**
 * configureMQTT() is a helper function to set up the MQTT client with the broker 
 * information and callback. It is called during the initial setup and also when reconnecting 
 * to  *  ensure the MQTT client is properly configured with the correct server and callback 
 * function. This function abstracts away the MQTT configuration details, making it easier to 
 * manage MQTT settings in one place. It also includes a log statement to confirm that the MQTT 
 * client has been configured for the Node-RED broker.   
 * 
 * @returns void
 */
void WiFiMQTT::configureMQTT() {
    mqtt.setClient(plainClient);
    mqtt.setServer(NR_BROKER, NR_PORT);
    Serial.println("[MQTT] Node-RED configured");
}
/**
 * begin() initializes the WiFi connection and configures the MQTT client. It connects to the
 *  specified WiFi network, waits for the connection to be established, and then sets up the MQTT 
 * client with the broker information and callback function. The function also includes logging to 
 * indicate the connection status and the  * assigned IP address. Additionally, it initializes 
 * the fan control pin to a known state (OFF) at startup. This method should be called in the 
 * setup() function of the main sketch to ensure that the WiFi and MQTT connections are 
 * established before entering the main loop. 
 * @returns void
 */
void WiFiMQTT::begin() {
    Serial.println("[WiFi] Connecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }

    Serial.print("\n[WiFi] Connected. IP=");
    Serial.println(WiFi.localIP());

    configureMQTT();
    mqtt.setCallback(mqttCallback);

    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
}
/**
 * loop() manages the MQTT connection and processes incoming messages.
 *  It checks if the MQTT client is connected, and if not, it attempts to 
 * reconnect to the broker. During reconnection, it configures the MQTT client 
 * and tries to connect with a unique client ID. If the connection is successful,
 *  it subscribes to the relevant topic for fan control. The function also includes logging for connection attempts and failures. Finally, it calls mqtt.loop() to allow the MQTT client to process incoming messages and maintain the connection. This method should be called in the main loop() function of the sketch to ensure continuous MQTT communication.
 * @returns void
 */
void WiFiMQTT::loop() {
    if (!mqtt.connected()) {
        Serial.println("[MQTT] Reconnecting...");

        configureMQTT();

        String clientId = "esp32-" + String(random(0xFFFF), HEX);
        bool ok = mqtt.connect(clientId.c_str());

        if (ok) {
            Serial.println("[MQTT] Connected.");
            // now, as MQTT is connected, we can subscribe different  topics             
            mqtt.subscribe(TOPIC_FAN_CMD);    // FAN    
            mqtt.subscribe(TOPIC_WIFI_CMD);   // WIFI
            mqtt.subscribe(TOPIC_PROTOCOLS);  // PROTOCOLS 
        } else {
            Serial.print("[MQTT] Failed. State=");
            Serial.println(mqtt.state());
        }
    }

    mqtt.loop();
}

/**
 * sendTelemetry() takes a JsonDocument as input, serializes it to a JSON string, and publishes it to the MQTT broker on the specified telemetry topic. The function includes error handling for cases where the JSON document is too large to serialize. It also logs the outgoing message for debugging purposes. This method can be called whenever there is new telemetry data to send to the backend, allowing for real-time updates of sensor readings or device status.  
 * @paras doc: A JsonDocument containing the telemetry data to be sent. The document will be serialized to JSON format before being published to the MQTT broker.
 * @returns void
 */
void WiFiMQTT::sendTelemetry(const JsonDocument& doc) {
    Serial.println("[TX] sendTelemetry()");

    char buf[512];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0) {
        Serial.println("[TX] JSON too large");
        return;
    }

    Serial.print("[TX] Node-RED -> ");
    Serial.println(buf);

    mqtt.publish(TOPIC_TELEMETRY, buf);
}

/**
 * publish() is a helper function that takes a topic and a JsonDocument, serializes the document to a JSON string, and publishes it to the specified MQTT topic. This function abstracts away the details of serialization and publishing, allowing the caller to simply provide the topic and the data to be sent. It also includes logging for the outgoing message, which can be useful for debugging and verifying that the correct data is being published to the MQTT broker. This method can be used for sending various types of messages to different topics as needed by the application. 
 * @paras topic: The MQTT topic to which the message should be published.
 * @paras doc: A JsonDocument containing the data to be published. The document will be serialized to JSON format before being published to the MQTT broker.
 * @returns void
 */
void WiFiMQTT::publish(const char* topic, const JsonDocument& doc) {
    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    Serial.print("[TX] ");
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(buf);
    mqtt.publish(topic, buf);
}
