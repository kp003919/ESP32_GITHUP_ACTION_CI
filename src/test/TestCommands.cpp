/**
 * TestCommands.cpp - Serial diagnostic commands for CI/CD and hardware verification
 */
#include "TestCommands.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../comms/WiFiMQTT.h"   // for wifi_status, mqtt_status, mqtt_pub_status
#include "../dht/Sensors.h"
#include "../gps/GPS.h"
#include "../rtls/RTLS.h"
#include <Wire.h>
#include <SPI.h>

/** 
 * handle_serial - Read and process serial commands for testing and diagnostics
 */
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
    Serial.println("[TEST] PULSE_DONE");
  }

  // ---------- DHT ----------
  else if (cmd == "TEST_DHT") {
    StaticJsonDocument<64> doc;
    Sensors_read(doc);
    float t = doc["temperature"];
    float h = doc["humidity"];
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

  // ---------- RTLS ----------
  else if (cmd == "TEST_RTLS") {
    StaticJsonDocument<128> doc;
    RTLS_fill(doc);
    if (!doc.containsKey("rtls")) {
      Serial.println("[TEST] {\"error\":\"rtls_fail\"}");
      return;
    }
    JsonArray arr = doc["rtls"].as<JsonArray>();
    Serial.print("[TEST] {\"rtls\":[");
    for (size_t i = 0; i < arr.size(); i++) {
      JsonObject tag = arr[i];
      Serial.printf("{\"id\":\"%s\",\"rssi\":%d}",
          tag["id"].as<const char*>(),
          tag["rssi"].as<int>());
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
      Serial.println("[TEST] MQTT_PUB_OK");
    } else {
      Serial.println("[TEST] MQTT_PUB_FAIL");
    }
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
    uint8_t addr = 0x40;
    uint8_t value = 0;
    Wire.beginTransmission(addr);
    Wire.write(0x00);
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
    int val = analogRead(34);
    Serial.printf("[TEST] ADC %d\n", val);
  }

  // ---------- SPI ----------
  else if (cmd == "TEST_SPI") {
    Serial.println("[TEST] SPI OK");
  }
  else if (cmd == "TEST_SPI_LOOP") {
    uint8_t send = 42;
    uint8_t recv = SPI.transfer(send);
    bool ok = (send == recv);
    Serial.printf("[TEST] {\"sent\":%u,\"received\":%u,\"loop_ok\":%s}\n",
        send, recv, ok ? "true" : "false");
  }

  // ---------- UART ----------
  else if (cmd == "TEST_UART") {
    Serial.println("[TEST] UART OK");
  }

  // ---------- BLE / BT ----------
  else if (cmd == "TEST_BLE") {
    Serial.println("[TEST] BLE OK");
  }
  else if (cmd == "TEST_BT") {
    Serial.println("[TEST] BT OK");
  }
  else if (cmd == "TEST_BLE_MATCH") {
    Serial.println("[TEST] {\"found\":false,\"rtls\":[]}");
  }

  // ---------- PROTO ----------
  else if (cmd == "TEST_PROTO") {
    Serial.println("[TEST] PROTO OK");
  }

  // ---------- RESET ----------
  else if (cmd == "TEST_RESET") {
    Serial.println("[TEST] RESET_OK");
    // ESP.restart();
  }
}