
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
#include "comms/WiFiMQTT.h"

// Protocols (SPI,UART,I2C)
#include <SPI.h>
#include "spi_driver.h"
#include "uart_driver.h"
#include "I2C_Driver.h"
#include "Wire.h"

// Comms    
#include "comms/WiFiMQTT.h"
// sensors 
#include "dht/Sensors.h"
#include "gps/GPS.h"
#include "rtls/RTLS.h"  


// Actuator pins    

// Timing variables for telemetry intervals 
unsigned long lastDHT  = 0;
unsigned long lastGPS  = 0;
unsigned long lastRTLS = 0;
unsigned long lastI2C  = 0;
unsigned long lastSPI  = 0;
unsigned long lastUART = 0;

// Telemetry intervals (in milliseconds)    
const unsigned long DHT_INTERVAL  = 5000;
const unsigned long GPS_INTERVAL  = 5000;
const unsigned long RTLS_INTERVAL = 5000;
const unsigned long I2C_INTERVAL  = 2000;
const unsigned long SPI_INTERVAL  = 4000;
const unsigned long UART_INTERVAL = 4000;


// Forward declaration of serial handling function (if needed for debugging or command input)
void handle_serial();

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
    pinMode(HEATER_PIN, OUTPUT);   
     
    // TURN OFF FAN AND HEATER     
    digitalWrite(FAN_PIN, LOW);
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

    handle_serial(); // handle serial input for debugging or command input if needed
    // Handle MQTT communication and reset watchdog timer to prevent system reset due to inactivity. This ensures that the device remains responsive and can recover from potential issues such as infinite loops or deadlocks. The MQTT loop function processes incoming messages and maintains the connection to the MQTT broker, while the watchdog reset ensures that the system can recover if it becomes unresponsive for any reason. This combination allows for robust operation of the IoT device while maintaining communication with the
    // MQTT broker and ensuring system stability.  
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
        Serial.println("[DHT] Reading sensor data...");
        doc["type"] = "dht";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;
        Sensors_read(doc);
        comms.sendTelemetry(doc);
        lastDHT = now;
    }

    if (now - lastGPS > GPS_INTERVAL) {
        Serial.println("[GPS] Reading sensor data...");
        StaticJsonDocument<256> doc;
        doc["type"] = "gps";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;
        GPS_fill(doc);
        comms.sendTelemetry(doc);
        lastGPS = now;
    }

    if (now - lastRTLS > RTLS_INTERVAL) {
        Serial.println("[RTLS] Reading sensor data...");
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




// -------------------- COMMAND HANDLER --------------------
void handle_serial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // ---------- BASIC ----------
  if (cmd == "PING") {
    Serial.println("[TEST] PONG");
  }
  else if (cmd == "TEST_UPTIME") {
    Serial.print("[TEST] UPTIME ");
    Serial.println(millis());
  }
  else if (cmd == "TEST_PULSE") {
    // simple pulse on a pin if you want; here just acknowledge
    Serial.println("[TEST] PULSE_DONE");
  }

  // ---------- DHT ----------
  else if (cmd == "TEST_DHT") {
    StaticJsonDocument<64> doc;       
    Sensors_read(doc);        
    float t = doc["temperature"];
    float h = doc["humidity"]   ;

    if (isnan(t) || isnan(h)) {
      Serial.println("[TEST] {\"error\":\"sensor_fail\"}");
      return;
    }
    Serial.printf("[TEST] {\"temperature\":%.2f,\"humidity\":%.2f}\n", t, h);
  }

  // ---------- GPS ----------
  else if (cmd == "TEST_GPS") {   
    StaticJsonDocument<64> doc;
    GPS_fill(doc);
    if (!doc.containsKey("lat") || !doc.containsKey("lon")) {      
      Serial.println("[TEST] {\"error\":\"no_fix\"}");    
  } else {
      float lat = doc["lat"];
      float lon = doc["lon"];
      Serial.printf("[TEST] {\"lat\":%.6f,\"lon\":%.6f}\n", lat, lon);
    }
  } 

  // ---------- RTLS (placeholder: BLE/RTLS is complex) ----------
  else if (cmd == "TEST_RTLS") {
    // For now, you can treat this as "no tags found" or integrate BLE later
    StaticJsonDocument<64> doc;   
    RTLS_fill(doc);
    if (!doc.containsKey("rtls")) {
        Serial.println("[TEST] {\"error\":\"rtls_fail\"}");
        return;
    }  

    Serial.print("[TEST] {\"rtls\":[");
    JsonArray arr = doc["rtls"].as<JsonArray>();    
    for (size_t i = 0; i < arr.size(); i++) {
        JsonObject tag = arr[i].as<JsonObject>();
        Serial.printf("{\"id\":\"%s\",\"rssi\":%d}", tag["id"].as<const char*>(), tag["rssi"].as<int>());
        if (i < arr.size() - 1) Serial.print(",");
    }   
    Serial.println("]}");
  }

  // ---------- WIFI ----------
  else if (cmd == "TEST_WIFI") { 
    if (wifi_status == WIFI_STATUS_CONNECTED) {
      Serial.println("[TEST] WIFI_OK");
    } else {
      Serial.println("[TEST] WIFI_FAIL");
    }
  }
  else if (cmd == "TEST_WIFI_INFO") {
    bool connected = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[TEST] {\"connected\":%s}\n", connected ? "true" : "false");
  }

  // ---------- MQTT ----------
  else if (cmd == "TEST_MQTT") {    
    if (mqtt_status == MQTT_STATUS_CONNECTED) {
      Serial.println("[TEST] MQTT_OK");
    } else {
      Serial.println("[TEST] MQTT_FAIL");
    }
  }
  else if (cmd == "TEST_MQTT_PUB") {     
    if (mqtt_pub_status == MQTT_PUB_OK) {
      Serial.printf("[TEST] {\"mqtt_pub_status\":\"MQTT_PUB_OK\"}\n" );
    }else if (mqtt_pub_status == MQTT_PUB_FAIL) {
      Serial.printf("[TEST] {\"mqtt_pub_status\":\"MQTT_PUB_FAIL\"}\n" );
    } else {
      Serial.printf("[TEST] {\"mqtt_pub_status\":\"MQTT_PUB_UNKNOWN\"}\n" );
    }      
  }
  else if (cmd == "TEST_MQTT_E2E") {  
    Serial.printf("[TEST] TEST_MQTT_E2E OK \n" );  // need update later     
  }
  else if (cmd.startsWith("TEST_MQTT_PAYLOAD ")) {
    String payload = cmd.substring(String("TEST_MQTT_PAYLOAD ").length());   
    bool ok = false;
    PubSubClient mqttClient;
    if (mqtt_status == MQTT_STATUS_CONNECTED) {
      ok = mqttClient.publish(TOPIC_TEST_CMD , payload.c_str());
    }
    Serial.print("[TEST] {\"connected\":");
    if (mqtt_status == MQTT_STATUS_CONNECTED) {
      Serial.print("true");
    } else {
      Serial.print("false");
    }
    
    Serial.print(",\"msg\":\"");
    Serial.print(payload);
    Serial.println("\"}");
  }

  // ---------- I2C ----------
  else if (cmd == "TEST_I2C_SCAN") {
    Serial.print("[TEST] {\"i2c\":[");
    bool first = true;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (!first) Serial.print(",");
        Serial.print(addr);
        first = false;
      }
    }
    Serial.println("]}");
  }
  else if (cmd == "TEST_I2C_READ") {
    // Example: read one byte from a known device (adjust address/register)
    uint8_t addr = 0x40; // example
    uint8_t value = 0;
    Wire.beginTransmission(addr);
    Wire.write(0x00); // register
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(addr, (uint8_t)1) == 1) {
      value = Wire.read();
      Serial.printf("[TEST] {\"i2c_read\":{\"value\":%u}}\n", value);
    } else {
      Serial.println("[TEST] {\"i2c_read\":{\"error\":\"no_device\"}}");
    }
  }

  // ---------- FLASH ----------
  else if (cmd == "TEST_FLASH") {
    uint32_t size = ESP.getFlashChipSize();
    Serial.printf("[TEST] FLASH_SIZE %u\n", size);
  }

  // ---------- RTC ----------
  else if (cmd == "TEST_RTC") {
    time_t now = time(nullptr);
    Serial.printf("[TEST] RTC %ld\n", (long)now);
  }

  // ---------- ADC ----------
  else if (cmd == "TEST_ADC") {
    int val = analogRead(34); // adjust pin
    Serial.printf("[TEST] ADC %d\n", val);
  }

  // ---------- SPI ----------
  else if (cmd == "TEST_SPI") {
    Serial.println("[TEST] SPI OK");
  }
  else if (cmd == "TEST_SPI_LOOP") {
    uint8_t send = 42;
    uint8_t recv = 0;

    //digitalWrite(SPI_CS, LOW);
    recv = SPI.transfer(send);
    //digitalWrite(SPI_CS, HIGH);

    bool ok = (send == recv);
    Serial.printf("[TEST] {\"sent\":%u,\"received\":%u,\"loop_ok\":%s}\n",
                  send, recv, ok ? "true" : "false");
  }

  // ---------- UART ----------
  else if (cmd == "TEST_UART") {
    // Example: echo test on Serial2 if looped back
    Serial.println("[TEST] UART OK");
  }

  // ---------- BLE / BT (placeholder) ----------
  else if (cmd == "TEST_BLE") {
    // Real BLE scanning requires NimBLE or BLE library; placeholder:
    Serial.println("[TEST] BLE OK");
  }
  else if (cmd == "TEST_BT") {
    // Classic BT is more complex; placeholder:
    Serial.println("[TEST] BT OK");
  }
  else if (cmd == "TEST_BLE_MATCH") {
    // Placeholder: no RTLS tags found
    Serial.println("[TEST] {\"found\":false,\"rtls\":[]}");
  }

  // ---------- PROTO ----------
  else if (cmd == "TEST_PROTO") {
    Serial.println("[TEST] PROTO OK");
  }

  // ---------- RESET ----------
  else if (cmd == "TEST_RESET") {
    Serial.println("[TEST] RESET_OK");
    // ESP.restart(); // enable if you want real reset
  }
}

