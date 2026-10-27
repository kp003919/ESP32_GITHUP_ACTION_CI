#!/usr/bin/env python3
"""
Hardware-in-the-Loop Test Runner
"""
import serial
import time
import os
import sys

# Configuration
PORT = os.getenv("SERIAL_PORT", "COM7")
BAUD = int(os.getenv("SERIAL_BAUD", 115200))
PER_TEST_TIMEOUT = 10  # Longer for BLE

# Test list — matches ACTUAL board output
TESTS = [
    ("PING",              "PONG"),
    ("TEST_UPTIME",       "UPTIME"),
    ("TEST_DHT",          "temperature"),
    ("TEST_GPS",          "lat"),
    ("TEST_WIFI",         "WIFI_OK"),
    ("TEST_MQTT",         "Connected"),      # Connection succeeded    
    ("TEST_FLASH",        "FLASH_SIZE"),
    ("TEST_RTC",          "RTC"),
    ("TEST_ADC",          "ADC"),
    ("TEST_SPI_LOOP",     "loop_ok"),
    ("TEST_I2C_SCAN",     "i2c"),  
    
    ("TEST_RESET",        "RESET_OK"),
]

def main():
    print("[HIL] Connecting to " + PORT + " at " + str(BAUD) + " baud...")
    
    time.sleep(6)
    
    try:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUD,
            timeout=0.1,
            write_timeout=1
        )
    except Exception as e:
        print("[HIL] ERROR: Cannot open port")
        print("[HIL] DETAILS: " + str(e))
        return 1

    time.sleep(2)
    ser.reset_input_buffer()
    print("[HIL] Connected. Starting tests...")
    print("-" * 50)

    passed = 0
    failed = 0

    for cmd, expected in TESTS:
        print("TEST: " + cmd)
        try:
            ser.write((cmd + "\n").encode('utf-8'))
        except Exception as e:
            print("  FAIL: Send error - " + str(e))
            failed += 1
            continue

        deadline = time.time() + PER_TEST_TIMEOUT
        found = False
        while time.time() < deadline:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    if expected in line:
                        found = True
                        break
            time.sleep(0.05)

        if found:
            print("  PASS")
            passed += 1
        else:
            print("  FAIL - expected: " + expected)
            failed += 1
        print("")

    ser.close()

    print("=" * 50)
    total = passed + failed
    print("RESULTS: " + str(passed) + "/" + str(total) + " passed")

    if failed == 0:
        print("ALL TESTS PASSED")
        return 0
    else:
        print(str(failed) + " test(s) FAILED")
        return 1

if __name__ == "__main__":
    sys.exit(main())