// tests_basic.h — bring-up, wiring, port isolation and SEL behaviour (tests 0-9)
#pragma once
#include "harness.h"

// =============================================================== 0: quick
// The original readback test: hash pattern written through port 1, verified
// through port 1, then (after flipping SEL) through port 0. Runs for both SEL
// states so both boards are covered. This is also the boot-time check.
static void quickBody(uint8_t) {
  HashPat p;
  fullPass(p);
}
static bool test_quick() { return runBothSel(quickBody); }

// ================================================================ 1: idle
static void printIdle(const __FlashStringHelper* name, uint16_t v) {
  Serial.print(F("    ")); Serial.print(name); Serial.print('='); printHex4(v);
}

static bool test_idle() {
  errBegin();
  if (!(RegG::pin() & NR0_BIT)) failMsg(F("nR0 reads low while idle (shorted or driven externally?)"));
  if (!(RegG::pin() & NR1_BIT)) failMsg(F("nR1 reads low while idle"));
  if (!(RegG::pin() & NW1_BIT)) failMsg(F("nW1 reads low while idle"));
  setSel(0);
  if (RegD::pin() & SEL_BIT) failMsg(F("SEL does not follow when driven low"));
  setSel(1);
  if (!(RegD::pin() & SEL_BIT)) failMsg(F("SEL does not follow when driven high"));
  flipSel();
  if (RegD::pin() & SEL_BIT) failMsg(F("flipSel() did not toggle SEL"));
  flipSel();
  if (!(RegD::pin() & SEL_BIT)) failMsg(F("flipSel() did not toggle SEL back"));
  printIdle(F("IO0(idle)"), busRead<IO0>());
  printIdle(F("IO1(idle)"), busRead<IO1>());
  Serial.println();
  return g_errors == 0;
}

// ===================================================== 2: address lines
// 32 addresses: 0, each single bit, 0x7FFF, and 0x7FFF with each bit cleared.
// Each gets a unique value; any stuck/shorted address line makes two of them
// alias and the read-back fails.
static uint16_t addrLineVal(uint16_t a) { return (uint16_t)(mangler(a) ^ 0x5A5A); }

static void addrLinesBody(uint8_t) {
  uint16_t list[32];
  uint8_t n = 0;
  list[n++] = 0;
  for (uint8_t b = 0; b < 15; b++) list[n++] = (uint16_t)1 << b;
  list[n++] = 0x7FFF;
  for (uint8_t b = 0; b < 15; b++) list[n++] = (uint16_t)(0x7FFF ^ ((uint16_t)1 << b));

  cpuDrive();
  for (uint8_t i = 0; i < n; i++) cpuWrite<REL_W>(list[i], addrLineVal(list[i]));
  cpuRelease();
  for (uint8_t i = 0; i < n; i++) {
    uint16_t got = cpuRead<REL_R>(list[i]);
    if (got != addrLineVal(list[i])) reportErr(F("port1"), list[i], addrLineVal(list[i]), got);
  }
  flipSel();
  for (uint8_t i = 0; i < n; i++) {
    uint16_t got = gpuRead<REL_R>(list[i]);
    if (got != addrLineVal(list[i])) reportErr(F("port0"), list[i], addrLineVal(list[i]), got);
  }
  flipSel();
}
static bool test_addrlines() { return runBothSel(addrLinesBody); }

// ======================================================== 3: data lines
static void dataLinesBody(uint8_t) {
  static const uint16_t addrs[4] = {0x0000, 0x7FFF, 0x2AAA, 0x5555};
  uint16_t pats[36];
  uint8_t n = 0;
  for (uint8_t b = 0; b < 16; b++) pats[n++] = (uint16_t)1 << b;
  for (uint8_t b = 0; b < 16; b++) pats[n++] = (uint16_t)~((uint16_t)1 << b);
  pats[n++] = 0x0000; pats[n++] = 0xFFFF; pats[n++] = 0xAAAA; pats[n++] = 0x5555;

  for (uint8_t i = 0; i < n; i++) {
    cpuDrive();
    for (uint8_t k = 0; k < 4; k++) cpuWrite<REL_W>(addrs[k], pats[i]);
    cpuRelease();
    for (uint8_t k = 0; k < 4; k++) {
      uint16_t got = cpuRead<REL_R>(addrs[k]);
      if (got != pats[i]) reportErr(F("port1"), addrs[k], pats[i], got);
    }
    flipSel();
    for (uint8_t k = 0; k < 4; k++) {
      uint16_t got = gpuRead<REL_R>(addrs[k]);
      if (got != pats[i]) reportErr(F("port0"), addrs[k], pats[i], got);
    }
    flipSel();
  }
}
static bool test_datalines() { return runBothSel(dataLinesBody); }

