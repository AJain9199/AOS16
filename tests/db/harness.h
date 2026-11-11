// harness.h — error accounting, reporting, and sweep helpers shared by all tests
#pragma once
#include "hal.h"

#define MAX_PRINT 16

static uint32_t g_errors  = 0;     // errors in the current test
static uint16_t g_printed = 0;     // detailed error lines printed so far
static bool     g_mute    = false; // count errors but do not print them
static bool     g_hold    = false; // errBegin() keeps counters (used by soak)
static bool     g_quiet   = false; // suppress per-SEL banners (used by soak)
static uint16_t g_arg     = 0;     // optional numeric argument from the serial command

static void errBegin() {
  if (g_hold) return;
  g_errors = 0;
  g_printed = 0;
}

static void printHex4(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x100)  Serial.print('0');
  if (v < 0x10)   Serial.print('0');
  Serial.print(v, HEX);
}

static void reportErr(const __FlashStringHelper* tag, uint16_t addr, uint16_t expected,
                      uint16_t got, bool hint = false) {
  g_errors++;
  if (g_mute || g_printed >= MAX_PRINT) return;
  g_printed++;
  Serial.print(F("    ")); Serial.print(tag);
  Serial.print(F(" addr=")); printHex4(addr);
  Serial.print(F(" expected=")); printHex4(expected);
  Serial.print(F(" got=")); printHex4(got);
  Serial.print(F(" xor=")); printHex4(expected ^ got);
  if (hint) {
    Serial.print(F(" (word belongs to address ")); printHex4(demangler(got)); Serial.print(')');
  }
  Serial.println();
}

static void failMsg(const __FlashStringHelper* msg) {
  g_errors++;
  Serial.print(F("    FAIL: ")); Serial.println(msg);
}

static void infoMsg(const __FlashStringHelper* msg) {
  Serial.print(F("    info: ")); Serial.println(msg);
}

// Runs `body(s)` once with SEL=1 and once with SEL=0, so both stacked boards get
// exercised through port 1 (and through port 0 after a flip). Leaves SEL=1.
typedef void (*SelBody)(uint8_t sel);

static bool runBothSel(SelBody body) {
  errBegin();
  for (int8_t s = 1; s >= 0; s--) {
    setSel(s);
    if (!g_quiet) { Serial.print(F("  SEL=")); Serial.println(s); }
    body(s);
  }
  setSel(1);
  return g_errors == 0;
}

// ------------------------------------------------------------- sweeps
// Port 1 write of pattern p in visiting order o. Leaves IO1 an input.
template <int W, class P, class O>
static void writeOrd(const P& p, const O& o) {
  cpuDrive();
  for (uint16_t i = 0; i < ADDR_COUNT; i++) {
    uint16_t a = o.a(i);
    cpuWrite<W>(a, p.at(a));
  }
  cpuRelease();
}

// Port 1 read-back (IO1 must be an input).
template <int R, class P, class O>
static uint32_t verifyCpuOrd(const P& p, const O& o, const __FlashStringHelper* tag) {
  uint32_t e = 0;
  for (uint16_t i = 0; i < ADDR_COUNT; i++) {
    uint16_t a = o.a(i);
    uint16_t exp = p.at(a);
    uint16_t got = cpuRead<R>(a);
    if (got != exp) { reportErr(tag, a, exp, got, P::hint); e++; }
  }
  return e;
}

// Port 0 read-back.
template <int R, class P, class O>
static uint32_t verifyGpuOrd(const P& p, const O& o, const __FlashStringHelper* tag) {
  uint32_t e = 0;
  for (uint16_t i = 0; i < ADDR_COUNT; i++) {
    uint16_t a = o.a(i);
    uint16_t exp = p.at(a);
    uint16_t got = gpuRead<R>(a);
    if (got != exp) { reportErr(tag, a, exp, got, P::hint); e++; }
  }
  return e;
}

// Port 0 read-back in scan mode: nR0 held low for the whole run, only the
// address changes, N NOPs before each sample. Mismatches are collected first and
// printed after nR0 is released, so Serial output never stalls the scan.
template <int N, class P, class O>
static uint32_t verifyScanOrd(const P& p, const O& o, const __FlashStringHelper* tag) {
  uint32_t e = 0;
  uint16_t badA[4], badE[4], badG[4];
  nR0Low();
  for (uint16_t i = 0; i < ADDR_COUNT; i++) {
    uint16_t a = o.a(i);
    uint16_t got = scanRead<N>(a);
    uint16_t exp = p.at(a);
    if (got != exp) {
      if (e < 4) { badA[e] = a; badE[e] = exp; badG[e] = got; }
      e++;
    }
  }
  nR0High();
  for (uint32_t k = 0; k < e; k++) {
    if (k < 4) reportErr(tag, badA[k], badE[k], badG[k], P::hint);
    else g_errors++;   // beyond the first four: count only
  }
  return e;
}

template <int W, class P> static void writeAll(const P& p) { writeOrd<W>(p, SeqOrd()); }
template <int R, class P> static uint32_t verifyCpu(const P& p, const __FlashStringHelper* tag) {
  return verifyCpuOrd<R>(p, SeqOrd(), tag);
}
template <int R, class P> static uint32_t verifyGpu(const P& p, const __FlashStringHelper* tag) {
  return verifyGpuOrd<R>(p, SeqOrd(), tag);
}

// Write through port 1 at the current SEL, verify through port 1, flip, verify
// through port 0, flip back. Returns errors found.
template <class P>
static uint32_t fullPass(const P& p) {
  uint32_t e0 = g_errors;
  writeAll<REL_W>(p);
  verifyCpu<REL_R>(p, F("port1"));
  flipSel();
  verifyGpu<REL_R>(p, F("port0"));
  flipSel();
  return g_errors - e0;
}
