#!/usr/bin/env python3
"""
Hardware-in-the-Loop Test Runner
Flashes firmware → opens serial → sends TEST_* commands → PASS/FAIL
"""
import serial
import time
import os
import sys

PORT = os.getenv("SERIAL_PORT", "COM3")
BAUD = int(os.getenv("SERIAL_BAUD", 115200))
PER_TEST_TIMEOUT = 5

TESTS = [
    ("PING",              "[TEST] PONG"),
    ("TEST_UPTIME",       "UPTIME"),
    ("TEST_DHT",          "temperature"),
    ("TEST_GPS",          "lat"),
    ("TEST_WIFI",         "WIFI_OK"),
    ("TEST_MQTT",         "MQTT_OK"),
    ("TEST_MQTT_PUB",     "MQTT_PUB_OK"),
    ("TEST_FLASH",        "FLASH_SIZE"),
    ("TEST_RTC",          "RTC"),
    ("TEST_ADC",          "ADC"),
    ("TEST_SPI_LOOP",     "loop_ok"),
    ("TEST_I2C_SCAN",     "i2c"),
    ("TEST_RTLS",         "rtls"),
    ("TEST_BLE",          "BLE OK"),
    ("TEST_RESET",        "RESET_OK"),
]

def main():
    print(f"[HIL] Connecting to {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.1)
    except Exception as e:
        print(f"[HIL] ❌ Cannot open port: {e}")
        print("[HIL] Check: board connected? Port correct?")
        return 1

    time.sleep(2.5)
    ser.reset_input_buffer()
    print("[HIL] ✅ Connected. Starting tests...\n")

    passed = 0
    failed = 0
    for cmd, expected in TESTS:
        print(f"→ {cmd}")
        ser.write(f"{cmd}\n".encode())
        deadline = time.time() + PER_TEST_TIMEOUT
        found = False
        while time.time() < deadline:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"  │ {line}")
                    if expected in line:
                        found = True
                        break
            time.sleep(0.05)
        if found:
            print(f"  ✅ PASS\n")
            passed += 1
        else:
            print(f"  ❌ FAIL — expecting: '{expected}'\n")
            failed += 1

    ser.close()
    total = passed + failed
    print("=" * 50)
    print(f"RESULTS: {passed}/{total} passed")
    if failed == 0:
        print("✅ ALL TESTS PASSED")
        return 0
    else:
        print(f"❌ {failed} test(s) FAILED")
        return 1

if __name__ == "__main__":
    sys.exit(main())