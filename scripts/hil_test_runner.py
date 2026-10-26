#!/usr/bin/env python3
"""
Hardware-in-the-Loop Test Runner
Flashes firmware → opens serial port → sends TEST_* commands → PASS/FAIL
"""
import serial
import time
import os
import sys

# ========== CONFIGURATION ==========
PORT = os.getenv("SERIAL_PORT", "COM7")
BAUD = int(os.getenv("SERIAL_BAUD", 115200))
PER_TEST_TIMEOUT = 5

# ========== TEST DEFINITION ==========
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

# ========== RUNNER ==========
def main():
    print(f"[HIL] Connecting to {PORT} @ {BAUD} baud...")
    
    # Give board time to restart after flashing
    time.sleep(6)
    
    try:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUD,
            timeout=0.1,
            write_timeout=1
        )
    except Exception as e:
        print(f"[HIL] ERROR: Cannot open port: {e}")
        print(f"[HIL] Check: board connected? Port {PORT} correct? No other program using it?")
        return 1

    # Wait for boot messages
    time.sleep(2.5)
    ser.reset_input_buffer()
    print("[HIL] Connected. Starting tests...")
    print("-" * 50)

    passed = 0
    failed = 0

    for cmd, expected in TESTS:
        print(f"TEST: {cmd}")
        try:
            ser.write(f"{cmd}\n".encode('utf-8'))
        except Exception as e:
            print(f"  ERROR: Send failed - {e}")
            failed += 1
            continue

        deadline = time.time() + PER_TEST_TIMEOUT
        found = False
        while time.time() < deadline:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"  RECV: {line}")
                    if expected in line:
                        found = True
                        break
            time.sleep(0.05)

        if found:
            print("  PASS")
            passed += 1
        else:
            print(f"  FAIL - expecting: '{expected}'")
            failed += 1
        print("")

    ser.close()

    # ========== SUMMARY ==========
    print("=" * 50)
    total = passed + failed
    print(f"RESULTS: {passed}/{total} passed")

    if failed == 0:
        print("ALL TESTS PASSED")
        return 0
    else:
        print(f"{failed} test(s) FAILED")
        return 1

if __name__ == "__main__":
    sys.exit(main())