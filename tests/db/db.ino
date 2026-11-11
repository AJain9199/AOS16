#include "fastbus.h"

#define NOP __asm__("nop")

#define SEL 38
#define nR0 39
#define nW0 0
#define nR1 40
#define nW1 41

#define PORT1 ADDR1, IO1, nR1, nW1
#define PORT0 ADDR0, IO0, nR0, nW0

typedef unsigned int uint;


template <class A, class B, int nR, int nW> FB_INLINE void writeCycle(typename A::type addr, typename B::type data) {
  // busMode<B>(OUTPUT);
  busWrite<A>(addr);

  busWrite<B>(data);

  digitalWrite(nW, 0);
  NOP;
  digitalWrite(nW, 1);
} 

inline uint16_t mangler(uint16_t x) {
  x ^= x >> 3;                                     // reversible xorshift
  return ((uint64_t)x * 0x9E37u) & 0xffff;       // reversible: odd multiplier
}

inline uint16_t demangler(uint16_t y) {
  uint16_t x = ((uint64_t) y * 0x7787u) & 0xffff; // 0x7787 = inverse of 0x9E37 mod 2^16
  x ^= (x >> 3) ^ (x >> 6) ^ (x >> 9) ^ (x >> 12) ^ (x >> 15);  // undo xorshift
  return x;
}

template <class A, class B, int nR, int nW> FB_INLINE typename B::type readCycle(typename A::type addr) {
  busWrite<A>(addr);
  // busMode<B>(INPUT);

  digitalWrite(nR, 0);

  NOP;
  uint16_t data = busRead<B>();
  digitalWrite(nR, 1);
  return data;
}

int sel_state = true;

void toggle_sel() {
  sel_state = !sel_state;
  digitalWrite(SEL, sel_state);
}

void set_sel_state(int state) {
  sel_state = state;
  digitalWrite(SEL, state);
}

template <class A, class B, int nR, int nW> FB_INLINE typename B::type readCycleDbg(typename A::type addr) {
  busWrite<A>(addr);
  busMode<B>(OUTPUT); busWrite<B>(0xA5A5);   // pre-charge bus to a sentinel
  busMode<B>(INPUT);
  digitalWrite(nR, 0);
  delayMicroseconds(2);                       // generous access time
  uint16_t d = busRead<B>();
  digitalWrite(nR, 1);
  return d;
}

void readback_test(uint32_t max_addr) {
  busMode<IO1>(OUTPUT);
  for (unsigned int i = 0; i < max_addr; i++) {
    writeCycle<PORT1>(i, mangler(i));
  }

  Serial.println("Writing elapsed.");

  busMode<IO1>(INPUT);

  for (uint i = 0; i < max_addr; i++) {
    uint16_t got = readCycle<PORT1>(i);
    if (got != mangler(i)) {
      Serial.print("Error in port 1 read. Address: ");
      Serial.print(i, HEX);
      Serial.print(". Expected: ");
      Serial.print(mangler(i), HEX);
      Serial.print(", Got: ");
      Serial.print(got, HEX);
      Serial.println(".");
    }
  }
  Serial.println("Port 1 read elapsed.");

  busMode<IO0>(INPUT);
  toggle_sel();

  for (uint i = 0; i < max_addr; i++) {
    uint16_t got = readCycle<PORT0>(i);
    if (got != mangler(i)) {
      Serial.print("Error in port 0 read. Address: ");
      Serial.print(i, HEX);
      Serial.print(". Expected: ");
      Serial.print(mangler(i), HEX);
      Serial.print(", Got: ");
      Serial.print(got, HEX);
      Serial.print(". ");
      Serial.print("(Corresponds to address ");
      Serial.print(demangler(got), HEX);
      Serial.println(").");
    }
  }

  Serial.println("Port 0 read elapsed.");
  Serial.println("Test elapsed.");

  toggle_sel();
}



void setup() {
  Serial.begin(115200);

  pinMode(SEL, OUTPUT);
  pinMode(nR0, OUTPUT);
  pinMode(nR1, OUTPUT);
  pinMode(nW1, OUTPUT);

  digitalWrite(nR0, 1);
  digitalWrite(nW1, 1);
  digitalWrite(nR1, 1);
  digitalWrite(SEL, 1);

  // digitalWrite(nR0, 0);
  // digitalWrite(SEL, 0);


  busMode<ADDR0>(OUTPUT);
  busMode<ADDR1>(OUTPUT);

  busMode<IO0>(OUTPUT);
  busMode<IO1>(OUTPUT);

  busWrite<IO0>(0xffff);
  busWrite<ADDR0>(0xffff);

  Serial.println("\n\n\n==========================\n");

  uint max_addr = 0x7fff;

  Serial.println("\nInitialized. Beginning test. (SEL=1)");
  set_sel_state(1);
  readback_test(0x7fff);

  set_sel_state(0);

  readback_test(0x7fff);
}

void loop() {}