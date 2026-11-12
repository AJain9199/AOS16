// cpu_fb.h — the Arduino as the "CPU" side of the double buffer.
//
// Salvaged from tests/db/hal.h (fast strobes, cpuWrite/cpuRead, SEL flip) and
// tests/db/harness.h (write/verify sweeps), trimmed to what a framebuffer writer
// needs. The Arduino drives PORT 1 (ADDR1, IO1, nR1, nW1) and SEL. PORT 0
// (ADDR0, IO0, nR0, nW0) belongs to the GPU, so every one of those pins is left
// as a high-impedance input here: the Arduino must never drive them.
//
// Two boards are stacked with inverted SEL:
//     SEL=1 : port 1 -> board A      port 0 (GPU) -> board B
//     SEL=0 : port 1 -> board B      port 0 (GPU) -> board A
// so the CPU always writes the "back" board and fbSwap() hands it to the GPU.
#pragma once
#include "fastbus.h"

#define SEL_PIN 38  // PD7
#define NR0_PIN 39  // PG2, GPU-driven: input only
#define NR1_PIN 40  // PG1
#define NW1_PIN 41  // PG0
// nW0 (pin 0) is Serial RX and is pulled high on the board; never touched.

#define FB_WORDS 0x8000u   // 32K x 16
#define CPU_W 3            // NOPs added to the write strobe (~125 ns + 3 clocks low)
#define CPU_R 4            // NOPs before the read-back sample

typedef fastbus::Reg<fastbus::PortD> RegD;
typedef fastbus::Reg<fastbus::PortG> RegG;
enum : uint8_t { SEL_BIT = 0x80, NW1_BIT = 0x01, NR1_BIT = 0x02 };

template <int N> struct Nops {
  static FB_INLINE void run() { __asm__ __volatile__("nop"); Nops<N - 1>::run(); }
};
template <> struct Nops<0> {
  static FB_INLINE void run() {}
};

// single-instruction strobes (cbi/sbi)
static FB_INLINE void nW1Low()  { RegG::port() &= (uint8_t)~NW1_BIT; }
static FB_INLINE void nW1High() { RegG::port() |= NW1_BIT; }
static FB_INLINE void nR1Low()  { RegG::port() &= (uint8_t)~NR1_BIT; }
static FB_INLINE void nR1High() { RegG::port() |= NR1_BIT; }

static uint8_t g_sel = 1;

static FB_INLINE void setSel(uint8_t s) {
  if (s) RegD::port() |= SEL_BIT; else RegD::port() &= (uint8_t)~SEL_BIT;
  g_sel = s ? 1 : 0;
}
// Writing a 1 to PINx toggles PORTx: a single-cycle, glitch-free flip.
static FB_INLINE void flipSel() {
  RegD::pin() = SEL_BIT;
  g_sel ^= 1;
}

static FB_INLINE void cpuDrive()   { busMode<IO1>(OUTPUT); }  // before writes
static FB_INLINE void cpuRelease() { busMode<IO1>(INPUT); }   // before reads

template <int W> static FB_INLINE void cpuWrite(uint16_t a, uint16_t d) {
  busWrite<ADDR1>(a);
  busWrite<IO1>(d);
  nW1Low();
  Nops<W>::run();
  nW1High();
}

template <int R> static FB_INLINE uint16_t cpuRead(uint16_t a) {
  busWrite<ADDR1>(a);
  nR1Low();
  Nops<R>::run();
  uint16_t d = busRead<IO1>();
  nR1High();
  return d;
}

// Idle state: strobes high, SEL=1 (CPU writes board A first), port 1 address
// driven and IO1 released, every port-0 pin an input with no pull-up.
static void fbInit() {
  pinMode(SEL_PIN, OUTPUT);
  pinMode(NR1_PIN, OUTPUT);
  pinMode(NW1_PIN, OUTPUT);
  nR1High();
  nW1High();
  setSel(1);

  busMode<ADDR1>(OUTPUT);
  busWrite<ADDR1>(0);
  busMode<IO1>(INPUT);

  busMode<ADDR0>(INPUT);   // GPU drives these
  busMode<IO0>(INPUT);
  pinMode(NR0_PIN, INPUT);
}

// Frame sources: at(a) is the word stored at address a.
struct BlankImage {
  uint16_t at(uint16_t) const { return 0x000f; }
};

// Writes the whole frame into the board port 1 currently reaches.
template <class P> static void fbWrite(const P& img) {
  cpuDrive();
  for (uint16_t a = 0; a < FB_WORDS; a++) cpuWrite<CPU_W>(a, img.at(a));
  cpuRelease();
}

// Reads the frame back through port 1. Returns the number of mismatching words
// and prints the first few.
template <class P> static uint32_t fbVerify(const P& img) {
  uint32_t errs = 0;
  for (uint16_t a = 0; a < FB_WORDS; a++) {
    uint16_t exp = img.at(a);
    uint16_t got = cpuRead<CPU_R>(a);
    if (got != exp) {
      if (errs < 8) {
        Serial.print(F("  mismatch addr=0x")); Serial.print(a, HEX);
        Serial.print(F(" expected=0x")); Serial.print(exp, HEX);
        Serial.print(F(" got=0x")); Serial.println(got, HEX);
      }
      errs++;
    }
  }
  return errs;
}

// Hand the board just written to the GPU (the hardware "swp"). The CPU's port 1
// then points at the other board, which is the new back buffer.
static FB_INLINE void fbSwap() { flipSel(); }
