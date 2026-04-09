
/**
 * RTLS.cpp - Real-Time Location System (RTLS) implementation for ESP32     
 * This file implements a simple RTLS system using BLE scanning on the ESP32. It defines a BeaconInfo structure to store information about detected beacons, including a unique ID, smoothed RSSI value, battery level, type, and last seen timestamp. The system uses a BLE callback to process advertised devices, extract relevant information from their manufacturer data, and maintain a list of active beacons with RSSI smoothing and expiration logic. The implementation supports both custom beacons with specific manufacturer data formats and generic devices by generating synthetic IDs based on their MAC addresses. The SHOW_ALL_BEACONS flag allows for filtering of valid beacons or displaying all detected devices.   
 * Note: This implementation is a starting point for an RTLS system and can be expanded with additional features such as distance estimation, trilateration, or integration with other sensors for improved accuracy. The current implementation focuses on BLE scanning and basic beacon management on the ESP32 platform. 
 * Author: Muhsin Atto
 * Date: 2026-04-01 
 * License: MIT License 
 * For more information on BLE scanning and ESP32 development, refer to the ESP-IDF documentation and the Arduino BLE library documentation.    
 * Details:
 * - Uses the Arduino BLE library to perform BLE scanning and process advertised devices.   
 *  - The BeaconInfo structure includes fields for a unique ID, smoothed RSSI value, battery level, type, last seen timestamp, and a flag to indicate if an RSSI value has been set.
 * - The RTLSCallback class implements the BLEAdvertisedDeviceCallbacks interface to handle detected BLE    
 * advertised devices. It extracts manufacturer data, parses it according to known formats (custom ESP32/Raspberry Pi beacons, iBeacon, Eddystone), and updates the list of beacons with RSSI smoothing and expiration logic.
 * - The hashMac function generates a 32-bit hash from a MAC address string using the
 *  djb2 algorithm, which is used to create synthetic IDs for devices that do not have valid manufacturer data when SHOW_ALL_BEACONS is enabled.
 * - The parseManufacturer function checks the manufacturer data of a BLE device against known formats and extracts
 *  the ID, battery level, and type. If the data does not match known formats, it generates a synthetic ID based on the MAC address if SHOW_ALL_BEACONS is enabled.
 * - The updateRSSI function implements an exponential moving average to smooth the RSSI values for each beacon, providing a more stable signal strength measurement over time.
 * - The system maintains a list of up to 20 beacons, updating their information when           detected and removing them if they have not been seen for a specified timeout period (2 minutes).
 * - The implementation assumes that the BLE scanning is started elsewhere in the code (e.g., in the setup function) and that the RTLSCallback is registered with the BLE scan object to receive callbacks for detected devices.
 * - The SHOW_ALL_BEACONS flag can be toggled to control whether only valid beacons with recognized manufacturer data are displayed, or if all detected devices are included in the list with synthetic IDs. This allows for flexibility in how the RTLS system handles and displays detected BLE devices based on the application's requirements.  
 *      
 */

#include "RTLS.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>


#define BEACON_LIST_SIZE 20  // Maximum number of beacons to track  

// =========================
// Mode Flag (React Controls This)
// =========================
bool SHOW_ALL_BEACONS = false;   // false = only valid beacons, true = all devices


// =========================
// Beacon Structure
// =========================
struct BeaconInfo {
    uint32_t id;            // 32-bit unique ID
    float rssiSmoothed;     // Smoothed RSSI value
    uint8_t battery;       // Battery level (0-100)   
    uint8_t type;         // Beacon type (0 = unknown, 1 = iBeacon, 2 = Eddystone, etc.)   
    uint32_t lastSeen;    // Timestamp of last detection (millis())       
    bool hasRssi = false; // Flag to indicate if RSSI has been set at least once    
};

static BeaconInfo beacons[BEACON_LIST_SIZE];  // Array to store beacon information    
static uint8_t beaconCount = 0; // Current number of beacons in the list    

// BLE Scan object
static BLEScan* pBLEScan = nullptr;

// RSSI smoothing
static const float EMA_ALPHA = 0.30f;

// Expire beacons after 2 minutes
static const uint32_t BEACON_TIMEOUT = 120000; // 2 minutes