// ================================================== 4: full-space sweeps
static void sweepsBody(uint8_t) {
  uint32_t e;
  { AddrPat p;     e = fullPass(p); if (!g_quiet) { Serial.print(F("    addr-as-data   errors=")); Serial.println(e); } }
  { InvAddrPat p;  e = fullPass(p); if (!g_quiet) { Serial.print(F("    ~addr-as-data  errors=")); Serial.println(e); } }
  { ZeroPat p;     e = fullPass(p); if (!g_quiet) { Serial.print(F("    all 0000       errors=")); Serial.println(e); } }
  { OnesPat p;     e = fullPass(p); if (!g_quiet) { Serial.print(F("    all FFFF       errors=")); Serial.println(e); } }
  { CheckPat p;    e = fullPass(p); if (!g_quiet) { Serial.print(F("    checker        errors=")); Serial.println(e); } }
  { InvCheckPat p; e = fullPass(p); if (!g_quiet) { Serial.print(F("    ~checker       errors=")); Serial.println(e); } }
  { HashPat p;     e = fullPass(p); if (!g_quiet) { Serial.print(F("    hash           errors=")); Serial.println(e); } }
}
static bool test_sweeps() { return runBothSel(sweepsBody); }

// ============================================ 5: concurrent dual-port read
// The two boards hold different data (Hash / ~Hash). Both read strobes are then
// asserted at the same time, once with equal addresses and once with
// complementary addresses (maximum activity on the other port's lines).
static void dualReadBody(uint8_t s) {
  HashPat h; HashInvPat hi;
  // X(s) <- Hash, X(!s) <- ~Hash
  writeAll<REL_W>(h);
  flipSel();
  writeAll<REL_W>(hi);
  // SEL=!s now: port 1 -> X(!s) holds ~Hash, port 0 -> X(s) holds Hash.
  for (uint8_t mode = 0; mode < 2; mode++) {
    for (uint16_t a = 0; a < ADDR_COUNT; a++) {
      uint16_t a1 = a;
      uint16_t a0 = mode ? (uint16_t)(~a & 0x7FFF) : a;
      uint16_t d0, d1;
      dualRead<REL_R>(a0, a1, d0, d1);
      if (d0 != h.at(a0))  reportErr(mode ? F("p0 vs compl.") : F("p0 same addr"), a0, h.at(a0), d0, true);
      if (d1 != hi.at(a1)) reportErr(mode ? F("p1 vs compl.") : F("p1 same addr"), a1, hi.at(a1), d1);
    }
  }
  flipSel();
}
static bool test_dualread() { return runBothSel(dualReadBody); }

// ====================================== 6: isolation under concurrent write
// X(s) holds Hash. After a flip it is read through port 0 while port 1
// simultaneously overwrites the OTHER board at the same address with junk
// (overlapping strobes, sweeping the edge phase). Port 0 must never see junk
// and the other board must end up fully junk.
static void isolationBody(uint8_t s) {
  HashPat h; JunkPat j;
  writeAll<REL_W>(h);
  flipSel();                       // X(s) now on port 0
  cpuDrive();
  // GPU-style scan: nR0 held low for the whole run, port 1 writes overlapped.
  // Errors are collected and printed after nR0 is released.
  uint16_t badA[4], badE[4], badG[4];
  uint32_t nbad = 0;
  nR0Low();
  for (uint16_t a = 0; a < ADDR_COUNT; a++) {
    uint16_t got = dualPhase((uint8_t)a, a, j.at(a), a);
    uint16_t exp = h.at(a);
    if (got != exp) {
      if (nbad < 4) { badA[nbad] = a; badE[nbad] = exp; badG[nbad] = got; }
      nbad++;
    }
  }
  nR0High();
  for (uint32_t k = 0; k < nbad; k++) {
    if (k < 4) reportErr(F("port0 during p1 write"), badA[k], badE[k], badG[k], true);
    else g_errors++;
  }
  cpuRelease();
  verifyCpu<REL_R>(j, F("junk landed"));   // port 1 -> the other board
  verifyGpu<REL_R>(h, F("port0 after"));   // X(s) untouched
  flipSel();
}
static bool test_isolation() { return runBothSel(isolationBody); }

// ================================================ 7: idle bus levels (info)
// Not a pass/fail test: reports what the shared IO buses float to when no read
// strobe is active, and whether the board's pull resistors (or a bus keeper)
// hold them.
static bool test_busidle() {
  errBegin();
  busMode<IO0>(INPUT);
  uint16_t io0 = busRead<IO0>();
  busMode<IO0>(INPUT_PULLUP);
  uint16_t io0p = busRead<IO0>();
  busMode<IO0>(INPUT);
  busMode<IO1>(INPUT);
  uint16_t io1 = busRead<IO1>();
  busMode<IO1>(INPUT_PULLUP);
  uint16_t io1p = busRead<IO1>();
  busMode<IO1>(INPUT);
  // Drive 0x0000 then release: does the bus keep it?
  busMode<IO1>(OUTPUT);
  busWrite<IO1>(0x0000);
  busMode<IO1>(INPUT);
  uint16_t keep = busRead<IO1>();

  Serial.print(F("    IO0 no-pull=")); printHex4(io0);  Serial.print(F("  Arduino pull-up=")); printHex4(io0p);
  Serial.println(io0 == io0p ? F("  (held by board)") : F("  (FLOATING bits: differ)"));
  Serial.print(F("    IO1 no-pull=")); printHex4(io1);  Serial.print(F("  Arduino pull-up=")); printHex4(io1p);
  Serial.println(io1 == io1p ? F("  (held by board)") : F("  (FLOATING bits: differ)"));
  Serial.print(F("    IO1 after driving 0000 and releasing=")); printHex4(keep); Serial.println();
  return true;
}

