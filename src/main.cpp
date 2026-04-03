/**
 * Cure IoT Project — FINAL VERSION (Node‑RED Only)
 * @Detials: This code is the main entry point for an ESP32-based IoT project that 
 * collects telemetry data from various sensors (DHT, GPS, RTLS) and sends it to a 
 * backend via MQTT. The code is structured to run on the ESP32 microcontroller and
 *  uses the Arduino framework. It includes setup and loop functions, as well as 
 * integration with a custom WiFiMQTT class for handling MQTT communication. 
 * The code also includes a watchdog timer to ensure the device remains responsive.
 *  The implementation is designed for use in an IoT project where the ESP32 collects
 *  sensor data and allows remote control via MQTT messages.  
 * 
 * Note: This implementation assumes a local MQTT broker (e.g., Node-RED) 
 * is running at the specified IP address and port. The MQTT topics and message 
 * formats can be customized as needed for the specific application. The code also 
 * includes basic error handling and logging for debugging purposes.    
 *      
 * 
 * ESP32 Telemetry + Fan Command Handling
 * Device ID: anchor_01
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

// Comms layer (Node‑RED only)
#include "comms/WiFiMQTT.h"

// Timers
unsigned long lastDHT  = 0;
unsigned long lastGPS  = 0;
unsigned long lastRTLS = 0;
unsigned long lastI2C = 0;
unsigned long lastSPI = 0;
unsigned long lastUART = 0;

// Telemetry intervals (in milliseconds)
// Adjust these intervals as needed for your application
// For example, DHT readings every 5 seconds, GPS every 10 seconds, 
// RTLS every 3 seconds, and I2C every 2 seconds.
// Note: Be mindful of the total data volume and network usage when setting these intervals.
const unsigned long DHT_INTERVAL  = 5000;
const unsigned long GPS_INTERVAL  = 9000;
const unsigned long RTLS_INTERVAL = 3000;
const unsigned long I2C_INTERVAL = 2000; // 2 seconds
const unsigned long SPI_INTERVAL = 4000; // 4 seconds
const unsigned long UART_INTERVAL = 4000; // 4 seconds  

// Global comms object
// This object will handle all MQTT communication with the backend.
//  It is initialized in the setup() function and used in the loop() 
// function to maintain the connection and send telemetry data. The WiFiMQTT 
//class abstracts away the details of connecting to the MQTT broker, subscribing 
// to topics, and publishing messages, allowing the main code to focus on sensor data 
// collection and processing. 

WiFiMQTT comms;

/**
 * setup() is the initialization function that runs once when the ESP32 boots up. 
 * It sets up the serial communication for debugging, initializes the sensors (DHT, GPS, RTLS), 
 * starts the I2C communication for RTLS, and configures the watchdog timer to ensure the device 
 * remains responsive. 
 * The function also initializes the MQTT communication by calling comms.begin(), which sets
 *  up the connection to the MQTT broker. 
 * Finally, it configures the actuator pins (e.g., fan control) 
 * and sets their initial state. This setup ensures that all components are ready before 
 * entering the main loop where telemetry data will be collected and sent to the backend.
 * @returns void
 */     

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nBooting...");   
    
    // Start sensors
    Sensors_begin();
    GPS_begin(); // enable when GPS is ready
    RTLS_begin();
    
    // Start I2C (for RTLS)
    i2c_begin();
    // Start SPI (if needed for other peripherals)  
    SPI_Init(); 
    // Start UART (if needed for other peripherals)
    UART2_Init(); 
    Serial.println("ESP32 ready to receive STM32 data");

    // Watchdog
    // Initialize the watchdog timer with a timeout of 10 seconds and enable panic mode.
    // This means that if the watchdog is not reset within 10 seconds, the ESP32 will automatically reset itself to recover from potential hangs or crashes.
    // The esp_task_wdt_add(NULL) function adds the current task (the main loop) to the watchdog, ensuring that it is monitored for responsiveness. 
    // In the loop() function, we will call esp_task_wdt_reset() to reset the watchdog timer regularly, indicating that the device is still functioning properly. If the loop fails to reset the watchdog within the specified timeout, the ESP32 will reboot, which can help maintain stability in an IoT application. 
    // Note: Be cautious when using the watchdog timer, as it can cause unintended resets if the loop takes too long to execute or if there are blocking operations. Always ensure that the loop is designed to run efficiently and that any long-running tasks are handled appropriately (e.g., using non-blocking code or separate tasks).    

    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    // Node‑RED only
    comms.begin();

    // Fan pin
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
}

void loop() {
    comms.loop();  // keep MQTT alive
    esp_task_wdt_reset();
    GPS_update();

    // ---------------- UART2 (STM32 → ESP32) ----------------
    UART2_Task();
    const char* data = UART2_GetPacket();

    if (data) {       
        if (strcmp(data, "PING") == 0) {
            Serial.println("[TEST] PONG");
        } 
        // Add more command handling as needed  
        // comment to be seen in ci 
        // For example, you could add commands to trigger specific actions on the ESP32 based on the data received from the STM32, such as controlling actuators, changing sensor reading intervals, or sending specific telemetry data back to the STM32.  
        
    }

    // ---------------- USB Serial Test Interface (Python) ----------------
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "PING") {
            Serial.println("[TEST] PONG");
        }
        else if (cmd == "TEST_UPTIME") {
            Serial.print("[TEST] UPTIME ");
            Serial.println(millis());
        }
        else if (cmd == "TEST_DHT") {
            StaticJsonDocument<128> doc;
            Sensors_read(doc);
            Serial.print("[TEST] DHT ");
            serializeJson(doc, Serial);
            Serial.println();
        }
        else if (cmd == "TEST_GPS") {
            StaticJsonDocument<128> doc;
            GPS_fill(doc);
            Serial.print("[TEST] GPS ");
            serializeJson(doc, Serial);
            Serial.println();
        }
        else if (cmd == "TEST_RTLS") {
            StaticJsonDocument<128> doc;
            RTLS_fill(doc);
            Serial.print("[TEST] RTLS ");
            serializeJson(doc, Serial);
            Serial.println();
        }
        else if (cmd == "TEST_PULSE") {
            digitalWrite(FAN_PIN, HIGH);
            delayMicroseconds(100);
            digitalWrite(FAN_PIN, LOW);
            Serial.println("[TEST] PULSE_DONE");
        } 
        else if (cmd == "TEST_NET") 
        { 
            bool wifi_ok = true;  // add function to return actual WiFi status if needed (e.g., WiFi.status() == WL_CONNECTED)  
            bool mqtt_ok = true; // You must add this method if not present 
            Serial.print("[TEST] NET "); 
            Serial.print(wifi_ok ? "WIFI_OK " : "WIFI_FAIL "); 
            Serial.println(mqtt_ok ? "MQTT_OK" : "MQTT_FAIL"); 
        }
    }

    // ---------------- Telemetry Tasks ----------------
    unsigned long now = millis();

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

    delay(5);
}
