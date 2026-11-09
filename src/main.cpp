/**
 * Cure IoT Project - ESP32 Firmware
 * Main Application Entry Point
 * =================================
 * 
 * Overview:
 *   Modular IoT firmware for ESP32-S3 collecting sensor data and sending
 *   telemetry over WiFi/MQTT, plus receiving commands to control actuators.
 * 
 * Architecture:
 *   - Core sensor/telemetry logic runs on CPU0 in main loop (non-blocking)
 *   - MQTT/WiFi connection & reconnection runs on CPU1 as dedicated FreeRTOS task
 *   - Data passes safely between them via thread-safe FreeRTOS queue
 *   - Watchdog timer ensures automatic recovery from hangs/deadlocks
 * 
 * Modules:
 *   - dht/        Temperature & humidity sensor
 *   - gps/        GNSS location & time
 *   - rtls/       BLE real-time location tracking
 *   - comms/      WiFi + MQTT (separate FreeRTOS task)
 *   - tests/      Serial diagnostic & CI/CD test commands
 * 
 * Author: Muhsin Atto
 * Date: 2024-06-01
 * License: MIT License
 */

// ==============================================
// SYSTEM & FRAMEWORK HEADERS
// ==============================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>   // Watchdog timer

// ==============================================
// PROJECT CONFIGURATION & SECRETS
// ==============================================
#include "config.h"          // Pins, intervals, device settings
#include "secrets_new.h"     // WiFi/MQTT credentials (kept separate)

// ==============================================
// DRIVER LAYER - COMMUNICATION BUSES
// ==============================================
#include <SPI.h>
#include "spi_driver.h"
#include <Wire.h>            // I2C master
#include "I2C_Driver.h"
#include "uart_driver.h"

// ==============================================
// SENSOR & FUNCTION MODULES
// ==============================================
#include "dht/Sensors.h"     // Temp/humidity
#include "gps/GPS.h"         // GNSS module
#include "rtls/RTLS.h"       // BLE proximity tracking
#include "comms/WiFiMQTT.h"  // WiFi + MQTT comms
#include "TestCommands.h" // Serial test harness

// ==============================================
// TELEMETRY TIMING - INTERVALS
// All values in milliseconds
// ==============================================
const unsigned long DHT_INTERVAL   = 5000;  // Temp/humidity: every 5s
const unsigned long GPS_INTERVAL   = 5000;  // GPS fix: every 5s
const unsigned long RTLS_INTERVAL  = 5000;  // BLE scan: every 5s
const unsigned long I2C_INTERVAL    = 2000;  // Reserved - future use
const unsigned long SPI_INTERVAL    = 4000;  // Reserved - future use
const unsigned long UART_INTERVAL   = 4000;  // Reserved - future use

// ==============================================
// TELEMETRY TIMESTAMP TRACKERS
// Record millis() value when each sensor was LAST read
// ==============================================
unsigned long lastDHT  = 0;
unsigned long lastGPS  = 0;
unsigned long lastRTLS = 0;
unsigned long lastI2C  = 0;  // Reserved
unsigned long lastSPI  = 0;  // Reserved
unsigned long lastUART = 0;  // Reserved

// ==============================================
// GLOBAL OBJECTS
// Single communication instance shared across the firmware
// ==============================================
WiFiMQTT comms;

