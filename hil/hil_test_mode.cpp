#include <Arduino.h>
#include <ArduinoJson.h>
#include "hil_test_mode.h"
#include "config.h"
#include <SPI.h>
#include <NimBLEDevice.h>
#include <Wire.h>
#include <WiFi.h>
#include <DHT.h>
#include <TinyGPSPlus.h>

DHT dht(DHTPIN, DHT22);
#define ADC_PIN 34
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

unsigned long bootTime = 0;

// ---------- MQTT (placeholder) ----------
bool mqttConnected() {
    return false;  // Replace with real MQTT client check
}

void startHilTestMode() {
    Serial.println("[HIL] Test mode active");
    bootTime = millis();

    pinMode(LED_BUILTIN, OUTPUT);

    dht.begin();
    GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);
    Wire.begin();
    BLEDevice::init("");

    while (true) {
        if (!Serial.available()) {
            delay(5);
            continue;
        }

        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        // ============================================================
        // BASIC TESTS
        // ============================================================

        if (cmd == "PING") {
            Serial.println("[TEST] PONG");
        }
        else if (cmd == "TEST_UPTIME") {
            unsigned long uptime = (millis() - bootTime) / 1000;
            Serial.printf("[TEST] UPTIME %lu\n", uptime);
        }
        else if (cmd == "TEST_PULSE") {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(50);
            digitalWrite(LED_BUILTIN, LOW);
            Serial.println("[TEST] PULSE_DONE");
        }

        // ============================================================
        // DHT SENSOR
        // ============================================================

        else if (cmd == "TEST_DHT") {
            StaticJsonDocument<128> doc;
            doc["temperature"] = dht.readTemperature();
            doc["humidity"]    = dht.readHumidity();
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // GPS
        // ============================================================

        else if (cmd == "TEST_GPS") {
            while (GPS_Serial.available()) gps.encode(GPS_Serial.read());
            StaticJsonDocument<128> doc;
            doc["lat"] = gps.location.isValid() ? gps.location.lat() : 0.0f;
            doc["lon"] = gps.location.isValid() ? gps.location.lng() : 0.0f;
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // BLE SCAN (RTLS)
        // ============================================================

        else if (cmd == "TEST_RTLS") {
            NimBLEScan* scan = BLEDevice::getScan();
            scan->stop();
            scan->setActiveScan(true);
            scan->setInterval(45);
            scan->setWindow(15);
            scan->start(3, false);

            NimBLEScanResults results = scan->getResults();
            StaticJsonDocument<512> doc;
            JsonArray arr = doc.createNestedArray("rtls");

            for (int i = 0; i < results.getCount(); i++) {
                const NimBLEAdvertisedDevice* dev = results.getDevice(i);
                JsonObject obj = arr.createNestedObject();
                obj["mac"]  = dev->getAddress().toString().c_str();
                obj["rssi"] = dev->getRSSI();
            }

            scan->clearResults();
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // WIFI
        // ============================================================

        else if (cmd == "TEST_WIFI") {
            Serial.println(WiFi.status() == WL_CONNECTED ? "[TEST] WIFI_OK" : "[TEST] WIFI_FAIL");
        }
        else if (cmd == "TEST_WIFI_INFO") {
            StaticJsonDocument<192> doc;
            bool connected = (WiFi.status() == WL_CONNECTED);
            doc["connected"] = connected;
            if (connected) {
                doc["ssid"] = WiFi.SSID();
                doc["rssi"] = WiFi.RSSI();
            }
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // MQTT
        // ============================================================

        else if (cmd == "TEST_MQTT") {
            Serial.println(mqttConnected() ? "[TEST] MQTT_OK" : "[TEST] MQTT_FAIL");
        }
        else if (cmd == "TEST_MQTT_PUB") {
            Serial.println(mqttConnected() ? "[TEST] MQTT_PUB_OK" : "[TEST] MQTT_PUB_FAIL");
        }
        else if (cmd == "TEST_MQTT_E2E") {
            StaticJsonDocument<128> doc;
            doc["connected"] = mqttConnected();
            doc["msg"] = "hello";
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ---------- NEW: MQTT PAYLOAD ECHO ----------
        else if (cmd.startsWith("TEST_MQTT_PAYLOAD")) {
            String payload = cmd.substring(strlen("TEST_MQTT_PAYLOAD"));
            payload.trim();

            StaticJsonDocument<128> doc;
            doc["connected"] = mqttConnected();
            doc["msg"] = payload;

            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // I2C
        // ============================================================

        else if (cmd == "TEST_I2C_SCAN") {
            StaticJsonDocument<256> doc;
            JsonArray arr = doc.createNestedArray("i2c");

            for (uint8_t addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) arr.add(addr);
            }

            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        else if (cmd == "TEST_I2C_READ") {
            const uint8_t addr = 0x68;
            const uint8_t reg  = 0x75;

            Wire.beginTransmission(addr);
            Wire.write(reg);
            Wire.endTransmission(false);

            Wire.requestFrom(addr, (uint8_t)1);
            uint8_t value = Wire.available() ? Wire.read() : 0xFF;

            StaticJsonDocument<128> doc;
            JsonObject obj = doc.createNestedObject("i2c_read");
            obj["reg"] = reg;
            obj["value"] = value;

            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // FLASH
        // ============================================================

        else if (cmd == "TEST_FLASH") {
            Serial.printf("[TEST] FLASH_SIZE %lu\n", (unsigned long)ESP.getFlashChipSize());
        }

        // ============================================================
        // PROTO
        // ============================================================

        else if (cmd == "TEST_PROTO") {
            Serial.println("[TEST] PROTO");
        }

        // ============================================================
        // RESET / SLEEP
        // ============================================================

        else if (cmd == "TEST_RESET") {
            Serial.println("[TEST] RESET_OK");
            delay(50);
            ESP.restart();
        }
        else if (cmd == "TEST_DEEP_SLEEP") {
            Serial.println("[TEST] DEEP_SLEEP_OK");
            delay(50);
            esp_deep_sleep_start();
        }

        // ============================================================
        // RTC
        // ============================================================

        else if (cmd == "TEST_RTC") {
            Serial.printf("[TEST] RTC %lu\n", millis());
        }

        // ============================================================
        // ADC
        // ============================================================

        else if (cmd == "TEST_ADC") {
            int value = analogRead(ADC_PIN);
            Serial.printf("[TEST] ADC %d\n", value);
        }

        // ============================================================
        // PWM
        // ============================================================

        else if (cmd == "TEST_PWM") {
            ledcAttachPin(LED_BUILTIN, 0);
            ledcSetup(0, 5000, 8);
            ledcWrite(0, 128);
            delay(50);
            ledcWrite(0, 0);
            Serial.println("[TEST] PWM_OK");
        }

        // ============================================================
        // SPI
        // ============================================================

        else if (cmd == "TEST_SPI") {
            Serial.println("[TEST] SPI");
        }

        else if (cmd == "TEST_SPI_LOOP") {
            SPI.begin();
            const uint8_t testByte = 0xA5;

            SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
            uint8_t received = SPI.transfer(testByte);
            SPI.endTransaction();

            StaticJsonDocument<128> doc;
            doc["sent"] = testByte;
            doc["received"] = received;
            doc["loop_ok"] = (received == testByte);

            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // UART / BLE / BT
        // ============================================================

        else if (cmd == "TEST_I2C") Serial.println("[TEST] I2C");
        else if (cmd == "TEST_UART") Serial.println("[TEST] UART");
        else if (cmd == "TEST_BLE") Serial.println("[TEST] BLE");
        else if (cmd == "TEST_BT")  Serial.println("[TEST] BT");

        else if (cmd == "TEST_BLE_MATCH") {
            const char* TARGET_MAC = "AA:BB:CC:DD:EE:FF";

            NimBLEScan* scan = BLEDevice::getScan();
            scan->stop();
            scan->setActiveScan(true);
            scan->start(3, false);

            NimBLEScanResults results = scan->getResults();
            StaticJsonDocument<256> doc;
            JsonArray arr = doc.createNestedArray("rtls");

            bool found = false;

            for (int i = 0; i < results.getCount(); i++) {
                const NimBLEAdvertisedDevice* dev = results.getDevice(i);
                String mac = dev->getAddress().toString().c_str();

                JsonObject obj = arr.createNestedObject();
                obj["mac"] = mac;
                obj["rssi"] = dev->getRSSI();

                if (mac.equalsIgnoreCase(TARGET_MAC)) found = true;
            }

            doc["found"] = found;

            scan->clearResults();
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }

        // ============================================================
        // UNKNOWN
        // ============================================================

        else {
            Serial.println("[TEST] UNKNOWN_CMD");
        }
    }
}
