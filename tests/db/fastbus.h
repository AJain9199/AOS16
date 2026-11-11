// fastbus.h — zero-overhead multi-port buses for the ATmega2560 (Arduino Mega)
//
// Each bus is a compile-time list of segments. A segment says which port it
// lives on, which byte of the bus value it fills, and how that port's bits map
// onto that byte. Every call is force-inlined and fully constant-folded, so
// busRead<IO1>() compiles to two `in` instructions and nothing else.
//
//   busOutput<ADDR1>();          busInput<IO0>();
//   busWrite<ADDR1>(0xBEEF);     uint16_t d = busRead<IO0>();
//   busMode<IO1>(INPUT_PULLUP);  // pinMode-style, also sets the PORT latch
//
// C++11 compatible (the Arduino IDE default).

#pragma once
#include <Arduino.h>

#define FB_INLINE __attribute__((always_inline)) inline

namespace fastbus {

// Data-space address of PINx. DDRx = PINx + 1, PORTx = PINx + 2.
// A–G sit in the low I/O space, so accesses compile to in/out/sbi/cbi;
// H–L are extended I/O and use lds/sts (one extra cycle each).
enum Port : uint16_t {
  PortA = 0x20,  PortB = 0x23,  PortC = 0x26,  PortD = 0x29,
  PortE = 0x2C,  PortF = 0x2F,  PortG = 0x32,
  PortH = 0x100, PortJ = 0x103, PortK = 0x106, PortL = 0x109
};

template <uint16_t P> struct Reg {
  static FB_INLINE volatile uint8_t& pin()  { return *(volatile uint8_t*)(P); }
  static FB_INLINE volatile uint8_t& ddr()  { return *(volatile uint8_t*)(P + 1); }
  static FB_INLINE volatile uint8_t& port() { return *(volatile uint8_t*)(P + 2); }
};

// Constant 8-bit shift: S > 0 shifts left, S < 0 shifts right.
// avr-gcc 7 sometimes widens small right shifts to 16 bits (asr/ror pairs),
// so right shifts of 1-3 are forced to plain 8-bit `lsr`. Larger shifts are
// left to the compiler, which uses `swap`.
template <int S> FB_INLINE uint8_t sh(uint8_t x) {
  if (S < 0 && S >= -3) {
    if (S <= -1) __asm__("lsr %0" : "+r"(x));
    if (S <= -2) __asm__("lsr %0" : "+r"(x));
    if (S <= -3) __asm__("lsr %0" : "+r"(x));
    return x;
  }
  return S >= 0 ? uint8_t(x << (S > 0 ? S : 0)) : uint8_t(x >> (S < 0 ? -S : 0));
}

constexpr bool fitsByte(unsigned m, int s) {
  return s >= 0 ? ((m << (s > 0 ? s : 0)) <= 0xFFu)
                : (((m >> -s) << -s) == m);
}

// One port's contribution to one byte of the bus value.
//   bus byte `Byte` |= ((PINx & M1) shifted by S1) | ((PINx & M2) shifted by S2)
// The optional second mask/shift lets a port with a gap (like PORTH) be
// handled with a single read and a single write.
template <uint16_t P, uint8_t Byte, uint8_t M1, int S1, uint8_t M2 = 0, int S2 = 0>
struct Seg {
  static_assert(fitsByte(M1, S1) && fitsByte(M2, S2), "segment bits spill out of their byte");
  static_assert((M1 & M2) == 0, "segment masks overlap");
  enum : uint16_t { port = P };
  enum : uint8_t  { byte = Byte, mask = M1 | M2, full = ((M1 | M2) == 0xFF) };

