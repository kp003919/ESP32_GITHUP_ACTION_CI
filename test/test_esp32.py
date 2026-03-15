
import serial 
import time
import json
import re
import pytest

PORT = "COM7"
BAUD = 115200


# --------------------------
# Pytest Fixtures 
# 
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
# HIL Test Cases
# --------------------------

# Each test sends a command to the ESP32 and checks the response and latency.   
# The tests cover basic connectivity (PING), uptime reporting, sensor readings (DHT, GPS),
# RTLS data, and pulse generation. 
# The latency thresholds are set based on expected response times for each command, allowing 
# for some margin. 
# The tests will fail if the ESP32 does not respond correctly or within the specified time limits,
# ensuring that the HIL setup is functioning as intended.
# Note: The actual commands and response formats must match what the ESP32 firmware is programmed to send.
# The tests assume that the ESP32 is running a firmware that listens for these specific commands and responds accordingly.  


@pytest.mark.hil
def test_ping(ser):
    resp, latency = send_test(ser, "PING", max_ms=150)
    assert "PONG" in resp
    assert latency < 150


@pytest.mark.hil
def test_uptime(ser):
    resp, latency = send_test(ser, "TEST_UPTIME", max_ms=200)
    parts = resp.split()
    assert parts[-1].isdigit()
    uptime = int(parts[-1])
    assert uptime > 0
    assert latency < 200


@pytest.mark.hil
def test_dht(ser):
    resp, latency = send_test(ser, "TEST_DHT", max_ms=500)
    dht = extract_json(resp)
    assert "temperature" in dht
    assert "humidity" in dht
    assert -20 <= dht["temperature"] <= 80
    assert 0 <= dht["humidity"] <= 100
    assert latency < 500

@pytest.mark.hil
def test_gps(ser):
    resp, latency = send_test(ser, "TEST_GPS", max_ms=300)
    gps = extract_json(resp)
    assert "lat" in gps and "lon" in gps
    assert -90 <= gps["lat"] <= 90
    assert -180 <= gps["lon"] <= 180
    assert latency < 300

@pytest.mark.hil
def test_rtls(ser):
    resp, latency = send_test(ser, "TEST_RTLS", max_ms=300)
    rtls = extract_json(resp)
    assert "rtls" in rtls
    assert isinstance(rtls["rtls"], list)

@pytest.mark.hil
def test_pulse(ser):
    resp, latency = send_test(ser, "TEST_PULSE", max_ms=200)
    assert "PULSE_DONE" in resp
    assert latency < 200

@pytest.mark.hil
def test_wifi(ser):
    resp, latency = send_test(ser, "TEST_WIFI", max_ms=500)
    assert "WIFI_OK" in resp or "WIFI_FAIL" in resp
    assert latency < 500

@pytest.mark.hil
def test_wifi_info(ser):
    resp, latency = send_test(ser, "TEST_WIFI_INFO", max_ms=500)
    assert resp.startswith("[TEST]")
    assert latency < 500

    # Extract JSON after "[TEST] "
    try:
        data = json.loads(resp.replace("[TEST] ", ""))
        assert "connected" in data
        # If connected, SSID + RSSI must exist
        if data["connected"]:
            assert "ssid" in data
            assert "rssi" in data
    except Exception:
        pytest.fail("Invalid JSON in TEST_WIFI_INFO")

@pytest.mark.hil
def test_mqtt(ser):
    resp, latency = send_test(ser, "TEST_MQTT", max_ms=500)
    assert "MQTT_OK" in resp or "MQTT_FAIL" in resp
    assert latency < 500

@pytest.mark.hil
def test_mqtt_publish(ser):
    resp, latency = send_test(ser, "TEST_MQTT_PUB", max_ms=500)
    assert "MQTT_PUB_OK" in resp or "MQTT_PUB_FAIL" in resp
    assert latency < 500

@pytest.mark.hil
def test_i2c_scan(ser):
    resp, latency = send_test(ser, "TEST_I2C_SCAN", max_ms=800)
    assert resp.startswith("[TEST]")
    assert latency < 800

    try:
        data = json.loads(resp.replace("[TEST] ", ""))
        assert "i2c" in data
        assert isinstance(data["i2c"], list)
    except Exception:
        pytest.fail("Invalid JSON in TEST_I2C_SCAN")

@pytest.mark.hil
def test_flash(ser):
    resp, latency = send_test(ser, "TEST_FLASH", max_ms=300)
    assert resp.startswith("[TEST] FLASH_SIZE")
    assert latency < 300


# To run these tests, ensure that the ESP32 is connected to the specified COM port and is running the appropriate firmware that responds to the test commands.
# Use the command `pytest test_esp32.py` to execute the tests.  
# The tests will output results indicating which tests passed or failed, along with any assertion errors for failed tests.  
