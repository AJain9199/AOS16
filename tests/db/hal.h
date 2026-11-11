// hal.h — hardware layer for the double-buffer test rig (Arduino Mega 2560)
//
// Everything here is header-only and force-inlined so that a bus cycle is a
// handful of AVR instructions with deterministic timing.
//
// Board topology (two stacked boards, shared buses/strobes, inverted SEL):
//
//     SEL=1 : port 1 -> board A      port 0 -> board B
//     SEL=0 : port 1 -> board B      port 0 -> board A
//
// "X(s)" in comments means "the board port 1 reaches when SEL=s". So after
// flipping SEL, whatever port 1 just wrote is what port 0 now reads.
#pragma once
#include "fastbus.h"

// ---------------------------------------------------------------- timing
// Delays are counted in NOPs (1 NOP = 1 clock = 62.5 ns at 16 MHz).
//   W = extra NOPs while nW1 is low (a cbi/sbi pair alone is ~125 ns low).
//   R = strobed read: NOPs between nR falling (cbi) and the sample.
//   N = scan read (nR0 already low, like the GPU during pixel data): NOPs
//       between the LAST ADDR0 write (the PORTK store) and the sample.
//
// Sampling caveat: the AVR input synchronizer delays a pin change by 0.5-1.5
// clocks before `in`/`lds` can see it. So a sample taken after R (or N) NOPs
// only sees a response that arrived within roughly (R-1.5)..(R-0.5) clocks,
// i.e. about 31-94 ns at R=2 and 94-156 ns at R=3. R=0 or N=0 samples BEFORE the
// edge is visible and is meaningless. Also note IO0's high byte (PORTL, `lds`)
// is sampled 2 clocks after the low byte (PORTC, `in`), so the low byte is the
// binding constraint.
//
// The access time the GPU needs at 9 MHz is < ~111 ns (minus its input setup),
// so R=2 / N=2 (response proven within ~94 ns) is the "9 MHz equivalent"
// setting used by the deployment simulation. Passing at R=1 / N=1 means the
// board answers within about one clock. REL_* are relaxed settings for
// functional tests.
//
// Throughput is NOT 9 MHz: one address step plus a 16-bit sample is at least
// out + in + lds = 4 clocks (~4 MHz) even with no checking, and the loops here
// are slower. What is reproduced is the access-time margin, not the cadence.
#ifndef REL_W
#define REL_W 4
#endif
#ifndef REL_R
#define REL_R 4
#endif
#ifndef EQ_W
#define EQ_W 0
#endif
#ifndef EQ_R
#define EQ_R 2
#endif
#ifndef EQ_N
#define EQ_N 2
#endif
#define DEPLOY_W EQ_W
#define DEPLOY_R EQ_R
#define DEPLOY_N EQ_N

// ------------------------------------------------------------------ pins
// nW0 (pin 0) is deliberately NOT driven: it is Serial RX, the board pulls it
// high, and port 0 never writes in the real system.
#define SEL_PIN 38  // PD7
#define NR0_PIN 39  // PG2
#define NR1_PIN 40  // PG1
#define NW1_PIN 41  // PG0

typedef fastbus::Reg<fastbus::PortD> RegD;
typedef fastbus::Reg<fastbus::PortG> RegG;
enum : uint8_t { SEL_BIT = 0x80, NW1_BIT = 0x01, NR1_BIT = 0x02, NR0_BIT = 0x04 };

#define ADDR_COUNT 0x8000u  // 15-bit address space, 32K x 16

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
static FB_INLINE void nR0Low()  { RegG::port() &= (uint8_t)~NR0_BIT; }
static FB_INLINE void nR0High() { RegG::port() |= NR0_BIT; }

// ------------------------------------------------------------------- SEL
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

// ------------------------------------------------------------ init / dirs
static void hwInit() {
  pinMode(SEL_PIN, OUTPUT);
  pinMode(NR0_PIN, OUTPUT);
  pinMode(NR1_PIN, OUTPUT);
  pinMode(NW1_PIN, OUTPUT);
  nR0High(); nR1High(); nW1High();
  setSel(1);
  busMode<ADDR0>(OUTPUT);
  busMode<ADDR1>(OUTPUT);
  busWrite<ADDR0>(0);
  busWrite<ADDR1>(0);
  busMode<IO0>(INPUT);  // the GPU side only ever reads
  busMode<IO1>(INPUT);
}

static FB_INLINE void cpuDrive()   { busMode<IO1>(OUTPUT); }  // before port-1 writes
static FB_INLINE void cpuRelease() { busMode<IO1>(INPUT); }   // before port-1 reads

// ---------------------------------------------------------- bus cycles
// Port 1 = "CPU" side. IO1 must be an output for cpuWrite, an input for cpuRead.
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

// Port 0 = "GPU" side (read only). IO0 is permanently an input.
template <int R> static FB_INLINE uint16_t gpuRead(uint16_t a) {
  busWrite<ADDR0>(a);
  nR0Low();
  Nops<R>::run();
  uint16_t d = busRead<IO0>();
  nR0High();
  return d;
}

// Both read strobes at once (different or equal addresses), IO0 and IO1 inputs.
template <int R> static FB_INLINE void dualRead(uint16_t a0, uint16_t a1, uint16_t& d0, uint16_t& d1) {
  busWrite<ADDR0>(a0);
  busWrite<ADDR1>(a1);
  nR0Low();
  nR1Low();
  Nops<R>::run();
  d0 = busRead<IO0>();
  d1 = busRead<IO1>();
  nR0High();
  nR1High();
}

