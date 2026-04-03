#include <Arduino.h>
#include "hil_test_mode.h"

void setup() {
    Serial.begin(115200);
    delay(200);
    startHilTestMode();
}

void loop() {
    // not used
}
