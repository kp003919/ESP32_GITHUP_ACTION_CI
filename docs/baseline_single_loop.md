# Baseline: single-loop firmware (before FreeRTOS)

Setup: ESP32, plain TCP MQTT (no TLS), mock DHT22, BLE scan + GPS in one loop.
Test: Mosquitto stopped for about 90 s, then restarted. Log: baseline_single_loop.log

| Measurement | Result |
|---|---|
| Normal sensor interval (DHT) | 5.0 s |
| Longest sensor gap during outage | 6.99 s |
| Time blocked per failed MQTT reconnect | about 3.0 s |
| Reconnect backoff | 3, 6, 12, 24, 48 s |
| Telemetry messages dropped during outage | about 52 (not buffered) |

Conclusion: reconnect attempts block the sensing loop, and data from an outage is lost.
Target for the FreeRTOS version: sensor interval stays 5.0 s during an outage, and
telemetry is queued and sent after reconnect.