/**
 * Update RSSI with Exponential Moving Average      
 * This function updates the smoothed RSSI value for a beacon using an exponential moving average (EMA). If the beacon does not have a valid RSSI value yet (hasRssi is false), it initializes the smoothed RSSI with the new value. If it already has a valid RSSI, it applies the EMA formula to update the smoothed RSSI. This helps to stabilize the RSSI readings over time, reducing the impact of sudden fluctuations in signal strength. The EMA_ALPHA constant controls the smoothing factor, with higher values giving more weight to recent readings and lower values providing more smoothing over time.        
 * @param b Reference to the BeaconInfo structure to be updated
 * @param newRssi The new RSSI value to be incorporated into the smoothed RSS
 *  
 */
void updateRSSI(BeaconInfo& b, int newRssi) {
    if (!b.hasRssi) {
        b.rssiSmoothed = newRssi;
        b.hasRssi = true;
    } else {
        b.rssiSmoothed = EMA_ALPHA * newRssi + (1.0f - EMA_ALPHA) * b.rssiSmoothed;
    }
}

// =========================
// 32-bit MAC Hash (djb2)
// =========================
uint32_t hashMac(const std::string& mac) {
    uint32_t hash = 5381;
    for (char c : mac) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// =========================
// UNIVERSAL MANUFACTURER PARSER
// =========================
bool parseManufacturer(BLEAdvertisedDevice& device,
                       uint32_t& id,
                       uint8_t& battery,
                       uint8_t& type)
{
    if (!device.haveManufacturerData())
        return false;

    std::string mfg = device.getManufacturerData();
    const uint8_t* raw = (const uint8_t*)mfg.data();
    size_t len = mfg.length();

    // -----------------------------------------
    // 1. Custom ESP32 / Raspberry Pi beacons
    // Format: FF FF [id] [battery] [type]
    // -----------------------------------------
    if (len >= 5 && raw[0] == 0xFF && raw[1] == 0xFF) {
        id      = raw[2];
        battery = raw[3];
        type    = raw[4];
        return true;
    }

    // -----------------------------------------
    // 2. iBeacon (Apple)
    // -----------------------------------------
    if (len >= 4 && raw[0] == 0x4C && raw[1] == 0x00 &&
        raw[2] == 0x02 && raw[3] == 0x15)
    {
        type = 1;
        id = raw[4] ^ raw[5] ^ raw[6] ^ raw[7]; // stable hash
        battery = 0;
        return true;
    }

    // -----------------------------------------
    // 3. Eddystone (UUID FEAA)
    // -----------------------------------------
    if (device.haveServiceUUID() &&
        device.getServiceUUID().equals(BLEUUID((uint16_t)0xFEAA)))
    {
        type = 2;
        id = raw[0] ^ raw[1] ^ raw[2];
        battery = 0;
        return true;
    }

    // Unknown manufacturer data → still valid
    id = hashMac(mfg);
    battery = 0;
    type = 0;
    return true;
}

/**
 * BLE Advertised Device Callback
 * This class implements the BLEAdvertisedDeviceCallbacks interface to handle detected BLE advertised devices during scanning           
 *  The onResult method is called for each detected device and processes the manufacturer data to extract relevant information such as a unique ID, battery level, and type. It updates the list of beacons with RSSI smoothing and expiration logic, allowing for a stable and up-to-date representation of nearby BLE devices. The callback supports both custom beacons with specific manufacturer data formats and generic devices by generating synthetic IDs based on their MAC addresses when SHOW_ALL_BEACONS is enabled. This implementation provides a flexible way to manage detected BLE devices in an RTLS system on the ESP32 platform.    
 * Note: The BLE scan object must be configured to use this callback for it to function correctly       
 *      
 */
class RTLSCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {

        int rssi = device.getRSSI();
        uint32_t id = 0;
        uint8_t battery = 0;
        uint8_t type = 0;

        // Try to parse manufacturer data
        bool valid = parseManufacturer(device, id, battery, type);

        // If invalid and we are in filtered mode → ignore
        if (!valid && !SHOW_ALL_BEACONS) {
            return;
        }

        // If invalid but SHOW_ALL_BEACONS = true → generate synthetic ID
        if (!valid && SHOW_ALL_BEACONS) {
            std::string mac = device.getAddress().toString();
            id = hashMac(mac);
            battery = 0;
            type = 0;
        }

        Serial.printf("MAC %s → ID %u\n",
            device.getAddress().toString().c_str(), id);

        // -----------------------------
        // Update or add beacon
        // -----------------------------
        for (uint8_t i = 0; i < beaconCount; i++) {
            if (beacons[i].id == id) {
                updateRSSI(beacons[i], rssi);
                beacons[i].battery  = battery;
                beacons[i].type     = type;
                beacons[i].lastSeen = millis();
                return;
            }
        }

        // Add new beacon
        if (beaconCount < 20) {
            beacons[beaconCount].id       = id;
            beacons[beaconCount].battery  = battery;
            beacons[beaconCount].type     = type;
            beacons[beaconCount].lastSeen = millis();
            updateRSSI(beacons[beaconCount], rssi);
            beaconCount++;
        }
    }
};

