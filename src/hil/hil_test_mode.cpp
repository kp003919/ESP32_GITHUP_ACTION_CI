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
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

unsigned long bootTime = 0;

// ---------- MQTT (placeholder) ----------
bool mqttConnected() {
    // Replace with your real MQTT client check:
    // return mqttClient.connected();
    return false;  // default until you integrate real MQTT
}


void startHilTestMode() {
    Serial.println("[HIL] Test mode active");
    bootTime = millis();

    pinMode(LED_BUILTIN, OUTPUT);

    dht.begin();
    GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);
    Wire.begin();

    while (true) {
        if (!Serial.available()) {
            delay(5);
            continue;
        }

        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

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
        else if (cmd == "TEST_DHT") {
            StaticJsonDocument<128> doc;
            doc["temperature"] = dht.readTemperature();
            doc["humidity"]    = dht.readHumidity();
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }
        else if (cmd == "TEST_GPS") {
            while (GPS_Serial.available()) gps.encode(GPS_Serial.read());
            StaticJsonDocument<128> doc;
            doc["lat"] = gps.location.isValid() ? gps.location.lat() : 0.0f;
            doc["lon"] = gps.location.isValid() ? gps.location.lng() : 0.0f;
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }
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
                obj["mac"] = dev->getAddress().toString().c_str();
                obj["rssi"] = dev->getRSSI();
            }

            scan->clearResults();
            Serial.print("[TEST] ");
            serializeJson(doc, Serial);
            Serial.println();
        }
        else if (cmd == "TEST_WIFI") {
            Serial.println(WiFi.status() == WL_CONNECTED ? "[TEST] WIFI_OK" : "[TEST] WIFI_FAIL");
        }
        else if (cmd == "TEST_MQTT") {
            Serial.println(mqttConnected() ? "[TEST] MQTT_OK" : "[TEST] MQTT_FAIL");
        }
        else {
            Serial.println("[TEST] UNKNOWN_CMD");
        }
    }
}
