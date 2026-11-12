// si5351_freq.h — program an Si5351 CLK1 (SI5351_CLK) to a given frequency in Hz over a
// SoftwareWire (or any Wire-like) bus. Assumes a 25 MHz crystal.
//
//   SoftwareWire i2cbus(46, 47);
//   i2cbus.begin();
//   si5351Init(i2cbus);                       // false if no ACK
//   uint32_t f = si5351SetFreq(i2cbus, 25175000UL);  // returns actual Hz, 0 if out of range
//   si5351Off(i2cbus);
//
// Range ~3.9 kHz .. 150 MHz. PLLA is set fractionally to freq * D (D = even
// integer output divider, PLL in 600..900 MHz); below 500 kHz the R divider
// (/2../128) is used. Registers are written directly, so Adafruit_SI5351
// (which only takes a hardware TwoWire) is not needed.

#pragma once
#include <Arduino.h>

#define SI5351_ADDR 0x60
#define SI5351_XTAL_HZ 25000000UL
#ifndef SI5351_CLK
#define SI5351_CLK 1          // output channel 0..2 (CLK0..CLK2)
#endif

template <class W>
static bool si5351Write(W& w, uint8_t reg, const uint8_t* data, uint8_t n) {
  w.beginTransmission(SI5351_ADDR);
  w.write(reg);
  for (uint8_t i = 0; i < n; i++) w.write(data[i]);
  return w.endTransmission() == 0;
}
template <class W>
static bool si5351Write1(W& w, uint8_t reg, uint8_t v) { return si5351Write(w, reg, &v, 1); }

// Pack a + b/c into the 8 synth-parameter registers.
static void si5351Pack(uint8_t* r, uint32_t a, uint32_t b, uint32_t c, uint8_t rDivBits) {
  uint32_t t  = (uint32_t)(((uint64_t)128 * b) / c);
  uint32_t p1 = 128UL * a + t - 512;
  uint32_t p2 = 128UL * b - c * t;
  uint32_t p3 = c;
  r[0] = p3 >> 8;
  r[1] = p3;
  r[2] = ((p1 >> 16) & 0x03) | (rDivBits << 4);
  r[3] = p1 >> 8;
  r[4] = p1;
  r[5] = ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F);
  r[6] = p2 >> 8;
  r[7] = p2;
}

template <class W>
static bool si5351Init(W& w) {
  if (!si5351Write1(w, 3, 0xFF)) return false;                    // outputs off (also probes ACK)
  for (uint8_t i = 16; i < 24; i++) si5351Write1(w, i, 0x80);     // power down CLK0-7
  si5351Write1(w, 183, 0xD2);                                     // 10 pF crystal load
  return true;
}

template <class W>
static void si5351Off(W& w) { si5351Write1(w, 3, 0xFF); si5351Write1(w, 16 + SI5351_CLK, 0x80); }

template <class W>
static uint32_t si5351SetFreq(W& w, uint32_t freq) {
  if (freq < 3900UL || freq > 150000000UL) return 0;

  uint8_t rBits = 0;
  uint32_t fSynth = freq;
  while (fSynth < 500000UL && rBits < 7) { fSynth <<= 1; rBits++; }

  uint32_t D = (uint32_t)(900000000UL / fSynth) & ~1UL;           // even, PLL <= 900 MHz
  if (D < 6) D = 6;
  if (D > 1800) D = 1800;

  uint64_t fPll = (uint64_t)fSynth * D;
  uint32_t a = fPll / SI5351_XTAL_HZ;
  uint64_t rem = fPll - (uint64_t)a * SI5351_XTAL_HZ;
  const uint32_t c = 1048575UL;
  uint32_t b = (uint32_t)((rem * c + SI5351_XTAL_HZ / 2) / SI5351_XTAL_HZ);
  if (b >= c) { a++; b = 0; }

  uint8_t pll[8], ms[8];
  si5351Pack(pll, a, b, c, 0);
  si5351Pack(ms, D, 0, 1, rBits);

  si5351Write1(w, 3, 0xFF);        // outputs off while reprogramming
  si5351Write(w, 26, pll, 8);      // PLLA
  si5351Write(w, 42 + 8 * SI5351_CLK, ms, 8);  // MultiSynth for the chosen clock
  si5351Write1(w, 16 + SI5351_CLK, 0x4F);  // CLKn: on, integer mode, PLLA, MS0, 8 mA
  si5351Write1(w, 177, 0x20);      // reset PLLA
  si5351Write1(w, 3, (uint8_t)~(1 << SI5351_CLK));  // enable only this clock

  double pllHz = SI5351_XTAL_HZ * ((double)a + (double)b / c);
  return (uint32_t)(pllHz / D / (1UL << rBits) + 0.5);
}

// Debug: dump status + key registers to Serial. Call after si5351SetFreq().
template <class W>
static void si5351Dump(W& w) {
  delay(200);  // give the PLL time to lock before reading status
  uint8_t regs[] = {0, 3, 16, 26, 27, 28, 29, 30, 31, 32, 33, 42, 43, 44, 45, 46, 47, 48, 49, 177, 183};
  for (uint8_t i = 0; i < sizeof(regs); i++) {
    w.beginTransmission(SI5351_ADDR); w.write(regs[i]);
    uint8_t e = w.endTransmission(false);
    w.requestFrom((uint8_t)SI5351_ADDR, (uint8_t)1);
    int v = w.available() ? w.read() : -1;
    Serial.print(F("reg ")); Serial.print(regs[i]); Serial.print(F(" = 0x")); Serial.print(v, HEX);
    Serial.print(F(" (tx err ")); Serial.print(e); Serial.println(')');
  }
}

// Debug: poll status reg 0 (b7 SYS_INIT, b6 LOL_B, b5 LOL_A, b4 LOS) for ~2 s.
template <class W>
static void si5351Poll(W& w) {
  for (uint8_t i = 0; i < 10; i++) {
    w.beginTransmission(SI5351_ADDR); w.write((uint8_t)0); w.endTransmission(false);
    w.requestFrom((uint8_t)SI5351_ADDR, (uint8_t)1);
    int v = w.available() ? w.read() : -1;
    Serial.print(F("status 0x")); Serial.print(v, HEX);
    Serial.print((v & 0x80) ? F(" SYS_INIT") : F(""));
    Serial.print((v & 0x20) ? F(" LOL_A") : F(" locked"));
    Serial.println();
    delay(200);
  }
}