// ==============================================
// SETUP - RUNS ONCE AT POWER-ON / RESET
// ==============================================
void setup() {
    // Initialise serial console (115200 baud) for debugging & diagnostics
    Serial.begin(115200);
    delay(1000);  // Allow time for USB/serial to connect
    Serial.println(F("\nBooting Cure IoT Firmware..."));

    // ---------- Initialise Sensors ----------
    Sensors_begin();   // DHT22 temp/humidity
    GPS_begin();       // GNSS receiver
    RTLS_begin();      // BLE scanner for proximity

    // ---------- Watchdog Timer Setup ----------
    // If loop() does not feed the watchdog within 10 seconds,
    // the system assumes it is stuck and performs an automatic restart.
    // This recovers from deadlocks, hangs, or unresponsive network calls.
    esp_task_wdt_init(10, true);  // 10s timeout, panic on expiry
    esp_task_wdt_add(NULL);        // Register current task (main loop)

    // ---------- Communication Stack ----------
    // Connect WiFi, establish MQTT settings
    // begin() blocks until WiFi connects - see WiFiMQTT for retry options
    Serial.println(F("[INIT] Starting comms..."));
    comms.begin();

    // Launch MQTT processing as a SEPARATE FreeRTOS task on CPU1
    // Priority 1 = low (sensor timing takes precedence)
    // Stack 12288 bytes = 12KB (TLS connections need extra buffer space)
    // Core 1 = dedicated network core, leaves CPU0 free for sensors
    comms.startTask(1, 12288, 1);

    // ---------- Actuator Pins - Safe Default State ----------
    // Set outputs LOW at power-on to ensure devices are OFF until commanded
    pinMode(FAN_PIN, OUTPUT);
    pinMode(HEATER_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);     // Fan OFF
    digitalWrite(HEATER_PIN, LOW);  // Heater OFF
    Serial.println(F("[INIT] Actuators set to OFF"));

    // ---------- MQTT Command Callback Registration ----------
    // Incoming messages on subscribed topics arrive here.
    // Format accepted:  key=value  or  JSON {"key":"value"}
    comms.setCommandCallback([](const String& key, const String& value) {
        // Log every received command for traceability
        Serial.print(F("[CMD] Key   = ")); Serial.println(key);
        Serial.print(F("[CMD] Value = ")); Serial.println(value);

        // Strict input validation - only accept known states
        if (value != "on" && value != "off") {
            Serial.println(F("[CMD] ERROR: Unknown command value"));
            return;
        }

        // ---------- Fan Control ----------
        if (key == "fan") {
            if (value == "on") {
                digitalWrite(FAN_PIN, HIGH);
                Serial.println(F("[FAN] ON"));
            } else {
                digitalWrite(FAN_PIN, LOW);
                Serial.println(F("[FAN] OFF"));
            }
        }
        // ---------- Heater Control ----------
        else if (key == "heater") {
            if (value == "on") {
                digitalWrite(HEATER_PIN, HIGH);
                Serial.println(F("[HEATER] ON"));
            } else {
                digitalWrite(HEATER_PIN, LOW);
                Serial.println(F("[HEATER] OFF"));
            }
        }
        // ---------- Unknown Command ----------
        else {
            Serial.println(F("[CMD] ERROR: Unknown key"));
        }
    });

    Serial.println(F("[INIT] System ready\n"));
}

// ==============================================
// MAIN LOOP - RUNS CONTINUOUSLY AFTER SETUP
// Design principle: NON-BLOCKING - no function waits
// ==============================================
void loop() {
    // ---------- Step 1: Feed Watchdog FIRST ----------
    // Must be called every loop pass - prevents unwanted reset
    esp_task_wdt_reset();

    // ---------- Step 2: Serial Diagnostics ----------
    // Check for incoming TEST_* commands over USB serial
    // Returns immediately if nothing is available
    handle_serial();

    // ---------- Step 3: Get Reference Time ----------
    // millis() returns milliseconds since power-on (rolls over ~every 49 days)
    // Captured ONCE per loop so all checks use the same reference
    unsigned long now = millis();

    // ---------- Step 4: GPS Update (Every Pass) ----------
    // GPS modules stream NMEA sentences continuously - must parse frequently
    // Does not transmit, only reads incoming serial data
    GPS_update();

    // ---------- Step 5: DHT22 Telemetry (Every 5s) ----------
    if (now - lastDHT > DHT_INTERVAL) {
        Serial.println(F("[DHT] Reading..."));
        StaticJsonDocument<256> doc;  // Fixed-size buffer = predictable memory

        // Standardised fields for all telemetry messages
        doc["type"] = "dht";           // Identifies payload type
        doc["id"]   = DEVICE_ID;       // Unique device instance
        doc["ts"]   = now;             // Local timestamp (ms since boot)

        Sensors_read(doc);             // Populate with temp & humidity
        comms.sendTelemetry(doc);      // Enqueue - returns instantly, never blocks

        lastDHT = now;                 // Update timestamp for next interval
    }

    // ---------- Step 6: GPS Telemetry (Every 5s) ----------
    if (now - lastGPS > GPS_INTERVAL) {
        Serial.println(F("[GPS] Reading..."));
        StaticJsonDocument<256> doc;

        doc["type"] = "gps";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;

        GPS_fill(doc);                 // Add lat, lon, altitude, time
        comms.sendTelemetry(doc);

        lastGPS = now;
    }

    // ---------- Step 7: RTLS Telemetry (Every 5s) ----------
    if (now - lastRTLS > RTLS_INTERVAL) {
        Serial.println(F("[RTLS] Scanning..."));
        RTLS_update();                 // Perform BLE scan

        StaticJsonDocument<256> doc;
        doc["type"] = "rtls";
        doc["id"]   = DEVICE_ID;
        doc["ts"]   = now;

        RTLS_fill(doc);                // Add detected tags & signal strengths
        comms.sendTelemetry(doc);

        lastRTLS = now;
    }

    // ---------- Step 8: Yield to Other Tasks ----------
    // Short delay prevents the loop from consuming 100% CPU
    // Gives background tasks (WiFi/BLE stack, idle) time to run
    // 5ms = ~200Hz loop rate - plenty fast for all sensor needs
    delay(5);
}