/**
 * RTLS Initialization
 * This function initializes the BLE scanning functionality for the RTLS system. It sets up the BLE             
 * device, configures the scan parameters, and registers the RTLSCallback to handle detected BLE advertised devices. The scan is configured for active scanning with specific interval and window settings to optimize detection of nearby BLE devices. This initialization function should be called in the setup phase of the main program to prepare the RTLS system for operation.    
 * Note: The BLE library must be included and properly configured in the project for this function to   work correctly. Additionally, the RTLSCallback class must be defined and implemented to handle the processing of detected devices during scanning.      
 * 
 */
void RTLS_begin() {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new RTLSCallback());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(80);
    pBLEScan->setWindow(79);
}

/**
 * RTLS Update Loop
 * This function should be called regularly in the main loop to maintain the BLE scanning and manage the list of detected beacons. It checks if the BLE scan object is initialized and starts a new scan every 2 seconds. Additionally, it iterates through the list of detected beacons and removes any that have not been seen for a specified timeout period (2 minutes). This ensures that the list of beacons remains up-to-date and does not contain stale entries. The function relies on the RTLSCallback to handle the processing of detected devices during scanning and update the beacon information accordingly.    
 * Note: The BLE scanning must be properly initialized using RTLS_begin() before calling this update function. The RTLSCallback class must also be implemented to handle device detection and beacon management effectively.      
 * 
 */ 

void RTLS_update() {
    static uint32_t lastScan = 0;
    if (!pBLEScan) return;

    if (millis() - lastScan >= 2000) {
        lastScan = millis();
        pBLEScan->start(1, false);
        pBLEScan->clearResults();
    }

    // Remove expired beacons
    uint32_t now = millis();
    for (uint8_t i = 0; i < beaconCount; i++) {
        if (now - beacons[i].lastSeen > BEACON_TIMEOUT) {
            beacons[i] = beacons[beaconCount - 1];
            beaconCount--;
            i--;
        }
    }
}
/**
 * RTLS Fill JSON
 * This function fills a JSON document with the list of detected beacons and their information. It creates a nested array of beacons and populates each beacon object with its ID, RSSI, battery level, type, and last seen timestamp. This function is typically called to prepare the data for transmission or display in a web interface or API response.
 * Note: The JSON document must be properly initialized before calling this function.
 */

void RTLS_fill(JsonDocument& doc) {
    JsonArray arr = doc.createNestedArray("beacons");
    for (uint8_t i = 0; i < beaconCount; i++) {
        JsonObject b = arr.createNestedObject();
        b["id"]       = beacons[i].id;
        b["rssi"]     = (int)beacons[i].rssiSmoothed;
        b["battery"]  = beacons[i].battery;
        b["type"]     = beacons[i].type;
        b["lastSeen"] = beacons[i].lastSeen;
    }
}

/**
 * RTLS Set Mode
 * This function sets the mode of the RTLS system based on the provided string. It updates the SHOW_ALL_BEACONS flag to control whether only valid beacons with recognized manufacturer data are displayed, or if all detected devices are included in the list with synthetic IDs. The mode can be set to "all" to show all devices or "valid" to show only valid beacons. This function allows for dynamic control of the beacon filtering behavior based on user input or application requirements.    
 * Note: The function expects a valid mode string ("all" or "valid") and does not perform error handling for invalid input. It should be called before starting the BLE scanning to ensure that the desired mode is applied correctly.      
 * 
 */
void RTLS_setMode(const char* mode) {
    if (strcmp(mode, "all") == 0) {
        SHOW_ALL_BEACONS = true;
    } else if (strcmp(mode, "valid") == 0) {
        SHOW_ALL_BEACONS = false;
    }
}
