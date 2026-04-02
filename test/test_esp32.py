import serial
import time
import json
import re
import pytest
import pathlib


# --------------------------
# Serial Config
# --------------------------

PORT = "COM7"
BAUD = 115200
METRICS_FILE = pathlib.Path("metrics.json")


# --------------------------
# Metrics Recorder
# --------------------------

def record_metric(name, value):
    metrics = {}
    if METRICS_FILE.exists():
        metrics = json.loads(METRICS_FILE.read_text())
    metrics[name] = value
    METRICS_FILE.write_text(json.dumps(metrics, indent=2))


# --------------------------
# Pytest Fixtures
# --------------------------

@pytest.fixture(scope="session")
def ser():
    s = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(2)
    yield s
    s.close()


# --------------------------
# Helper Functions
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
# BASIC TESTS
# --------------------------

@pytest.mark.hil
def test_ping(ser):
    resp, latency = send_test(ser, "PING", max_ms=150)
    assert "PONG" in resp
    record_metric("ping_latency_ms", latency)


@pytest.mark.hil
def test_uptime(ser):
    resp, latency = send_test(ser, "TEST_UPTIME", max_ms=200)
    parts = resp.split()
    assert parts[-1].isdigit()
    record_metric("uptime_latency_ms", latency)


@pytest.mark.hil
def test_pulse(ser):
    resp, latency = send_test(ser, "TEST_PULSE", max_ms=200)
    assert "PULSE_DONE" in resp
    record_metric("pulse_latency_ms", latency)


# --------------------------
# DHT SENSOR
# --------------------------

@pytest.mark.hil
def test_dht(ser):
    resp, latency = send_test(ser, "TEST_DHT", max_ms=500)
    data = extract_json(resp)
    assert "temperature" in data
    assert "humidity" in data
    record_metric("dht_latency_ms", latency)


# --------------------------
# GPS
# --------------------------

@pytest.mark.hil
def test_gps(ser):
    resp, latency = send_test(ser, "TEST_GPS", max_ms=300)
    data = extract_json(resp)
    assert "lat" in data
    assert "lon" in data
    record_metric("gps_latency_ms", latency)


# --------------------------
# BLE (RTLS)
# --------------------------

@pytest.mark.hil
def test_rtls(ser):
    resp, latency = send_test(ser, "TEST_RTLS", max_ms=3000)
    data = extract_json(resp)
    assert "rtls" in data
    assert isinstance(data["rtls"], list)
    record_metric("rtls_latency_ms", latency)


# --------------------------
# WIFI
# --------------------------

@pytest.mark.hil
def test_wifi(ser):
    resp, latency = send_test(ser, "TEST_WIFI", max_ms=500)
    assert "WIFI_OK" in resp or "WIFI_FAIL" in resp
    record_metric("wifi_latency_ms", latency)


@pytest.mark.hil
def test_wifi_info(ser):
    resp, latency = send_test(ser, "TEST_WIFI_INFO", max_ms=500)
    data = extract_json(resp)
    assert "connected" in data
    record_metric("wifi_info_latency_ms", latency)


# --------------------------
# MQTT
# --------------------------

@pytest.mark.hil
def test_mqtt_status(ser):
    resp, latency = send_test(ser, "TEST_MQTT", max_ms=500)
    assert "MQTT_OK" in resp or "MQTT_FAIL" in resp
    record_metric("mqtt_status_latency_ms", latency)


@pytest.mark.hil
def test_mqtt_pub(ser):
    resp, latency = send_test(ser, "TEST_MQTT_PUB", max_ms=500)
    assert "MQTT_PUB_OK" in resp or "MQTT_PUB_FAIL" in resp
    record_metric("mqtt_pub_latency_ms", latency)


@pytest.mark.hil
def test_mqtt_e2e(ser):
    resp, latency = send_test(ser, "TEST_MQTT_E2E", max_ms=1500)
    data = extract_json(resp)
    assert "connected" in data
    assert "msg" in data
    record_metric("mqtt_e2e_latency_ms", latency)


