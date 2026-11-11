// Double-buffer board test suite (Arduino Mega 2560, 115200 baud).
//
//   q          quick mode: basic wiring/integrity readback (runs on boot)
//   a          run the full suite (aborts if the quick test fails)
//   <n>        run test n         e.g. "4"
//   <n> <arg>  run test n with a numeric argument, e.g. "15 100" (100 frames
//              per phase), "13 30" (30 s retention), "14 50" (50 soak laps)
//   ?          list tests
//
// Wiring and architecture notes live in hal.h; the test groups are in
// tests_basic.h, tests_stress.h and tests_deploy.h.
// Timing overrides (defaults live in hal.h): #define EQ_W / EQ_R / EQ_N here.

#include "hal.h"
#include "harness.h"
#include "tests_basic.h"
#include "tests_stress.h"
#include "tests_deploy.h"

struct TestDef {
  const char* name;
  bool (*fn)();
  bool inAll;   // included in the 'a' run
};

static const TestDef TESTS[] = {
  {"quick readback (hash, both ports, both SEL)",   test_quick,              true},
  {"idle levels: strobes, SEL",                     test_idle,               true},
  {"address lines (aliasing, stuck bits)",          test_addrlines,          true},
  {"data lines (walking 1/0, solids)",              test_datalines,          true},
  {"full-space pattern sweeps",                     test_sweeps,             true},
  {"concurrent dual-port read",                     test_dualread,           true},
  {"isolation: read port 0 while writing port 1",   test_isolation,          true},
  {"idle IO bus levels (info only)",                test_busidle,            true},
  {"board separation + SEL flip storm",             test_separation,         true},
  {"SEL flip margin vs write strobe",               test_flipglitch,         true},
  {"strobe width / sample delay sweep",             test_strobe,             true},
  {"scattered address order",                       test_scatter,            true},
  {"worst-case bit/address switching",              test_switching,          true},
  {"retention (default 5 s)",                       test_retention,          true},
  {"soak (default 10 laps)",                        test_soak,               true},
  {"deploy sim: interleaved 9:5, 8 edge phases",    test_deploy_interleaved, true},
  {"deploy sim: ISR-driven async GPU",              test_deploy_isr,         true},
  {"deploy sim: full (15 + 16)",                    test_deploy_full,        false},
};
static const uint8_t N_TESTS = sizeof(TESTS) / sizeof(TESTS[0]);

static bool runTest(uint8_t i) {
  Serial.print(F("[")); if (i < 10) Serial.print(' '); Serial.print(i); Serial.print(F("] "));
  Serial.println(TESTS[i].name);
  uint32_t t0 = millis();
  bool ok = TESTS[i].fn();
  uint32_t dt = millis() - t0;
  Serial.print(ok ? F("[PASS] ") : F("[FAIL] "));
  Serial.print(i); Serial.print(F(" ")); Serial.print(TESTS[i].name);
  Serial.print(F("  (")); Serial.print(dt / 1000.0, 1); Serial.print(F(" s"));
  if (!ok) { Serial.print(F(", errors=")); Serial.print(g_errors); }
  Serial.println(F(")"));
  setSel(1);
  return ok;
}

static void runQuick() {
  Serial.println(F("--- quick mode: basic wiring / integrity ---"));
  bool ok = runTest(0);
  Serial.println(ok ? F("QUICK: PASS") : F("QUICK: FAIL (fix wiring before running the full suite)"));
}

static void listTests() {
  Serial.println(F("Commands: q quick | a all | <n> [arg] one test | ? list"));
  for (uint8_t i = 0; i < N_TESTS; i++) {
    Serial.print(F("  ")); if (i < 10) Serial.print(' '); Serial.print(i); Serial.print(F("  ")); Serial.println(TESTS[i].name);
  }
}

static void runAll() {
  Serial.println(F("=== full suite ==="));
  if (!runTest(0)) {
    Serial.println(F("Quick test failed: aborting, later results would be meaningless."));
    return;
  }
  uint8_t pass = 1, fail = 0;
  bool failed[N_TESTS];
  for (uint8_t i = 0; i < N_TESTS; i++) failed[i] = false;
  for (uint8_t i = 1; i < N_TESTS; i++) {
    if (!TESTS[i].inAll) continue;
    if (runTest(i)) pass++; else { fail++; failed[i] = true; }
  }
  Serial.println(F("=== summary ==="));
  Serial.print(F("passed: ")); Serial.print(pass); Serial.print(F("  failed: ")); Serial.println(fail);
  for (uint8_t i = 0; i < N_TESTS; i++) {
    if (failed[i]) { Serial.print(F("  FAILED: ")); Serial.print(i); Serial.print(' '); Serial.println(TESTS[i].name); }
  }
  Serial.println(fail ? F("SUITE: FAIL") : F("SUITE: PASS"));
}

void setup() {
  Serial.begin(115200);
  hwInit();
  Serial.println(F("\n\n==========================\nDouble buffer test rig"));
  runQuick();
  listTests();
}

// Next byte without consuming it, waiting up to 50 ms for it to arrive (bytes
// of a typed command can trickle in one at a time). -1 on timeout.
static int peekWait() {
  uint32_t t0 = millis();
  while (!Serial.available()) {
    if (millis() - t0 > 50) return -1;
  }
  return Serial.peek();
}

static int readNumber(int first) {
  int v = first;
  while (true) {
    int c = peekWait();
    if (c < '0' || c > '9') break;
    v = v * 10 + (Serial.read() - '0');
  }
  return v;
}

void loop() {
  if (!Serial.available()) return;
  int c = Serial.read();
  if (c == 'q') { g_arg = 0; runQuick(); }
  else if (c == 'a') { g_arg = 0; runAll(); }
  else if (c == '?') listTests();
  else if (c >= '0' && c <= '9') {
    int n = readNumber(c - '0');
    g_arg = 0;
    if (peekWait() == ' ') { Serial.read(); g_arg = readNumber(0); }
    if (n < N_TESTS) runTest(n); else Serial.println(F("no such test"));
    g_arg = 0;
  }
}
