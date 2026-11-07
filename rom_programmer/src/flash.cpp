#include "flash.h"
#include "pins.h"
#include <util/crc16.h>
#include <util/delay.h>

// ---------------------------------------------------------------------------
// Fast bus access using direct port I/O + hardware SPI.
//
//   DQ0..2 = PC0..2, DQ3..7 = PD2..6   (PORTC/PORTD are shared with other
//   signals, so every access is a masked read-modify-write)
//   A16..18 = PC3..5
//   WE#/OE#/LATCH = PB2/PB1/PB0, written with single-instruction sbi/cbi
// ---------------------------------------------------------------------------

static bool     g_data_out  = false;       // current data-bus direction
static uint32_t g_last_addr = 0xFFFFFFFFUL; // address currently on the bus (cache)

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

static inline void set_data_input() {
    if (!g_data_out) return;
    DDRC  &= ~DATA_MASK_C;
    DDRD  &= ~DATA_MASK_D;
    PORTC &= ~DATA_MASK_C;   // no pull-ups
    PORTD &= ~DATA_MASK_D;
    g_data_out = false;
}

static inline void set_data_output() {
    if (g_data_out) return;
    DDRC |= DATA_MASK_C;
    DDRD |= DATA_MASK_D;
    g_data_out = true;
}

static inline void spi_send(const uint8_t b) {
    SPDR = b;
    while (!(SPSR & _BV(SPIF))) {}
}

static inline void write_address(uint32_t address) {
    address &= 0x7FFFFUL;
    if (address == g_last_addr) return;
    g_last_addr = address;

    // A16–A18 on PC3–PC5
    PORTC = (PORTC & ~ADDR_MASK_C) | static_cast<uint8_t>(((address >> 16) & 0x07) << 3);

    // Lower 16 bits through the two cascaded 74HC595s, MSB first, high byte
    // first.  NOTE: the bit order matches the physical wiring of the
    // registers — do not change it.
    PORTB &= ~LATCH_BIT;
    spi_send(static_cast<uint8_t>(address >> 8));
    spi_send(static_cast<uint8_t>(address));
    PORTB |= LATCH_BIT;
}

static inline void put_data(const uint8_t data) {
    PORTC = (PORTC & ~DATA_MASK_C) | (data & DATA_MASK_C);
    PORTD = (PORTD & ~DATA_MASK_D) | ((data >> 1) & DATA_MASK_D);
}

static inline uint8_t get_data() {
    return static_cast<uint8_t>((PINC & DATA_MASK_C) | ((PIND & DATA_MASK_D) << 1));
}

// Single WE#-controlled write bus cycle (address + data latch, no SDP).
// Each sbi/cbi is 2 cycles = 125 ns at 16 MHz, which satisfies TWP >= 40 ns
// and TWPH >= 30 ns; the nop gives the 595 outputs time to settle (TAS).
static inline void write_raw(const uint32_t address, const uint8_t data) {
    set_data_output();
    write_address(address);
    put_data(data);
    __asm__ __volatile__("nop\n\tnop\n\t");
    PORTB &= ~nWE_BIT;    // falling edge latches address
    __asm__ __volatile__("nop\n\t");
    PORTB |= nWE_BIT;     // rising edge latches data
}

// One OE#-controlled read with the address already on the bus.
// TAA <= 70 ns; two nops + the cbi give ~250 ns before sampling.
static inline uint8_t read_bus() {
    PORTB &= ~nOE_BIT;
    __asm__ __volatile__("nop\n\tnop\n\t");
    const uint8_t v = get_data();
    PORTB |= nOE_BIT;
    return v;
}

static inline uint8_t read_raw(const uint32_t address) {
    set_data_input();
    write_address(address);
    return read_bus();
}