// Scan read, the way the GPU reads during pixel data: nR0 is held low by the
// caller (nR0Low() ... nR0High()) and only the address changes. N NOPs elapse
// between the last address write and the sample (see the timing notes above).
template <int N> static FB_INLINE uint16_t scanRead(uint16_t a) {
  busWrite<ADDR0>(a);
  Nops<N>::run();
  return busRead<IO0>();
}

// A CPU write overlapped with a GPU scan read (IO1 output, IO0 input, nR0 held
// low by the caller). Port 1's address/data buses switch first, then the GPU
// address steps. The write strobe's falling edge lands just before the sample
// (dualEarly, ~K NOPs before it; the sample is never earlier than the scan
// delay N) or K NOPs after it, while the address is still held (dualLate).
template <int K> static FB_INLINE uint16_t dualEarly(uint16_t wa, uint16_t wd, uint16_t ra) {
  enum { PRE = (DEPLOY_N > K + 2) ? (DEPLOY_N - K - 2) : 0 };   // cbi itself takes 2 clocks
  busWrite<ADDR1>(wa);
  busWrite<IO1>(wd);
  busWrite<ADDR0>(ra);
  Nops<PRE>::run();
  nW1Low();
  Nops<K>::run();
  uint16_t d = busRead<IO0>();
  nW1High();
  return d;
}

template <int K> static FB_INLINE uint16_t dualLate(uint16_t wa, uint16_t wd, uint16_t ra) {
  busWrite<ADDR1>(wa);
  busWrite<IO1>(wd);
  busWrite<ADDR0>(ra);
  Nops<DEPLOY_N>::run();
  uint16_t d = busRead<IO0>();
  Nops<K>::run();
  nW1Low();
  Nops<DEPLOY_W>::run();
  nW1High();
  return d;
}

#define DEPLOY_PHASES 8
// phase 0..3: write edge 3..0 NOPs before sample; phase 4..7: 0..3 NOPs after.
// nR0 must already be low.
static uint16_t dualPhase(uint8_t phase, uint16_t wa, uint16_t wd, uint16_t ra) {
  switch (phase & 7) {
    case 0:  return dualEarly<3>(wa, wd, ra);
    case 1:  return dualEarly<2>(wa, wd, ra);
    case 2:  return dualEarly<1>(wa, wd, ra);
    case 3:  return dualEarly<0>(wa, wd, ra);
    case 4:  return dualLate<0>(wa, wd, ra);
    case 5:  return dualLate<1>(wa, wd, ra);
    case 6:  return dualLate<2>(wa, wd, ra);
    default: return dualLate<3>(wa, wd, ra);
  }
}

// ------------------------------------------------------------- patterns
// mangler is a bijection on 16 bits, so a read that returns the wrong word can
// be traced back to the address it really came from via demangler().
inline uint16_t mangler(uint16_t x) {
  x ^= x >> 3;                       // reversible xorshift
  return (uint16_t)(x * 0x9E37u);    // odd multiplier, mod 2^16
}

inline uint16_t demangler(uint16_t y) {
  uint16_t x = (uint16_t)(y * 0x7787u);  // 0x7787 = inverse of 0x9E37 mod 2^16
  x ^= (x >> 3) ^ (x >> 6) ^ (x >> 9) ^ (x >> 12) ^ (x >> 15);
  return x;
}

// Frame pattern for the deployment simulation: distinct for every frame id.
static inline uint16_t framePat(uint8_t f, uint16_t a) {
  return mangler((uint16_t)(a ^ (uint16_t)(f * 0x0101u)));
}

struct AddrPat      { static const bool hint = false; uint16_t at(uint16_t a) const { return a; } };
struct InvAddrPat   { static const bool hint = false; uint16_t at(uint16_t a) const { return (uint16_t)~a; } };
struct ZeroPat      { static const bool hint = false; uint16_t at(uint16_t) const { return 0x0000; } };
struct OnesPat      { static const bool hint = false; uint16_t at(uint16_t) const { return 0xFFFF; } };
struct CheckPat     { static const bool hint = false; uint16_t at(uint16_t a) const { return (a & 1) ? 0xAAAA : 0x5555; } };
struct InvCheckPat  { static const bool hint = false; uint16_t at(uint16_t a) const { return (a & 1) ? 0x5555 : 0xAAAA; } };
struct AltDataPat   { static const bool hint = false; uint16_t at(uint16_t a) const { return (a & 1) ? 0xFFFF : 0x0000; } };
struct HashPat      { static const bool hint = true;  uint16_t at(uint16_t a) const { return mangler(a); } };
struct HashInvPat   { static const bool hint = false; uint16_t at(uint16_t a) const { return (uint16_t)~mangler(a); } };
struct JunkPat      { static const bool hint = false; uint16_t at(uint16_t a) const { return (uint16_t)(mangler(a) ^ 0x1234); } };
struct FramePat {
  static const bool hint = false;
  uint8_t f;
  explicit FramePat(uint8_t id) : f(id) {}
  uint16_t at(uint16_t a) const { return framePat(f, a); }
};

// Address visiting orders: i in [0, 0x8000) -> a distinct address.
struct SeqOrd    { uint16_t a(uint16_t i) const { return i; } };
struct StrideOrd {  // odd multiplier => permutation of the 15-bit space
  uint16_t m;
  explicit StrideOrd(uint16_t mul) : m(mul) {}
  uint16_t a(uint16_t i) const { return (uint16_t)(i * m) & 0x7FFF; }
};
struct AltOrd {     // 0, 7FFF, 1, 7FFE, ...: every address bit flips each cycle
  uint16_t a(uint16_t i) const { uint16_t k = i >> 1; return (i & 1) ? (uint16_t)(0x7FFF ^ k) : k; }
};
