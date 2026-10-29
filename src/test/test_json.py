def test_json_parsing():
    data = {"temp": 25, "humidity": 60}
    assert "temp" in data
    assert data["temp"] == 25
