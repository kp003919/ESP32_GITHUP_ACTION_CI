/**
 * Sensors.cpp - DHT Sensor Handling
 * This file contains functions to initialize and read data from the DHT sensor. 
 * It uses the DHT library to interface with the sensor and populate a JSON document
 * with the temperature and humidity readings. The Sensors_begin() function initializes 
 * the sensor, while the Sensors_read() function reads the sensor data and updates the 
 * provided JSON document with the latest values. If the sensor read fails, it populates
 *  the JSON document with error values (-999.0) for both temperature and humidity.
 * Author: Muhsin Atto 
 * Date: 2024-06-01
 * Details:
 * - Uses the DHT library to read temperature and humidity from the specified pin and sensor type   
 * - Handles read failures by checking for NaN values and populating the JSON document with error values if the read fails
 * - The JSON document is expected to have "temperature" and "humidity" keys where the sensor data will be stored
 * - The pull-up time for the DHT sensor can be configured in the DHT constructor (default is 55 microseconds)
 * - The sensor is initialized in the Sensors_begin() function, which should be called once during  setup, and the Sensors_read() function should be called periodically (e.g. in the main loop) to update the sensor data in the JSON document
 *  
 */


#include "Sensors.h"
#include "../config.h"
// DHT sensor library
#include <DHT.h>
#include "mock_dht.h"

static DHT dht(DHTPIN, DHTTYPE);
/** 
 * Initialize sensors   
 * This function initializes the DHT sensor by calling the begin() method on the DHT object.
 *  It also sets the pin modes for the LED and fan pins defined in the config.h file.
 *  The DHT sensor is configured with the specified pin and sensor type, and it is ready to
 *  be read after this function is called. This function should be called once during the 
 *  setup phase of the program to prepare the sensors for operation.
 */
void Sensors_begin() 
{
    #if !USE_MOCK_DHT
    dht.begin();
    #else
    randomSeed(42);                 // repeatable mock sequence (remove for varied data)
    Serial.println("Sensors: DHT22 mock mode enabled");
    #endif
}

/**
 * Read sensors and populate JSON document  
 * This function reads the temperature and humidity from the DHT sensor. 
 * It checks if the readings are valid (not NaN) and populates the provided 
 * JSON document with the temperature and humidity values. If the sensor read fails 
 * (i.e., if either reading is NaN), it prints an error message to the serial console 
 * and populates the JSON document with error values (-999.0) for both temperature and humidity.
 *  The JSON document is expected to have "temperature" and "humidity" keys where the sensor data 
 * will be stored. This function should be called periodically (e.g., in the main loop) to keep 
 * the sensor data updated in the JSON document.  
 */
#include "mock_dht.h"

void Sensors_read(JsonDocument& doc) {
    float t, h;
    bool ok;

#if USE_MOCK_DHT
    DhtReading r = mockDhtRead();       // no hardware access, returns instantly
    t = r.temperature;
    h = r.humidity;
    ok = r.valid;
#else
    t = dht.readTemperature();
    h = dht.readHumidity();
    ok = !(isnan(t) || isnan(h));
#endif

    if (!ok) {
        Serial.println("Sensors: failed to read DHT");
        t = -999.0f;                    // keeps your existing error convention
        h = -999.0f;
    }

    doc["temperature"] = roundf(t * 10.0f) / 10.0f;   // one decimal place
    doc["humidity"]    = roundf(h * 10.0f) / 10.0f;
    //doc["dht_mock"]    = USE_MOCK_DHT;                // so mock data is never mistaken for real
}
