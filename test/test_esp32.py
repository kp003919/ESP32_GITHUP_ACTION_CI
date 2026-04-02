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
# HIL Test Cases (Existing)
# --------------------------

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


# --------------------------
# NEW TESTS FOR NEW COMMANDS
# --------------------------

@pytest.mark.hil
def test_mqtt_e2e(ser):
    """End-to-end MQTT test (stub until real MQTT integrated)."""
    resp, latency = send_test(ser, "TEST_MQTT_E2E", max_ms=1500)
    data = extract_json(resp)

    assert "connected" in data
    assert "msg" in data
    assert latency < 1500


@pytest.mark.hil
def test_ble_match(ser):
    """BLE scan + target MAC detection."""
    resp, latency = send_test(ser, "TEST_BLE_MATCH", max_ms=2000)
    data = extract_json(resp)

    assert "found" in data
    assert "rtls" in data
    assert isinstance(data["rtls"], list)
    assert latency < 2000


@pytest.mark.hil
def test_i2c_read(ser):
    """Functional I2C register read (e.g., MPU6050 WHO_AM_I)."""
    resp, latency = send_test(ser, "TEST_I2C_READ", max_ms=500)
    data = extract_json(resp)

    assert "i2c_read" in data
    assert "value" in data["i2c_read"]
    assert latency < 500


@pytest.mark.hil
def test_spi_loop(ser):
    """SPI MOSI->MISO loopback test."""
    resp, latency = send_test(ser, "TEST_SPI_LOOP", max_ms=300)
    data = extract_json(resp)

    assert "loop_ok" in data
    assert latency < 300
