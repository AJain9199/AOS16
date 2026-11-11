// tests_stress.h — timing margins, address/data stress, retention, soak (tests 10-14)
#pragma once
#include "harness.h"

// ======================================================= 10: strobe sweep
// Write strobe widths W, strobed read delays R, and scan-read delays N (nR0 held
// low, address stepping, as the GPU does) from relaxed down toward the limit of
// what the MCU can sample. Settings at or slower than the deployment setting
// (EQ_W / EQ_R / EQ_N) must pass; faster ones are informational margin only and
// do not fail the test. R=0 and N=0 are not swept: they sample before the edge
// is visible to the MCU, so the result would say nothing about the board.
// The scan sweep runs both a sequential order (only the low address byte
// changes) and AltOrd (every address line flips each step: the worst case).
static void cfgLine(const __FlashStringHelper* what, int n, uint32_t errs, bool required) {
  if (g_quiet && (errs == 0 || !required)) return;
  Serial.print(F("    ")); Serial.print(what); Serial.print(n);
  if (errs == 0) Serial.println(F(" : ok"));
  else {
    Serial.print(F(" : FAIL (")); Serial.print(errs); Serial.print(F(" errors)"));
    Serial.println(required ? F("") : F("  [faster than deployment setting: info only]"));
  }
}

#define SWEEP_W(W)                                                            \
  do {                                                                        \
    uint32_t e0 = g_errors; HashPat p;                                        \
    writeAll<W>(p);                                                           \
    verifyCpu<REL_R>(p, F("port1")); flipSel();                               \
    verifyGpu<REL_R>(p, F("port0")); flipSel();                               \
    uint32_t e = g_errors - e0; const bool req = (W >= EQ_W);                 \
    if (!req) g_errors = e0;                                                  \
    cfgLine(F("write W="), W, e, req);                                        \
  } while (0)

#define SWEEP_R(R)                                                            \
  do {                                                                        \
    uint32_t e0 = g_errors; HashPat p;                                        \
    verifyCpu<R>(p, F("port1")); flipSel();                                   \
    verifyGpu<R>(p, F("port0")); flipSel();                                   \
    uint32_t e = g_errors - e0; const bool req = (R >= EQ_R);                 \
    if (!req) g_errors = e0;                                                  \
    cfgLine(F("strobed read R="), R, e, req);                                 \
  } while (0)

#define SWEEP_SCAN(N)                                                         \
  do {                                                                        \
    uint32_t e0 = g_errors; HashPat p; SeqOrd so; AltOrd ao;                  \
    flipSel();                                                                \
    verifyScanOrd<N>(p, so, F("scan seq"));                                   \
    uint32_t eSeq = g_errors - e0;                                            \
    verifyScanOrd<N>(p, ao, F("scan alt"));                                   \
    flipSel();                                                                \
    uint32_t e = g_errors - e0; const bool req = (N >= EQ_N);                 \
    if (!req) g_errors = e0;                                                  \
    cfgLine(F("scan read N="), N, e, req);                                    \
    if (e && !g_quiet) { Serial.print(F("      (sequential order errors: ")); \
                         Serial.print(eSeq); Serial.println(')'); }           \
  } while (0)

static void strobeBody(uint8_t) {
  g_mute = true;
  if (!g_quiet) {
    Serial.print(F("    deployment settings: W=")); Serial.print(EQ_W);
    Serial.print(F(" R=")); Serial.print(EQ_R); Serial.print(F(" N=")); Serial.println(EQ_N);
  }
  SWEEP_W(4); SWEEP_W(2); SWEEP_W(1); SWEEP_W(0);
  { HashPat p; writeAll<REL_W>(p); }   // known-good fill for the read sweeps
  SWEEP_R(4); SWEEP_R(3); SWEEP_R(2); SWEEP_R(1);
  SWEEP_SCAN(4); SWEEP_SCAN(3); SWEEP_SCAN(2); SWEEP_SCAN(1);
  g_mute = false;
}
static bool test_strobe() { return runBothSel(strobeBody); }

// ========================================== 11: scattered address order
// Write in one pseudo-random order, read back in two others, so every cycle
// changes many address bits (worst case address-bus settling).
static void scatterBody(uint8_t) {
  HashPat p;
  writeOrd<EQ_W>(p, StrideOrd(0x4E6B));
  verifyCpuOrd<EQ_R>(p, StrideOrd(0x1D95), F("port1"));
  flipSel();
  verifyScanOrd<EQ_N>(p, StrideOrd(0x2B3D), F("port0 scan"));
  flipSel();
}
static bool test_scatter() { return runBothSel(scatterBody); }

// ================================================ 12: worst-case switching
// (a) data alternates 0000/FFFF on consecutive cycles;
// (b) addresses alternate 0000/7FFF/0001/7FFE..., all 15 lines flipping each
//     cycle. Both at the fastest strobe timing.
static void switchingBody(uint8_t) {
  { AltDataPat p;
    writeAll<EQ_W>(p);
    verifyCpu<EQ_R>(p, F("alt-data p1")); flipSel();
    verifyScanOrd<EQ_N>(p, SeqOrd(), F("alt-data p0 scan")); flipSel(); }
  { HashPat p; AltOrd o;
    writeOrd<EQ_W>(p, o);
    verifyCpuOrd<EQ_R>(p, o, F("alt-addr p1")); flipSel();
    verifyScanOrd<EQ_N>(p, o, F("alt-addr p0 scan")); flipSel(); }
}
static bool test_switching() { return runBothSel(switchingBody); }

// ========================================================= 13: retention
// Write, wait (default 5 s, or the numeric argument in seconds), then verify
// through both ports.
static void retentionBody(uint8_t) {
  HashPat p;
  writeAll<REL_W>(p);
  uint16_t secs = g_arg ? g_arg : 5;
  if (!g_quiet) { Serial.print(F("    waiting ")); Serial.print(secs); Serial.println(F(" s")); }
  delay((uint32_t)secs * 1000UL);
  verifyCpu<REL_R>(p, F("port1"));
  flipSel();
  verifyGpu<REL_R>(p, F("port0"));
  flipSel();
}
static bool test_retention() { return runBothSel(retentionBody); }

// ============================================================== 14: soak
// Repeats the sweep, scatter and worst-case tests N times (default 10, or the
// numeric argument) with errors accumulated across iterations.
static bool test_soak() {
  uint16_t n = g_arg ? g_arg : 10;
  g_errors = 0; g_printed = 0;
  g_hold = true; g_quiet = true;
  for (uint16_t i = 0; i < n; i++) {
    test_sweeps();
    test_strobe();
    test_scatter();
    test_switching();
    Serial.print(F("    iteration ")); Serial.print(i + 1); Serial.print('/'); Serial.print(n);
    Serial.print(F("  cumulative errors=")); Serial.println(g_errors);
  }
  g_hold = false; g_quiet = false;
  return g_errors == 0;
}
