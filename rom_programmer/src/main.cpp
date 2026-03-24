#include <Arduino.h>
#include "flash.h"
#include "protocol.h"

void setup() {
    flash_init();
    Serial.begin(115200);
    // Send RESP_READY so the host knows the Arduino has finished booting
    // and is not stuck in a partial command from a previous session.
    Serial.write(RESP_READY);
}

void loop() {
    protocol_handle();
}