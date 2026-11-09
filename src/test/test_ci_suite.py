import pytest
import json

# ---------------------------------------------------------
# 1) JSON parsing tests (4 tests)
# ---------------------------------------------------------
@pytest.mark.parametrize(
    "raw, expected",
    [
        ('{"temp": 25}', 25),
        ('{"temp": 0}', 0),
        ('{"temp": -5}', -5),
        ('{"temp": 100}', 100),
    ]
)
def test_json_parsing(raw, expected):
    data = json.loads(raw)
    assert data["temp"] == expected


# ---------------------------------------------------------
# 2) MQTT topic formatting tests (3 tests)
# ---------------------------------------------------------
def build_topic(device, sensor):
    return f"devices/{device}/sensors/{sensor}"

@pytest.mark.parametrize(
    "device, sensor, expected",
    [
        ("esp32", "temp", "devices/esp32/sensors/temp"),
        ("node1", "humidity", "devices/node1/sensors/humidity"),
        ("abc", "pressure", "devices/abc/sensors/pressure"),
    ]
)
def test_mqtt_topic(device, sensor, expected):
    assert build_topic(device, sensor) == expected


# ---------------------------------------------------------
# 3) Clamp utility tests (5 tests)
# ---------------------------------------------------------
def clamp(value, min_val, max_val):
    return max(min_val, min(value, max_val))

@pytest.mark.parametrize(
    "value, min_val, max_val, expected",
    [
        (5, 0, 10, 5),
        (-1, 0, 10, 0),
        (20, 0, 10, 10),
        (7, 5, 9, 7),
        (100, 0, 100, 100),
    ]
)
def test_clamp(value, min_val, max_val, expected):
    assert clamp(value, min_val, max_val) == expected


# ---------------------------------------------------------
# 4) Payload builder tests (2 tests)
# ---------------------------------------------------------
def build_payload(temp, humidity):
    return {
        "temp": temp,
        "humidity": humidity,
        "status": "ok" if temp < 50 else "alert"
    }

@pytest.mark.parametrize(
    "temp, humidity, expected_status",
    [
        (25, 60, "ok"),
        (55, 60, "alert"),
    ]
)
def test_payload_builder(temp, humidity, expected_status):
    payload = build_payload(temp, humidity)
    assert payload["status"] == expected_status