  // port pins -> bus byte bits
  static FB_INLINE uint8_t get(uint8_t in) { return sh<S1>(in & M1) | sh<S2>(in & M2); }
  // bus byte bits -> port pins (already masked)
  static FB_INLINE uint8_t put(uint8_t v)  { return (sh<-S1>(v) & M1) | (sh<-S2>(v) & M2); }
};

template <typename T> union Bytes { T v; uint8_t b[sizeof(T)]; };

template <typename T, typename... S> struct Ops;

template <typename T> struct Ops<T> {
  static FB_INLINE void read(Bytes<T>&) {}
  static FB_INLINE void write(const Bytes<T>&) {}
  static FB_INLINE void output() {}
  static FB_INLINE void input() {}
  static FB_INLINE void latch(bool) {}
};

template <typename T, typename S0, typename... S> struct Ops<T, S0, S...> {
  typedef Reg<S0::port> R;
  typedef Ops<T, S...> Next;

  static FB_INLINE void read(Bytes<T>& u) {
    u.b[S0::byte] |= S0::get(R::pin());
    Next::read(u);
  }

  static FB_INLINE void write(const Bytes<T>& u) {
    const uint8_t x = S0::put(u.b[S0::byte]);
    if (S0::full) {
      R::port() = x;                                   // plain store
    } else {
      // Writing 1s to PINx toggles PORTx. Toggling exactly the bits that
      // differ costs the same as a read-modify-write but never clobbers the
      // port's other pins, even if an ISR changes them mid-sequence.
      R::pin() = uint8_t((R::port() ^ x) & S0::mask);
    }
    Next::write(u);
  }

  static FB_INLINE void output() {
    if (S0::full) R::ddr() = 0xFF; else R::ddr() |= uint8_t(S0::mask);
    Next::output();
  }

  static FB_INLINE void input() {
    if (S0::full) R::ddr() = 0x00; else R::ddr() &= uint8_t(~S0::mask);
    Next::input();
  }

  static FB_INLINE void latch(bool high) {
    if (S0::full)  R::port() = high ? 0xFF : 0x00;
    else if (high) R::port() |= uint8_t(S0::mask);
    else           R::port() &= uint8_t(~S0::mask);
    Next::latch(high);
  }
};

template <typename T, typename... S> struct Bus {
  typedef T type;
  typedef Ops<T, S...> ops;
};

} // namespace fastbus

// ---------------------------------------------------------------- public API

template <class B> FB_INLINE typename B::type busRead() {
  fastbus::Bytes<typename B::type> u;
  u.v = 0;
  B::ops::read(u);
  return u.v;
}

template <class B> FB_INLINE void busWrite(typename B::type v) {
  fastbus::Bytes<typename B::type> u;
  u.v = v;
  B::ops::write(u);
}

// Direction only — fastest turnaround. The PORT latch is left alone, so a
// pin last written high keeps its pull-up while it is an input.
template <class B> FB_INLINE void busOutput() { B::ops::output(); }
template <class B> FB_INLINE void busInput()  { B::ops::input(); }

// pinMode() semantics: INPUT clears the latch (no pull-ups), INPUT_PULLUP
// sets it. Folds to straight-line code when `mode` is a constant.
template <class B> FB_INLINE void busMode(uint8_t mode) {
  if (mode == OUTPUT) { B::ops::output(); return; }
  B::ops::input();
  B::ops::latch(mode == INPUT_PULLUP);
}

// ------------------------------------------------------------ your wiring
// Concatenation order is MSB first, so the right-most term is bit 0.

// IO0 = PINL | PINC
typedef fastbus::Bus<uint16_t,
  fastbus::Seg<fastbus::PortC, 0, 0xFF, 0>,         // bits  0-7  = PC0-7
  fastbus::Seg<fastbus::PortL, 1, 0xFF, 0>          // bits  8-15 = PL0-7
> IO0;

// ADDR0 = PINK | PINF
typedef fastbus::Bus<uint16_t,
  fastbus::Seg<fastbus::PortF, 0, 0xFF, 0>,         // bits  0-7  = PF0-7
  fastbus::Seg<fastbus::PortK, 1, 0xFF, 0>          // bits  8-15 = PK0-7
> ADDR0;

// IO1 = PA | PB
typedef fastbus::Bus<uint16_t,
  fastbus::Seg<fastbus::PortB, 0, 0xFF, 0>,         // bits  0-7  = PB0-7
  fastbus::Seg<fastbus::PortA, 1, 0xFF, 0>          // bits  8-15 = PA0-7
> IO1;

// ADDR1 = PG5 | PE5-3 | PD3-0 | PJ1-0 | PH6-3 | PH1-0   (16 bits, MSB first)
// PE6/7, PD4-7, PH2/7 and PG0-4 are not part of the bus.
typedef fastbus::Bus<uint16_t,
  fastbus::Seg<fastbus::PortH, 0, 0x03, 0, 0x78, -1>, // bits 0-1   = PH0-1, bits 2-5 = PH3-6
  fastbus::Seg<fastbus::PortJ, 0, 0x03, 6>,           // bits 6-7   = PJ0-1
  fastbus::Seg<fastbus::PortD, 1, 0x0F, 0>,           // bits 8-11  = PD0-3
  fastbus::Seg<fastbus::PortE, 1, 0x38, 1>,           // bits 12-14 = PE3-5
  fastbus::Seg<fastbus::PortG, 1, 0x20, 2>            // bit  15    = PG5
> ADDR1;
