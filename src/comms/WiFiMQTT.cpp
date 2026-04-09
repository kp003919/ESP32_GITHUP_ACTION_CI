/*
 * WiFiMQTT.cpp - WiFi and MQTT communication for ESP32
 * This module handles connecting to WiFi, connecting to an MQTT broker (Node-RED), subscribing to command topics, and publishing telemetry data.
 * It uses the PubSubClient library for MQTT communication and the ArduinoJson library for JSON handling.
 * The module is designed to be used in an ESP32-based IoT project where the device needs to communicate with a Node-RED instance running on a Raspberry Pi.
 * 
 * Key features:
 * - Connects to WiFi using credentials defined in secrets_new.h
 * - Connects to an MQTT broker (Node-RED) at a specified IP and port
 * - Subscribes to command topics for controlling fans, heaters, and WiFi settings
 * - Publishes telemetry data as JSON documents to a specified topic
 * - Provides a callback mechanism for handling incoming commands in the main application
 * 
 */ 
#include "WiFiMQTT.h"
#include "../config.h"
#include "../secrets_new.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// ------------------------------------------------------
// NODE-RED ONLY
// ------------------------------------------------------
static WiFiClient plainClient; // We use a single WiFiClient for MQTT. If we wanted to support multiple backends, we would need to manage multiple clients and connections. 

static PubSubClient mqtt(plainClient); // MQTT client using the plain WiFiClient. This is sufficient for connecting to a local Node-RED instance without TLS. If we wanted to support secure connections, we would need to use WiFiClientSecure and manage certificates.    


// MQTT broker (Node-RED / Mosquitto on Pi or PC )
static const char* NR_BROKER = "192.168.0.92"; // 
static const int   NR_PORT   = 1883;

// MQTT topics  
static const char* TOPIC_TELEMETRY = "esp32/telemetry";
static const char* TOPIC_FAN_CMD   = "esp32/fan/cmd";
static const char* TOPIC_HEATER_CMD   = "esp32/heater/cmd";
static const char* TOPIC_WIFI_CMD  = "esp32/anchor_01/wifi/cmd";
static const char* TOPIC_PROTOCOLS = "device/protocols";

// Command callback function pointer    
// This callback will be set by the main application to handle incoming commands from MQTT. It takes a key and value as parameters, which are parsed from the incoming MQTT messages. The main application can implement this callback to perform actions based on the received commands, such as controlling fans, heaters, or updating WiFi settings. 
// The callback is defined as a std::function, allowing for flexible assignment of any callable object (e.g., lambda, function pointer, std::bind result) that matches the signature.   
WiFiMQTT::CommandCallback commandCallback = nullptr;

/**  
 * MQTT callback function
 * This function is called by the PubSubClient library when a message is received on a subscribed   topic. It parses the incoming message, extracts the key and value, and then forwards them to the main application through the commandCallback function pointer. 
 * The expected format of the incoming MQTT message is "key=value". The function checks for this format, and if valid, it splits the message into key and value components, trims any whitespace, and then calls the commandCallback with the parsed key and value. If the format is invalid, it logs an error message to the console. 
 * Note: The commandCallback should be set by the main application using the setCommandCallback method of the WiFiMQTT class for this mechanism to work. If no callback is set, incoming commands will be ignored after parsing.    
 * This function is registered with the MQTT client using mqtt.setCallback(mqttCallback) in the begin() method, ensuring that it will be called for any incoming messages on subscribed topics. 
 *  
 */
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    // Convert payload to string
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.println("\n[MQTT] RAW message: " + msg);

    // Parse key=value
    int sep = msg.indexOf('=');
    if (sep == -1) {
        Serial.println("[MQTT] Invalid format. Expected key=value");
        return;
    }

    String key   = msg.substring(0, sep);
    String value = msg.substring(sep + 1);

    key.trim();
    value.trim();

    // Forward to main.cpp
    if (commandCallback) {
        commandCallback(key,value);   
    }
}


// ------------------------------------------------------
// Command callback setter
// ------------------------------------------------------
void WiFiMQTT::setCommandCallback(CommandCallback cb) {
    commandCallback = cb;
}