// JEDEC SDP 3-byte program prefix (Table 4-2)
static inline void program_prefix() {
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0xA0);
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
        const uint8_t r1 = read_bus();
        const uint8_t r2 = read_bus();

        if ((r1 & 0x40) == (r2 & 0x40)) {
            // DQ6 stopped toggling — confirm with two more reads (§3.5)
            const uint8_t r3 = read_bus();
            const uint8_t r4 = read_bus();
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
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(DATA_PIN,  OUTPUT);   // MOSI
    pinMode(CLK_PIN,   OUTPUT);   // SCK
    pinMode(ADDR16,    OUTPUT);
    pinMode(ADDR17,    OUTPUT);
    pinMode(ADDR18,    OUTPUT);   // flash A18 — NC on 010A/020A, safe to drive
    pinMode(nCE,       OUTPUT);
    pinMode(nOE,       OUTPUT);
    pinMode(nWE,       OUTPUT);   // also SPI SS: must be an output in master mode

    // Safe idle state: chip selected, outputs and writes inhibited
    digitalWrite(nCE, LOW);
    digitalWrite(nWE, HIGH);
    digitalWrite(nOE, HIGH);

    // Hardware SPI: master, MSB first, mode 0, fosc/2 = 8 MHz.  The 74HC595
    // is specified for 25+ MHz at 5 V.
    SPCR = _BV(SPE) | _BV(MSTR);
    SPSR = _BV(SPI2X);

    // Data bus starts as inputs, address cache invalid
    DDRC  &= ~DATA_MASK_C;
    DDRD  &= ~DATA_MASK_D;
    g_data_out  = false;
    g_last_addr = 0xFFFFFFFFUL;
}

uint8_t flash_read(const uint32_t address) {
    return read_raw(address);
}

void flash_read_block(const uint32_t address, uint8_t *buf, const uint16_t len) {
    set_data_input();
    for (uint16_t i = 0; i < len; i++) {
        write_address(address + i);
        buf[i] = read_bus();
    }
}

uint16_t flash_crc16(const uint32_t address, const uint32_t len) {
    uint16_t crc = 0;                       // CRC-16/XMODEM (matches binascii.crc_hqx)
    set_data_input();
    for (uint32_t i = 0; i < len; i++) {
        write_address(address + i);
        crc = _crc_xmodem_update(crc, read_bus());
    }
    return crc;
}

bool flash_write_byte(const uint32_t address, const uint8_t data) {
    return flash_write_block(address, &data, 1);
}

bool flash_write_block(const uint32_t address, const uint8_t *buf, const uint16_t len) {
    PORTB |= nOE_BIT;                       // outputs off while we drive the bus
    for (uint16_t i = 0; i < len; i++) {
        const uint8_t data = buf[i];
        // Erased cells already read 0xFF — no program needed
        if (data == 0xFF) continue;

        program_prefix();
        write_raw(address + i, data);       // 4th cycle: target address and data

        // Byte program completes within Tbp = 20 µs (max) after the final WE#
        // rising edge.  Wait 18 µs here, then preload the next unlock address
        // (SPI + latch, ~2 µs) so the wait overlaps useful work and the next
        // WE# falling edge is still >= 20 µs away.
        _delay_us(18);
        write_address(0x5555UL);
    }
    set_data_input();                       // release the bus
    return true;
}

bool flash_sector_erase(const uint32_t address) {
    PORTB |= nOE_BIT;

    // JEDEC SDP 6-byte sector-erase sequence (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x80);
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(address,  0x30);   // SA = any address within the target sector

    // Typical 18 ms, max 25 ms per datasheet; allow generous margin
    return poll_toggle(address, 200);
}

bool flash_chip_erase() {
    PORTB |= nOE_BIT;

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
    PORTB |= nOE_BIT;

    // Software Product ID Entry (Table 4-2)
    write_raw(0x5555UL, 0xAA);
    write_raw(0x2AAAUL, 0x55);
    write_raw(0x5555UL, 0x90);
    delayMicroseconds(1);   // TIDA ≤ 150 ns

    mfr_id = flash_read(0x0000UL);
    dev_id = flash_read(0x0001UL);

    // Software Product ID Exit — single-cycle form (Table 4-2, note 6)
    PORTB |= nOE_BIT;
    write_raw(0x0000UL, 0xF0);
    delayMicroseconds(1);
    set_data_input();
}
