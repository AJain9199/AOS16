// tests_deploy.h — CPU/GPU deployment simulation (tests 15-17)
//
// The real system: the CPU (port 1) writes a frame into the "back" board while
// the GPU (port 0) scans the "front" board; when the frame is complete the CPU
// flips SEL, swapping the roles of the two boards. The GPU behaves like a VGA
// scanner: nR0 is held LOW through a run of pixel data while the address keeps
// changing, then goes HIGH for a blanking/sync period.
//
// One MCU cannot truly run two 5-9 MHz bus masters, so this is simulated two ways:
//   15  interleaved: deterministic 9 GPU reads : 5 CPU writes inside each active
//       run, CPU-only writes during blanking, and the CPU write strobe overlapped
//       with the GPU sample at 8 different edge phases (reproducible).
//   16  ISR: a timer interrupt performs the GPU scan steps and preempts the CPU
//       loop at arbitrary points, including mid-strobe (true asynchrony).
// The GPU sample delay is DEPLOY_N (the ~9 MHz-equivalent access time, see
// hal.h); the 9:5 ratio and the interleaving are realistic, absolute throughput
// is not.
#pragma once
#include "harness.h"
#include <avr/interrupt.h>

#ifndef DEPLOY_FRAMES
#define DEPLOY_FRAMES 4   // frames per phase / per ISR rate (override with the numeric arg)
#endif
#define GPU_RUN   256     // GPU reads per active run (nR0 low)
#define GPU_BLANK 16      // CPU writes (interleaved sim) / timer ticks (ISR sim) with nR0 high

static uint8_t g_frame = 0;  // id of the frame currently on the GPU (front) board

// Frame id n is written in sequential order when n is even and in a scattered
// order when odd, so both access styles are exercised.
static FB_INLINE uint16_t cpuOrder(uint8_t n, uint16_t w) {
  return (n & 1) ? (uint16_t)(w * 0x4E6B) & 0x7FFF : w;
}

// Leaves: SEL flipped so port 0 sees frame 0, IO1 an input, g_frame = 0.
static void deployInit() {
  nR0High();
  setSel(1);
  FramePat f0(0);
  writeAll<REL_W>(f0);
  flipSel();
  g_frame = 0;
}

// ============================================== 15: interleaved simulation
static uint32_t deployFrames(uint8_t phase, uint16_t frames) {
  uint32_t e0 = g_errors;
  uint16_t badA[4], badE[4], badG[4];
  uint32_t nbad = 0;
#define NOTE_BAD(A, E, G) do { if (nbad < 4) { badA[nbad] = (A); badE[nbad] = (E); badG[nbad] = (G); } nbad++; } while (0)

  for (uint16_t f = 0; f < frames; f++) {
    const uint8_t front = g_frame;
    const uint8_t back  = (uint8_t)(g_frame + 1);
    uint16_t gpos = 0;      // GPU scan position in the front board
    uint8_t  acc  = 0;      // Bresenham accumulator for the 9:5 ratio
    bool     active = false;// true while nR0 is held low
    uint16_t runLeft = 0;
    uint8_t  blank = 0;
    cpuDrive();
    for (uint16_t w = 0; w < ADDR_COUNT; w++) {
      const uint16_t wa = cpuOrder(back, w);
      const uint16_t wd = framePat(back, wa);

      if (!active) {                       // blanking: nR0 high, CPU writes only
        cpuWrite<DEPLOY_W>(wa, wd);
        if (++blank >= GPU_BLANK) { blank = 0; active = true; runLeft = GPU_RUN; nR0Low(); }
        continue;
      }

      acc += 9;
      uint8_t reads = 0;
      while (acc >= 5) { acc -= 5; reads++; }   // always >= 1 since 9 > 5
      const uint8_t nr = reads;

      // first GPU read shares the CPU write cycle (overlapping strobes)
      uint16_t ra = gpos; gpos = (gpos + 1) & 0x7FFF;
      uint16_t got = dualPhase(phase, wa, wd, ra);
      uint16_t exp = framePat(front, ra);
      if (got != exp) NOTE_BAD(ra, exp, got);

      while (--reads) {
        ra = gpos; gpos = (gpos + 1) & 0x7FFF;
        got = scanRead<DEPLOY_N>(ra);
        exp = framePat(front, ra);
        if (got != exp) NOTE_BAD(ra, exp, got);
      }

      if (runLeft > nr) runLeft -= nr;
      else { runLeft = 0; nR0High(); active = false; }   // end of active run -> blanking
    }
    if (active) { nR0High(); active = false; }
    cpuRelease();
    flipSel();               // frame complete: swap buffers
    g_frame = back;

    // report after the frame so Serial output never stalls a scan
    for (uint32_t k = 0; k < nbad; k++) {
      if (k < 4) reportErr(F("gpu"), badA[k], badE[k], badG[k]);
      else g_errors++;
    }
    nbad = 0;
  }
#undef NOTE_BAD
  return g_errors - e0;
}