/**
 * Configure MQTT client
 * This function sets up the MQTT client with the appropriate server and callback function. It is called    during the initialization process in the begin() method to ensure that the MQTT client is ready to connect and handle incoming messages.    
 * The function sets the MQTT server to the specified broker IP and port, and registers the mqttCallback function to handle incoming messages. This setup is essential for enabling MQTT communication with the Node-RED instance and allowing the device to receive commands and publish telemetry data as needed.    
 * Note: If we wanted to support multiple backends or secure connections, we would need to modify this function to manage multiple clients and configure TLS settings accordingly. For the current use case of connecting to a local Node-RED instance without TLS, this configuration is sufficient.       
 *  
 */
void WiFiMQTT::configureMQTT() {
    mqtt.setClient(plainClient);
    mqtt.setServer(NR_BROKER, NR_PORT);
    Serial.println("[MQTT] Node-RED configured");
}

/**
 * Initialize WiFi and MQTT
 * This function initializes the WiFi connection and configures the MQTT client. 
 * It is called during the setup process to ensure that the device is connected to 
 * the network and ready to communicate via MQTT.   
 * The function first attempts to connect to the WiFi network using the credentials defined in secrets_new.h. It waits until a connection is established, printing status messages to the console. Once connected, it prints the assigned IP address and calls configureMQTT() to set up the MQTT client with the appropriate server and callback function. Finally, it registers the mqttCallback function to handle incoming MQTT messages. 
 * Note: The function   assumes that the WiFi credentials and MQTT broker information are correctly defined in the configuration files. If the WiFi connection fails, it will block indefinitely until a connection is established. For a more robust implementation, you might want to add timeout handling or retry logic for the WiFi connection.        
 *      
 *  
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
}

/**
 * MQTT loop
 * This function should be called regularly in the main loop to maintain the MQTT connection and process incoming       
 *  messages. It checks if the MQTT client is connected, and if not, it attempts to reconnect. If the connection is successful, it subscribes to the relevant command topics. Finally, it calls mqtt.loop() to process any incoming messages and maintain the connection. 
 * Note: The function assumes that the MQTT client is properly configured and that the MQTT broker is               
 * available. If the connection to the MQTT broker fails, it will print the error state to the console and continue trying to reconnect on subsequent calls. For a more robust implementation, you might want to add additional error handling or backoff strategies for reconnection attempts.         
 * 
 */
void WiFiMQTT::loop() {
    if (!mqtt.connected()) {
        Serial.println("[MQTT] Reconnecting...");

        configureMQTT();

        String clientId = "esp32-" + String(random(0xFFFF), HEX);
        bool ok = mqtt.connect(clientId.c_str());

        if (ok) {
            Serial.println("[MQTT] Connected.");
            mqtt.subscribe(TOPIC_FAN_CMD);
            mqtt.subscribe(TOPIC_WIFI_CMD);
            mqtt.subscribe(TOPIC_HEATER_CMD);
        } else {
            Serial.print("[MQTT] Failed. State=");
            Serial.println(mqtt.state());
        }
    }

    mqtt.loop();
}

/**
 * Send telemetry data
 * This function serializes the provided JSON document and publishes it to the telemetry topic.
 * It prints a message to the console indicating that telemetry data is being sent.
 * @param doc The JSON document containing the telemetry data to be sent.
 */

void WiFiMQTT::sendTelemetry(const JsonDocument& doc) {
    Serial.println("[TX] sendTelemetry()");

    char buf[512];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0) {
        Serial.println("[TX] JSON too large");
        return;
    }

    //Serial.print("[TX] Node-RED -> ");
    //Serial.println(buf);

    mqtt.publish(TOPIC_TELEMETRY, buf);
    mqtt.publish("test", "to PI");
}

/**
 * Publish to a topic
 * This function serializes the provided JSON document and publishes it to the specified MQTT topic. 
 * It prints a message to the console indicating the topic and the raw message being sent. 
 * @param topic The MQTT topic to which the message should be published.
 * @param doc The JSON document containing the data to be published.
 */ 

void WiFiMQTT::publish(const char* topic, const JsonDocument& doc) {
    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    //Serial.print("[TX] ");
   // Serial.print(topic);
   // Serial.print(" -> ");
   // Serial.println(buf);
    mqtt.publish(topic, buf);
}
