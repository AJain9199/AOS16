#ifndef PINS_H
#define PINS_H

// ---------------------------------------------------------------------------
// 74HC595 shift register chain — drives flash address bits A0–A15
// Two cascaded 595s: first byte shifted → downstream reg (A8–A15),
//                    second byte shifted → upstream reg  (A0–A7)
//
// The 595s are driven by the ATmega328 *hardware SPI* peripheral:
//   DATA  = D11 (MOSI, PB3)
//   CLK   = D13 (SCK,  PB5)   <-- was D12; the wire must be moved to D13
//   LATCH = D8  (PB0)
// ---------------------------------------------------------------------------
#define LATCH_PIN  8
#define CLK_PIN    13
#define DATA_PIN   11

// ---------------------------------------------------------------------------
// Upper address bits driven directly from Arduino analog pins (PORTC).
//
//   Arduino pin │ port │ flash pin │ macro  │ used by
//   ────────────┼──────┼───────────┼────────┼──────────────────────
//   A3          │ PC3  │ A16       │ ADDR16 │ 010A (AMS), 020A, 040
//   A4          │ PC4  │ A17       │ ADDR17 │ 020A (AMS), 040  (NC on 010A)
//   A5          │ PC5  │ A18       │ ADDR18 │ 040  (AMS)       (NC on 010A/020A)
//
// The flash_write_address() fast path writes PC3..PC5 as one field, so these
// three pins must stay consecutive on PORTC.
// ---------------------------------------------------------------------------
#define ADDR16  A3
#define ADDR17  A4
#define ADDR18  A5

// ---------------------------------------------------------------------------
// Flash control lines
// ---------------------------------------------------------------------------
#define nCE  7      // PD7  Chip Enable   (active low)
#define nOE  9      // PB1  Output Enable (active low)
#define nWE  10     // PB2  Write Enable  (active low) — also SPI SS, must stay OUTPUT

// ---------------------------------------------------------------------------
// Direct-port bit masks used by the fast paths in flash.cpp.
// ---------------------------------------------------------------------------
#define LATCH_BIT  _BV(PB0)
#define nOE_BIT    _BV(PB1)
#define nWE_BIT    _BV(PB2)

// Data bus: DQ0..DQ2 = A0..A2 = PC0..PC2,  DQ3..DQ7 = D2..D6 = PD2..PD6
#define DATA_MASK_C  0x07
#define DATA_MASK_D  0x7C
// Upper address field on PORTC (PC3..PC5)
#define ADDR_MASK_C  0x38

#endif //PINS_H