static bool test_deploy_interleaved() {
  errBegin();
  uint16_t frames = g_arg ? g_arg : DEPLOY_FRAMES;
  deployInit();
  for (uint8_t phase = 0; phase < DEPLOY_PHASES; phase++) {
    uint32_t e = deployFrames(phase, frames);
    Serial.print(F("    phase ")); Serial.print(phase);
    Serial.print(phase < 4 ? F(" (write edge ") : F(" (write edge +"));
    Serial.print(phase < 4 ? 3 - phase : phase - 4);
    Serial.print(F(" NOP")); Serial.print(phase < 4 ? F(" before sample): ") : F(" after sample): "));
    if (e == 0) Serial.println(F("ok")); else { Serial.print(e); Serial.println(F(" errors")); }
  }
  // the last written frame is now on port 0: verify the entire thing
  FramePat last(g_frame);
  verifyGpu<REL_R>(last, F("final frame"));
  Serial.print(F("    frames simulated: ")); Serial.println((uint32_t)frames * DEPLOY_PHASES);
  setSel(1);
  return g_errors == 0;
}

// ======================================================= 16: ISR-driven GPU
static volatile uint8_t  isr_cur = 0;     // frame on the GPU board
static volatile uint8_t  isr_next = 0;    // frame about to replace it (set before the flip)
static volatile uint16_t isr_addr = 0;
static volatile uint16_t isr_left = 0;    // reads left in the current active run
static volatile uint8_t  isr_gap = 0;     // blanking ticks left (nR0 high); 0 = active
static volatile uint32_t isr_reads = 0;
static volatile uint32_t isr_errs = 0;
struct IsrBad { uint16_t a, e, g; };
static volatile IsrBad isr_bad[8];

// One tick = one GPU step. During an active run nR0 stays low between ticks
// and each tick only moves the address and samples; during blanking nR0 is
// high and ticks do nothing. Right around a flip either frame is legal; any
// other value (stale, torn, corrupted by CPU writes) is an error.
ISR(TIMER1_COMPA_vect) {
  if (isr_gap) {
    if (--isr_gap == 0) { isr_left = GPU_RUN; nR0Low(); }
    return;
  }
  uint16_t a = isr_addr;
  uint16_t got = scanRead<DEPLOY_N>(a);
  uint16_t e1 = framePat(isr_cur, a);
  if (got != e1) {
    uint16_t e2 = framePat(isr_next, a);
    if (got != e2) {
      uint32_t n = isr_errs;
      if (n < 8) { isr_bad[n].a = a; isr_bad[n].e = e1; isr_bad[n].g = got; }
      isr_errs = n + 1;
    }
  }
  isr_addr = (a + 1) & 0x7FFF;
  isr_reads++;
  if (--isr_left == 0) { nR0High(); isr_gap = GPU_BLANK; }
}

static void isrStart(uint32_t hz) {
  isr_reads = 0; isr_errs = 0; isr_addr = 0; isr_left = 0; isr_gap = 1;
  nR0High();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10);       // CTC, no prescaler
  OCR1A = (uint16_t)(F_CPU / hz - 1);
  TCNT1 = 0;
  TIFR1 = _BV(OCF1A);
  TIMSK1 = _BV(OCIE1A);
}

static void isrStop() {
  TIMSK1 = 0;
  TCCR1B = 0;
  nR0High();
}

static uint32_t isrRun(uint32_t hz, uint16_t frames) {
  const uint32_t e0 = g_errors;
  isr_cur = g_frame; isr_next = g_frame;
  cpuDrive();
  isrStart(hz);
  for (uint16_t f = 0; f < frames; f++) {
    const uint8_t back = (uint8_t)(g_frame + 1);
    for (uint16_t w = 0; w < ADDR_COUNT; w++) {
      uint16_t wa = cpuOrder(back, w);
      cpuWrite<DEPLOY_W>(wa, framePat(back, wa));
    }
    isr_next = back;
    flipSel();
    isr_cur = back;
    g_frame = back;
  }
  isrStop();
  cpuRelease();

  Serial.print(F("    ")); Serial.print(hz); Serial.print(F(" Hz GPU: reads=")); Serial.print(isr_reads);
  Serial.print(F(" errors=")); Serial.println(isr_errs);
  uint32_t shown = isr_errs < 8 ? isr_errs : 8;
  for (uint32_t i = 0; i < shown; i++) {
    reportErr(F("gpu isr"), isr_bad[i].a, isr_bad[i].e, isr_bad[i].g);
  }
  g_errors += (isr_errs > shown) ? (isr_errs - shown) : 0;
  if (isr_reads == 0) failMsg(F("ISR never ran"));
  return g_errors - e0;
}

static bool test_deploy_isr() {
  errBegin();
  uint16_t frames = g_arg ? g_arg : DEPLOY_FRAMES;
  deployInit();
  // Rates a ~14 us ISR can sustain; each is asynchronous to the CPU loop.
  isrRun(10000, frames);
  isrRun(25000, frames);
  isrRun(40000, frames);
  FramePat last(g_frame);
  verifyGpu<REL_R>(last, F("final frame"));
  setSel(1);
  return g_errors == 0;
}

// ============================================= 17: full deployment run
static bool test_deploy_full() {
  bool a = test_deploy_interleaved();
  bool b = test_deploy_isr();
  Serial.print(F("    interleaved: ")); Serial.print(a ? F("pass") : F("FAIL"));
  Serial.print(F("   isr: ")); Serial.println(b ? F("pass") : F("FAIL"));
  return a && b;
}
