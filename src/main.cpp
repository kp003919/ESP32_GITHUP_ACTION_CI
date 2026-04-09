
/** 
 * Cure IoT Project - ESP32 Firmware    
 * This firmware is designed for an ESP32-based IoT device that collects data from various sensors (DHT22, GPS, RTLS) and communicates with a Node-RED instance running on a Raspberry Pi using MQTT. The device can also receive commands from MQTT to control actuators such as fans and heaters. The code is structured into separate modules for sensors, communication, and real-time location tracking (RTLS), allowing for modular development and maintenance. The main loop handles telemetry collection and transmission at specified intervals, while the command callback processes incoming MQTT messages to control the device's actuators.   
 * Author: Muhsin Atto
 * Date: 2024-06-01 
 * License: MIT License 
 * For more information on ESP32 development, sensor integration, and MQTT communication, refer to the
 *  ESP-IDF documentation, Arduino library documentation for the respective sensors, and the PubSubClient library documentation for MQTT communication.    
 * Details:     
 *  - Uses the Arduino framework for ESP32 development with C++14 standard
 *  - Integrates DHT22 for temperature and humidity, a GPS module for location data                 and time, and a custom RTLS implementation for tracking nearby BLE devices
 *  - Communicates with a Node-RED instance using MQTT for both telemetry and command handling               
 *              
 * 
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets_new.h"
#include "esp_task_wdt.h"

// Sensors
#include "dht/Sensors.h"
#include "gps/GPS.h"
#include "rtls/RTLS.h"

// Protocols (SPI,UART,I2C)
#include <SPI.h>
#include "spi_driver.h"
#include "uart_driver.h"
#include "I2C_Driver.h"

// Comms    
#include "comms/WiFiMQTT.h"

// Actuator pins    
#define FAN_PIN  8     //   fan ON/OFF
#define HEATER_PIN 18  // heater ON/OFF

// Timing variables for telemetry intervals 
unsigned long lastDHT  = 0;
unsigned long lastGPS  = 0;
unsigned long lastRTLS = 0;
unsigned long lastI2C  = 0;
unsigned long lastSPI  = 0;
unsigned long lastUART = 0;

// Telemetry intervals (in milliseconds)    
const unsigned long DHT_INTERVAL  = 5000;
const unsigned long GPS_INTERVAL  = 9000;
const unsigned long RTLS_INTERVAL = 3000;
const unsigned long I2C_INTERVAL  = 2000;
const unsigned long SPI_INTERVAL  = 4000;
const unsigned long UART_INTERVAL = 4000;

// Global comms object
WiFiMQTT comms;

/** 
 * Setup function
 * Initializes all components and starts the system
*/
void setup() {
    // Initialize serial communication for debugging    
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nBooting...");

    // Initialize sensors, GPS, and RTLS    
    Sensors_begin();
    GPS_begin();
    RTLS_begin();

    // Initialize watchdog timer to reset the system if it becomes unresponsive.
    //  The timeout is set to 10 seconds, and the system will reset if the watchdog
    //  is not reset within this period. This helps to ensure that the device can recover 
    // from potential issues such as infinite loops or deadlocks. The watchdog should be 
    // reset regularly in the main loop to prevent unintended resets during normal operation. 

    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    // Initialize communication (WiFi and MQTT) 
    comms.begin();

    // Set up actuator pins and ensure they are in a known state (OFF) at startup       
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
     
    // TURN OFF FAN AND HEATER 
    pinMode(FAN_PIN, OUTPUT);
    pinMode(HEATER_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(HEATER_PIN, LOW);

/**
 * MQTT Command Callback
 * This callback function is registered with the MQTT client to handle incoming messages on subscribed topics. It parses the incoming MQTT message, extracts the key and value, and then forwards them to the main application through the commandCallback function pointer. The expected format of the incoming MQTT message is "key=value". The function checks for this format, and if valid, it splits the message into key and value components, trims any whitespace, and then calls the commandCallback with the parsed key and value. If the format is invalid, it logs an error message to the console. Note: The commandCallback should be set by the main application using the setCommandCallback method of the WiFiMQTT class for this mechanism to work. If no callback is set, incoming commands will be ignored after parsing. This function is registered with the MQTT client using mqtt.setCallback(mqttCallback) in the begin() method, ensuring that it will be called for any incoming messages on subscribed topics. 
 *  
 */ 
comms.setCommandCallback([](const String& key, const String& value)
{
    Serial.println("[CMD] Key   = " + key);
    Serial.println("[CMD] Value = " + value);

    // Validate value
    if (value != "on" && value != "off") {
        Serial.println("[CMD] ERROR: Unknown command value");
        return;
    }

    // Switch-like structure using if-else (strings cannot be used in switch)
    if (key == "fan")
    {
        if (value == "on") {
            digitalWrite(FAN_PIN, HIGH);            
            Serial.println("[FAN] ON");
        }
        else { // value == "off"
            digitalWrite(FAN_PIN, LOW);            
            Serial.println("[FAN] OFF");
        }
    }
    else if (key == "heater")
    {
        if (value == "on") {
            Serial.println("[HEATER] ON");
            digitalWrite(HEATER_PIN, HIGH);  // if you add heater pin later
        }
        else { // value == "off"
            Serial.println("[HEATER] OFF");
            digitalWrite(HEATER_PIN, LOW);
        }
    }
    else
    {
        Serial.println("[CMD] ERROR: Unknown key");
    }
});
}

/** 
 * Main loop function
 * Handles telemetry collection and transmission at specified intervals, and resets the watchdog timer to prevent system resets. It collects data from the DHT22 sensor, GPS module, and RTLS system at their respective intervals and sends the telemetry data to the MQTT broker. The loop also includes a small delay to prevent it from running too fast, which can help with power consumption and allow other tasks to run smoothly. The watchdog timer is reset at the beginning of each loop iteration to ensure that the system does not reset due to inactivity. 
*/
void loop() {
    // Handle MQTT communication and reset watchdog timer to prevent system reset due to inactivity. This ensures that the device remains responsive and can recover from potential issues such as infinite loops or deadlocks. The MQTT loop function processes incoming messages and maintains the connection to the MQTT broker, while the watchdog reset ensures that the system can recover if it becomes unresponsive for any reason. This combination allows for robust operation of the IoT device while maintaining communication with the
    MQTT broker and ensuring system stability.  
    comms.loop();
    esp_task_wdt_reset();
    // Update sensors and send telemetry at specified intervals     
    GPS_update();

    // Get the current time in milliseconds since the device started. This is used to determine when to collect and send telemetry data based on the defined intervals for each sensor. By comparing the current time with the last recorded time for each sensor, the loop can decide when to read new data and send it to the MQTT broker. This approach allows for efficient scheduling of sensor readings and data transmission without blocking the main loop, ensuring that the device remains responsive to incoming MQTT messages and other tasks.  
    // Get current time for interval checks 
    unsigned long now = millis();
     
    // Check if it's time to read from the DHT22 sensor and send telemetry data. 
    // If the current time minus the last recorded time for the DHT22 sensor exceeds
    //  the defined interval (DHT_INTERVAL), it creates a new JSON document, populates
    //  it with the sensor data, and sends it to the MQTT broker. After sending the telemetry,
    //  it updates the last recorded time for the DHT22 sensor to the current time. 
    // This ensures that the DHT22 sensor data is collected and transmitted at regular 
    // intervals without blocking the main loop. The same logic is applied for GPS and RTLS 
    // data collection and transmission  in their respective intervals.   
    if (now - lastDHT > DHT_INTERVAL) {
        StaticJsonDocument<256> doc;
        doc["type"] = "dht";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;
        Sensors_read(doc);
        comms.sendTelemetry(doc);
        lastDHT = now;
    }

    if (now - lastGPS > GPS_INTERVAL) {
        StaticJsonDocument<256> doc;
        doc["type"] = "gps";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;
        GPS_fill(doc);
        comms.sendTelemetry(doc);
        lastGPS = now;
    }

    if (now - lastRTLS > RTLS_INTERVAL) {
        RTLS_update();
        StaticJsonDocument<256> doc;
        doc["type"] = "rtls";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;
        RTLS_fill(doc);
        comms.sendTelemetry(doc);
        lastRTLS = now;
    }
   
    // Small delay to prevent the loop from running too fast, which can help with power consumption and allow other tasks to run smoothly. This also gives time for the MQTT loop to process incoming messages and maintain the connection to the MQTT broker without overwhelming the system. The delay can be adjusted based on the specific requirements of the application and the desired responsiveness of the device. In this case, a 5 millisecond delay is used as a balance between responsiveness and allowing other tasks to execute effectively.   
    
    delay(5);
}
