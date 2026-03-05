import serial
import time
import json
import re
import pytest

PORT = "COM7"
BAUD = 115200


# --------------------------
# Pytest Fixtures
# --------------------------

@pytest.fixture(scope="session")
def ser():
    """Open serial port once per test session."""
    s = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(2)  # allow ESP32 reboot
    yield s
    s.close()


# --------------------------
# Helper functions
# --------------------------

def extract_json(line):
    match = re.search(r"\{.*\}", line)
    assert match, f"Invalid JSON in response: {line}"
    return json.loads(match.group(0))


def send_test(ser, cmd, max_ms=300):
    ser.reset_input_buffer()
    start = time.time()
    ser.write((cmd + "\n").encode())

    deadline = start + (max_ms / 1000.0)
    while time.time() < deadline:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("[TEST]"):
            latency = (time.time() - start) * 1000
            return line, latency

    pytest.fail(f"No response for command '{cmd}' within {max_ms} ms")
    

# --------------------------
# Test Cases
# --------------------------

def test_ping(ser):
    resp, latency = send_test(ser, "PING", max_ms=150)
    assert "PONG" in resp
    assert latency < 150


def test_uptime(ser):
    resp, latency = send_test(ser, "TEST_UPTIME", max_ms=200)
    parts = resp.split()
    assert parts[-1].isdigit()
    uptime = int(parts[-1])
    assert uptime > 0
    assert latency < 200


def test_dht(ser):
    resp, latency = send_test(ser, "TEST_DHT", max_ms=500)
    dht = extract_json(resp)
    assert "temperature" in dht
    assert "humidity" in dht
    assert -20 <= dht["temperature"] <= 80
    assert 0 <= dht["humidity"] <= 100
    assert latency < 500


def test_gps(ser):
    resp, latency = send_test(ser, "TEST_GPS", max_ms=300)
    gps = extract_json(resp)
    assert "lat" in gps and "lon" in gps
    assert -90 <= gps["lat"] <= 90
    assert -180 <= gps["lon"] <= 180
    assert latency < 300


def test_rtls(ser):
    resp, latency = send_test(ser, "TEST_RTLS", max_ms=300)
    rtls = extract_json(resp)
    assert "rtls" in rtls
    assert isinstance(rtls["rtls"], list)
    assert latency < 300


def test_pulse(ser):
    resp, latency = send_test(ser, "TEST_PULSE", max_ms=200)
    assert "PULSE_DONE" in resp
    assert latency < 200
