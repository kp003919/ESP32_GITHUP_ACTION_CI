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
#include <freertos/queue.h>

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

/**
 * WiFiMQTT Class
 * @details  class encapsulates the functionality for managing WiFi and MQTT communications. 
 * It provides methods to initialize the communication, handle incoming MQTT messages, 
 * send telemetry data, and manage the connection status. The class supports different 
 * backends (Node-RED, AWS, ThingsBoard) and allows the main application to register a 
 * callback for handling commands received via MQTT. It also includes an optional FreeRTOS 
 * task for running the MQTT loop in a separate thread. 
 * 
 * @note: The class is designed to be used in an ESP32 environment with the Arduino framework,
 * and it relies on the PubSubClient library for MQTT communication. The class is intended      
 * to be used in conjunction with other modules for sensor data collection and actuator control,
 * allowing for a modular and extensible IoT application architecture.  
 * 
 * 
 *  
 * 
 */
class WiFiMQTT {
public:
    // Type alias for command callback function 
    using CommandCallback = std::function<void(const String&, const String&)>;
    // Constructor
    WiFiMQTT(); 
   /**
    * @brief Initializes the WiFi and MQTT communication.
    * This method sets up the WiFi connection and configures the MQTT client. 
    * It should be called in the setup() function of the main application to 
    * establish communication with the MQTT broker. The method handles the initial 
    * connection and prepares the system for sending and receiving messages.        
    * 
    */
    void begin();
    /**
     * @brief Main loop for handling WiFi and MQTT communication.
     * This method should be called regularly in the main loop() function of the application. 
     * It manages the WiFi connection, processes incoming MQTT messages, and maintains the 
     * connection to the MQTT broker. The method ensures that the system remains responsive 
     * to incoming commands and can send telemetry data as needed. It also handles reconnection 
     * logic in case of disconnections.  
     */
    void loop();
    /**
     * @brief Sends telemetry data to the MQTT broker.
     * This method takes a JSON document containing telemetry data and publishes it to the 
     * configured telemetry topic. The JSON document should be structured according to the 
     * expected format for the backend being used (e.g., Node-RED, AWS, ThingsBoard). The method 
     * handles serialization of the JSON document and ensures that the data is sent over the 
     * established MQTT connection.  
     * 
     * @param doc Reference to a JSON document containing telemetry data to be sent.  
     */
    void sendTelemetry(const JsonDocument& doc);
    /**
     * @brief Publishes a JSON document to a specified MQTT topic.
     * This method allows the application to send arbitrary JSON data to any topic on the MQTT broker. 
     * It takes the topic name and a JSON document as parameters, serializes the JSON document, and 
     * publishes it to the specified topic. The method can be used for sending commands, status updates, 
     * or any other structured data that needs to be communicated over MQTT.  
     * 
     * @param topic The MQTT topic to which the JSON document will be published.  
     * @param doc Reference to a JSON document containing the data to be published.  
     */
    void publish(const char* topic, const JsonDocument& doc);

    /**
     * @brief Sets the backend type for the MQTT communication.
     * This method allows the application to specify the backend type (e.g., Node-RED, AWS, ThingsBoard) for the MQTT communication. The backend type determines how the JSON documents are structured and sent over the MQTT connection.
     * 
     * @param b The backend type to be set.
     */
    void setBackend(BackendType b) { backend = b; }

    /**
     * @brief Registers a callback function for handling incoming commands.
     * This method allows the main application to register a callback function that will be called whenever a command is received via MQTT. The callback function should take two parameters: the command key and the command value. The expected format of incoming MQTT messages is "key=value". The registered callback will be invoked with the parsed key and value, allowing the application to handle commands in a structured manner.
     * 
     * @param cb A function or lambda that takes two String parameters (key and value) and handles the command.
     */
    void setCommandCallback(CommandCallback cb);

    // Optional FreeRTOS task for MQTT loop
    void startTask(UBaseType_t priority = 1,
                   uint32_t stackSize = 4096,
                   BaseType_t core = 1);

// Private members
private:
    WiFiClient        _plainClient; // Plain WiFi client for non-secure connections 
    WiFiClientSecure  _secureClient;// Secure WiFi client for TLS connections   
    PubSubClient      _mqtt; // MQTT client for handling MQTT communication 
   
    // Command callback function pointer for handling incoming commands 
    CommandCallback   _commandCallback = nullptr;

    // Timing variables for managing WiFi and MQTT reconnections
    unsigned long     _lastWifiReconnect = 0;
    unsigned long     _lastWifiCheck = 0;
    unsigned long     _lastMqttReconnect = 0;
    uint32_t          _mqttBackoffMs = 1000;

    TaskHandle_t      _taskHandle = nullptr;
    QueueHandle_t _txQueue = nullptr;
    uint32_t      _dropped = 0;
    bool enqueue(const char* topic, const JsonDocument& doc);
    void drainQueue();

    
    // Private methods for internal operations  
    void configureMQTT();
    void ensureWifi();
    void ensureMqtt();
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static void taskEntry(void* pv);

    static WiFiMQTT* instance;
};

#endif
