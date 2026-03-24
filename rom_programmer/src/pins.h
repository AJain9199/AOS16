#ifndef PINS_H
#define PINS_H

// ---------------------------------------------------------------------------
// 74HC595 shift register chain — drives flash address bits A0–A14
// Two cascaded 595s: first shiftOut byte → downstream reg (A8–A14),
//                   second shiftOut byte → upstream reg  (A0–A7)
// ---------------------------------------------------------------------------
#define LATCH_PIN  8
#define CLK_PIN    12
#define DATA_PIN   11

// ---------------------------------------------------------------------------
// Upper address bits driven directly from Arduino digital pins.
//
// Flash address bit mapping (see write_address() in flash.cpp):
//
//   Arduino pin │ flash pin │ macro  │ used by
//   ────────────┼───────────┼────────┼──────────────────────
//   A3          │ A15       │ ADDR15 │ 010A, 020A, 040
//   4           │ A16       │ ADDR16 │ 010A (AMS), 020A, 040
//   A5          │ A17       │ ADDR17 │ 020A (AMS), 040
//   13          │ A18       │ ADDR18 │ 040  (AMS)
//
// On SST39SF010A pin A17 is NC, on SST39SF020A pin A18 is NC — the firmware
// drives those pins anyway; the chip simply ignores them.
//
// WARNING: A6 and A7 are analog-input-only on Uno/Nano.
//          Do NOT reassign ADDR18/ADDR19 to those pins.
// ---------------------------------------------------------------------------
#define ADDR15  A3   // flash A15  — 010A / 020A / 040
#define ADDR16  4    // flash A16  — 010A (AMS) / 020A / 040
#define ADDR17  A5   // flash A17  — 020A (AMS) / 040   (NC on 010A)
#define ADDR18  13   // flash A18  — 040  (AMS)          (NC on 010A/020A)

// ---------------------------------------------------------------------------
// Flash control lines
// ---------------------------------------------------------------------------
#define nCE  A4      // Chip Enable   (active low)
#define nOE  2       // Output Enable (active low)
#define nWE  3       // Write Enable  (active low)

#endif //PINS_H
