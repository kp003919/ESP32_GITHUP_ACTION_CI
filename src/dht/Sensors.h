#pragma once

#include <ArduinoJson.h>
#include "mock_dht.h"
/**
 * Sensors Interface
 * This module provides an interface for initializing and reading from the DHT sensor.
 * It uses the DHT library to read temperature and humidity values and populates a JSON
 *  document with the readings. 
 * 
 */
void Sensors_begin();
/**
 * Read sensors and populate JSON document
 * This function reads the temperature and humidity from the DHT sensor and populates the provided JSON document with the readings.
 * If the sensor read fails, it populates the JSON document with error values (-999.0) for both temperature and humidity.
 * @param doc Reference to a JSON document where the sensor readings will be stored.
 */
void Sensors_read(JsonDocument& doc);