// ================================================== 8: board separation
// Distinct data on each board, verified through each board's own ports, then
// again after thousands of idle SEL flips (flipping must never disturb data).
static void separationBody(uint8_t s) {
  HashPat h; HashInvPat hi;
  writeAll<REL_W>(h);
  verifyCpu<REL_R>(h, F("A p1"));
  flipSel();
  writeAll<REL_W>(hi);
  verifyCpu<REL_R>(hi, F("B p1"));
  verifyGpu<REL_R>(h, F("A p0"));
  flipSel();
  verifyCpu<REL_R>(h, F("A p1 again"));
  verifyGpu<REL_R>(hi, F("B p0"));
  for (uint16_t i = 0; i < 4000; i++) { flipSel(); Nops<(int)3>::run(); }   // even count: ends at SEL=s
  verifyCpu<REL_R>(h, F("A after flips"));
  verifyGpu<REL_R>(hi, F("B after flips"));
}
static bool test_separation() { return runBothSel(separationBody); }

// ====================================================== 9: SEL flip margin
// Flip SEL M NOPs before a write strobe (flip->write) or M NOPs after one
// (write->flip). The write must land on the board port 1 pointed at when the
// strobe actually fired; the other board must be untouched. Margins below
// FLIP_SAFE_MARGIN are informational; at or above it a mismatch fails.
#define FLIP_SAFE_MARGIN 2

template <int MODE, int M>
static uint8_t flipTrial(uint16_t a0) {
  HashPat h; HashInvPat hi;
  const uint16_t origOld = h.at(a0);    // board port 1 reached when the trial began
  const uint16_t origNew = hi.at(a0);   // the other board
  const uint16_t J = (uint16_t)(0xDEAD ^ (uint16_t)(M * 0x111) ^ MODE);

  busWrite<ADDR1>(a0);
  cpuDrive();
  busWrite<IO1>(J);
  if (MODE == 0) {  // flip, then strobe
    flipSel(); Nops<M>::run();
    nW1Low(); Nops<REL_W>::run(); nW1High();
  } else {          // strobe, then flip
    nW1Low(); Nops<REL_W>::run(); nW1High();
    Nops<M>::run(); flipSel();
  }
  cpuRelease();
  // SEL is flipped now: port 1 -> "new" board, port 0 -> "old" board.
  uint16_t gotNew = cpuRead<REL_R>(a0);
  uint16_t gotOld = gpuRead<REL_R>(a0);
  uint16_t expNew = (MODE == 0) ? J : origNew;
  uint16_t expOld = (MODE == 0) ? origOld : J;
  uint8_t bad = (gotNew != expNew) + (gotOld != expOld);

  // repair both cells, ending back on the original SEL
  cpuDrive();
  cpuWrite<REL_W>(a0, origNew);
  flipSel();
  cpuWrite<REL_W>(a0, origOld);
  cpuRelease();
  return bad;
}

template <int MODE, int M>
static void flipMargin() {
  uint8_t bad = 0;
  for (uint8_t rep = 0; rep < 8; rep++) bad += flipTrial<MODE, M>((uint16_t)(0x0555 * (rep + 1)));
  Serial.print(F("    ")); Serial.print(MODE == 0 ? F("flip->write") : F("write->flip"));
  Serial.print(F(" margin ")); Serial.print(M); Serial.print(F(" NOPs: "));
  if (!bad) Serial.println(F("ok"));
  else if (M >= FLIP_SAFE_MARGIN) { Serial.print(F("MISMATCH x")); Serial.println(bad); g_errors++; }
  else { Serial.print(F("mismatch x")); Serial.print(bad); Serial.println(F(" (below safe margin, informational)")); }
}

static void flipGlitchBody(uint8_t s) {
  HashPat h; HashInvPat hi;
  writeAll<REL_W>(h);
  flipSel();
  writeAll<REL_W>(hi);
  flipSel();                       // back to SEL=s
  flipMargin<0, 0>(); flipMargin<0, 1>(); flipMargin<0, 2>(); flipMargin<0, 3>();
  flipMargin<1, 0>(); flipMargin<1, 1>(); flipMargin<1, 2>(); flipMargin<1, 3>();
  // whole-array integrity after all the flip abuse
  verifyCpu<REL_R>(h, F("A p1"));
  flipSel();
  verifyCpu<REL_R>(hi, F("B p1"));
  flipSel();
}
static bool test_flipglitch() { return runBothSel(flipGlitchBody); }
