#pragma once
#include <Arduino.h>

struct DhtReading {
  float temperature;
  float humidity;
  bool  valid;
};

// Slow random walk that drifts back toward a nominal value.
inline DhtReading mockDhtRead() {
  static float t = 24.0f, h = 45.0f;

  t += 0.05f * (24.0f - t) + random(-10, 11) * 0.02f;   // about +-0.2 C per step
  h += 0.05f * (45.0f - h) + random(-10, 11) * 0.05f;   // about +-0.5 % per step

  if (t < 15.0f) t = 15.0f;  if (t > 35.0f) t = 35.0f;
  if (h < 20.0f) h = 20.0f;  if (h > 80.0f) h = 80.0f;

  DhtReading r = { t, h, true };

#ifdef MOCK_DHT_FAULTS
  if (random(0, 50) == 0) r.valid = false;               // about 2% failed reads
#endif
  return r;
}