# ---------- NEW: MQTT PAYLOAD ----------
@pytest.mark.hil
@pytest.mark.parametrize("payload", ["hi", "12345678", "long_message_test"])
def test_mqtt_payloads(ser, payload):
    resp, latency = send_test(ser, f"TEST_MQTT_PAYLOAD {payload}", max_ms=1500)
    data = extract_json(resp)

    assert "connected" in data
    assert data["msg"] == payload
    record_metric(f"mqtt_payload_{payload}_latency_ms", latency)


# --------------------------
# I2C
# --------------------------

@pytest.mark.hil
def test_i2c_scan(ser):
    resp, latency = send_test(ser, "TEST_I2C_SCAN", max_ms=800)
    data = extract_json(resp)
    assert "i2c" in data
    assert isinstance(data["i2c"], list)
    record_metric("i2c_scan_latency_ms", latency)


@pytest.mark.hil
def test_i2c_read(ser):
    resp, latency = send_test(ser, "TEST_I2C_READ", max_ms=500)
    data = extract_json(resp)
    assert "i2c_read" in data
    assert "value" in data["i2c_read"]
    record_metric("i2c_read_latency_ms", latency)


# --------------------------
# FLASH
# --------------------------

@pytest.mark.hil
def test_flash(ser):
    resp, latency = send_test(ser, "TEST_FLASH", max_ms=300)
    assert "FLASH_SIZE" in resp
    record_metric("flash_latency_ms", latency)


# --------------------------
# RTC
# --------------------------

@pytest.mark.hil
def test_rtc(ser):
    resp, latency = send_test(ser, "TEST_RTC", max_ms=300)
    assert "RTC" in resp
    record_metric("rtc_latency_ms", latency)


# --------------------------
# ADC
# --------------------------

@pytest.mark.hil
def test_adc(ser):
    resp, latency = send_test(ser, "TEST_ADC", max_ms=300)
    assert "ADC" in resp
    record_metric("adc_latency_ms", latency)


# --------------------------
# PWM
# --------------------------

@pytest.mark.hil
def test_pwm(ser):
    resp, latency = send_test(ser, "TEST_PWM", max_ms=600)
    assert "PWM_OK" in resp
    record_metric("pwm_latency_ms", latency)


# --------------------------
# SPI
# --------------------------

@pytest.mark.hil
def test_spi(ser):
    resp, latency = send_test(ser, "TEST_SPI", max_ms=300)
    assert "SPI" in resp
    record_metric("spi_latency_ms", latency)


@pytest.mark.hil
def test_spi_loop(ser):
    resp, latency = send_test(ser, "TEST_SPI_LOOP", max_ms=500)
    data = extract_json(resp)
    assert "sent" in data
    assert "received" in data
    assert "loop_ok" in data
    record_metric("spi_loop_latency_ms", latency)


# --------------------------
# UART / BLE / BT
# --------------------------

@pytest.mark.hil
def test_uart(ser):
    resp, latency = send_test(ser, "TEST_UART", max_ms=300)
    assert "UART" in resp
    record_metric("uart_latency_ms", latency)


@pytest.mark.hil
def test_ble_basic(ser):
    resp, latency = send_test(ser, "TEST_BLE", max_ms=300)
    assert "BLE" in resp
    record_metric("ble_basic_latency_ms", latency)


@pytest.mark.hil
def test_bt(ser):
    resp, latency = send_test(ser, "TEST_BT", max_ms=300)
    assert "BT" in resp
    record_metric("bt_latency_ms", latency)


@pytest.mark.hil
def test_ble_match(ser):
    resp, latency = send_test(ser, "TEST_BLE_MATCH", max_ms=3000)
    data = extract_json(resp)
    assert "found" in data
    assert "rtls" in data
    record_metric("ble_match_latency_ms", latency)


# --------------------------
# PROTO
# --------------------------

@pytest.mark.hil
def test_proto(ser):
    resp, latency = send_test(ser, "TEST_PROTO", max_ms=300)
    assert "PROTO" in resp
    record_metric("proto_latency_ms", latency)


# --------------------------
# RESET (run last)
# --------------------------

@pytest.mark.hil
def test_reset(ser):
    resp, latency = send_test(ser, "TEST_RESET", max_ms=300)
    assert "RESET_OK" in resp
    record_metric("reset_latency_ms", latency)
