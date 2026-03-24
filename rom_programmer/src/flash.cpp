#include "flash.h"
#include "pins.h"

// Data bus — ordered DQ0..DQ7.  Kept static; nothing else needs it.
static constexpr int DATA_PINS[8] = {A0, A1, A2, 5, 6, 7, 9, 10};

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

static void set_data_input() {
    for (int i = 0; i < 8; i++) pinMode(DATA_PINS[i], INPUT);
}

static void set_data_output() {
    for (int i = 0; i < 8; i++) pinMode(DATA_PINS[i], OUTPUT);
}

static void write_address(uint32_t address) {
    address &= 0x7FFFFUL;

    // Upper four bits driven directly — all four pins wired regardless of
    // variant; NC pins on smaller devices are ignored by the chip.
    digitalWrite(ADDR18, (address >> 18) & 1);   // flash A18 — 040 only
    digitalWrite(ADDR17, (address >> 17) & 1);   // flash A17 — 020A / 040
    digitalWrite(ADDR16, (address >> 16) & 1);   // flash A16
    digitalWrite(ADDR15, (address >> 15) & 1);   // flash A15

    // Lower 15 bits through two cascaded 74HC595s.
    // First byte clocks into the downstream register (drives A8–A14).
    // Second byte clocks into the upstream register  (drives A0–A7).
    // NOTE: the low byte requires MSBFIRST here due to the physical wiring
    // of the upstream register being reversed — do not change the bit order.
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, (address >> 8) & 0xFF);
    shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, address & 0xFF);
    digitalWrite(LATCH_PIN, HIGH);
}

// Single WE#-controlled write bus cycle (address + data latch, no SDP).
static void write_raw(const uint32_t address, const uint8_t data) {
    set_data_output();
    write_address(address);
    for (int i = 0; i < 8; i++)
        digitalWrite(DATA_PINS[i], (data >> i) & 1);
    digitalWrite(nWE, LOW);    // falling edge latches address
    delayMicroseconds(1);      // TWP ≥ 40 ns
    digitalWrite(nWE, HIGH);   // rising edge latches data
    delayMicroseconds(1);      // TWPH ≥ 30 ns
}

static uint8_t read_raw(const uint32_t address) {
    set_data_input();
    write_address(address);
    digitalWrite(nOE, LOW);
    delayMicroseconds(1);      // TAA ≤ 70 ns
    uint8_t val = 0;
    for (int i = 0; i < 8; i++)
        val |= static_cast<uint8_t>(digitalRead(DATA_PINS[i]) << i);
    digitalWrite(nOE, HIGH);
    return val;
}

// ---------------------------------------------------------------------------
// Toggle-Bit (DQ6) polling — used for sector/chip erase where we must
// wait up to ~100 ms.  Per datasheet §3.5, we confirm completion with an
// additional double-read after DQ6 stops toggling.
//
// Returns true  = operation complete within timeout_ms.
//         false = timed out (erase likely failed).
// ---------------------------------------------------------------------------
static bool poll_toggle(const uint32_t address, const uint32_t timeout_ms) {
    set_data_input();
    write_address(address);
    const unsigned long deadline = millis() + timeout_ms;

    while (millis() < deadline) {
        // Read DQ6 twice in quick succession
        digitalWrite(nOE, LOW); delayMicroseconds(1);
        uint8_t r1 = 0;
        for (int i = 0; i < 8; i++) r1 |= (uint8_t)(digitalRead(DATA_PINS[i]) << i);
        digitalWrite(nOE, HIGH);

        digitalWrite(nOE, LOW); delayMicroseconds(1);
        uint8_t r2 = 0;
        for (int i = 0; i < 8; i++) r2 |= (uint8_t)(digitalRead(DATA_PINS[i]) << i);
        digitalWrite(nOE, HIGH);

        if ((r1 & 0x40) == (r2 & 0x40)) {
            // DQ6 stopped toggling — confirm with two more reads (§3.5)
            digitalWrite(nOE, LOW); delayMicroseconds(1);
            uint8_t r3 = 0;
            for (int i = 0; i < 8; i++) r3 |= (uint8_t)(digitalRead(DATA_PINS[i]) << i);
            digitalWrite(nOE, HIGH);

            digitalWrite(nOE, LOW); delayMicroseconds(1);
            uint8_t r4 = 0;
            for (int i = 0; i < 8; i++) r4 |= (uint8_t)(digitalRead(DATA_PINS[i]) << i);
            digitalWrite(nOE, HIGH);

            if ((r3 & 0x40) == (r4 & 0x40)) return true;
            // False stop — DQ6 resumed toggling; keep waiting
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void flash_init() {
    pinMode(CLK_PIN,   OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN,  OUTPUT);
    pinMode(ADDR15,    OUTPUT);
    pinMode(ADDR16,    OUTPUT);
    pinMode(ADDR17,    OUTPUT);
    pinMode(ADDR18,    OUTPUT);   // flash A18 — NC on 010A/020A, safe to drive
    pinMode(nCE,       OUTPUT);
    pinMode(nOE,       OUTPUT);
    pinMode(nWE,       OUTPUT);

    // Safe idle state: chip selected, outputs and writes inhibited
    digitalWrite(nCE, LOW);
    digitalWrite(nWE, HIGH);
    digitalWrite(nOE, HIGH);
}

uint8_t flash_read(const uint32_t address) {
    digitalWrite(nWE, HIGH);
    return read_raw(address);
}

bool flash_write_byte(const uint32_t address, const uint8_t data) {
    // Erased cells already read 0xFF — no program needed
    if (data == 0xFF) return true;

    digitalWrite(nOE, HIGH);

    // JEDEC SDP 3-byte unlock sequence (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0xA0);

    // 4th cycle: target address and data
    write_raw(address, data);

    // Byte program completes within 20 µs (datasheet max).
    // A simple fixed delay is faster than polling for such a short window.
    delayMicroseconds(25);
    return true;
}

bool flash_sector_erase(const uint32_t address) {
    digitalWrite(nOE, HIGH);

    // JEDEC SDP 6-byte sector-erase sequence (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x80);
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(address,  0x30);   // SA = any address within the target sector

    // Typical 18 ms, max 100 ms per datasheet
    return poll_toggle(address, 200);
}

bool flash_chip_erase() {
    digitalWrite(nOE, HIGH);

    // JEDEC SDP 6-byte chip-erase sequence (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x80);
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x10);

    // Typical 70 ms, max 100 ms per datasheet
    return poll_toggle(0x0000UL, 300);
}

void flash_get_id(uint8_t &mfr_id, uint8_t &dev_id) {
    digitalWrite(nOE, HIGH);

    // Software Product ID Entry (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x90);
    delayMicroseconds(1);   // TIDA ≤ 150 ns

    mfr_id = flash_read(0x0000UL);
    dev_id = flash_read(0x0001UL);

    // Software Product ID Exit — single-cycle form (Table 4-2, note 6)
    digitalWrite(nOE, HIGH);
    write_raw(0x0000UL, 0xF0);
    delayMicroseconds(1);
}