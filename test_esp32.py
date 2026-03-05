import serial
import time
import json
import re
import sys

PORT = "COM7"
BAUD = 115200

# Open serial port
ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # Allow ESP32 to reboot after flashing


# --------------------------
# Assertion helpers
# --------------------------

def assert_true(cond, msg):
    if not cond:
        raise AssertionError(msg)


def extract_json(line):
    match = re.search(r"\{.*\}", line)
    if not match:
        raise AssertionError(f"Invalid JSON in response: {line}")
    return json.loads(match.group(0))


# --------------------------
# Command sender with timing
# --------------------------

def send_test(cmd, max_ms=300):
    ser.reset_input_buffer()
    start = time.time()
    ser.write((cmd + "\n").encode())

    deadline = start + (max_ms / 1000.0)
    while time.time() < deadline:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("[TEST]"):
            latency = (time.time() - start) * 1000
            return line, latency

    raise AssertionError(f"No response for command '{cmd}' within {max_ms} ms")


# --------------------------
# Test suite
# --------------------------
print(">>> USING UPDATED SCRIPT WITH TIMING <<<")

print("\nRunning PING...")
resp, latency = send_test("PING", max_ms=150)
print("Response:", resp, f"(latency {latency:.1f} ms)")
assert_true("PONG" in resp, "PING failed")
assert_true(latency < 150, "PING too slow")


print("\nRunning TEST_UPTIME...")
resp, latency = send_test("TEST_UPTIME", max_ms=200)
print("Response:", resp, f"(latency {latency:.1f} ms)")
parts = resp.split()
assert_true(parts[-1].isdigit(), "UPTIME missing numeric value")
uptime = int(parts[-1])
assert_true(uptime > 0, "UPTIME must be positive")
assert_true(latency < 200, "UPTIME response too slow")


print("\nRunning TEST_DHT...")
resp, latency = send_test("TEST_DHT", max_ms=500)
print("Response:", resp, f"(latency {latency:.1f} ms)")
dht = extract_json(resp)
assert_true("temperature" in dht, "DHT missing temperature")
assert_true("humidity" in dht, "DHT missing humidity")
assert_true(-20 <= dht["temperature"] <= 80, "Temperature out of range")
assert_true(0 <= dht["humidity"] <= 100, "Humidity out of range")
assert_true(latency < 500, "DHT read too slow")


print("\nRunning TEST_GPS...")
resp, latency = send_test("TEST_GPS", max_ms=300)
print("Response:", resp, f"(latency {latency:.1f} ms)")
gps = extract_json(resp)
assert_true("lat" in gps and "lon" in gps, "GPS missing coordinates")
assert_true(-90 <= gps["lat"] <= 90, "Invalid latitude")
assert_true(-180 <= gps["lon"] <= 180, "Invalid longitude")
assert_true(latency < 300, "GPS response too slow")


print("\nRunning TEST_RTLS...")
resp, latency = send_test("TEST_RTLS", max_ms=300)
print("Response:", resp, f"(latency {latency:.1f} ms)")
rtls = extract_json(resp)
assert_true("rtls" in rtls, "RTLS missing list")
assert_true(isinstance(rtls["rtls"], list), "RTLS must be a list")
assert_true(latency < 300, "RTLS response too slow")


print("\nRunning TEST_PULSE...")
resp, latency = send_test("TEST_PULSE", max_ms=200)
print("Response:", resp, f"(latency {latency:.1f} ms)")
assert_true("PULSE_DONE" in resp, "Pulse test did not complete")
assert_true(latency < 200, "Pulse test too slow")


print("\nAll tests passed successfully.")
