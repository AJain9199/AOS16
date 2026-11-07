#include "Arduino.h"

#define SETMODE(MODE) for (int i = 2; i < 18; i++) {pinMode(i, MODE);}

void writeData(uint16_t data) {
  SETMODE(OUTPUT);
  for (int i = 2; i < 18; i++) {
    digitalWrite(i, (data >> (i-2)) & 1);
  }
}

uint16_t readData() {
  SETMODE(INPUT);
  uint16_t data = 0;
  for (int i = 2; i < 18; i++) {
    data |= ((uint16_t)digitalRead(i) << (i-2));

  }
  return data;
}

uint16_t mangler(uint16_t input) {
  uint64_t tmp = (uint64_t) input;
  return (uint16_t) ((tmp * 0x9E37 ^ (tmp >> 3)) & 0xffff);
}

#define MAL 19
#define RSE 21
#define CRE 23
#define CWE 25
#define DBS 27
#define CLK 29

void loadAddr(uint16_t addr) {
  digitalWrite(CLK, 0);
  writeData(addr);
  digitalWrite(MAL, 1);

  digitalWrite(CLK, 1);


  digitalWrite(CLK, 0);
  digitalWrite(MAL, 1);
}

void readCheck() {
  digitalWrite(CRE, 1);
  
  for (uint16_t i = 0; i < 0x7fff; i++) {
    loadAddr(i);
    if (i % 1000 == 0) {
      Serial.print(".");
    }

    SETMODE(INPUT);
    digitalWrite(CRE, 0);

    uint16_t data = readData();

    if (data != mangler(i)) {

    Serial.print("At address ");
    Serial.print(i);
    Serial.print(" | Expected ");
    Serial.print(mangler(i));
    Serial.print(" got ");
    Serial.print(data);
     Serial.print(" xor ");  Serial.println(data ^
  mangler(i), HEX);
    }
    digitalWrite(CRE, 1);
  }

  Serial.println("\nTests complete."); 
}

void checkROM() {
  Serial.print("Begun ROM test (using hash)...");
  digitalWrite(RSE, 1);
  readCheck();
}

void checkRAM() {
  Serial.print("Begun RAM test (using hash)...\n");
  digitalWrite(RSE, 0);

  Serial.print("Writing...");
  for (uint16_t i = 0; i < 0x7fff; i++) {
    loadAddr(i);
    writeData(mangler(i));
    
    if (i % 1000 == 0) {
      Serial.print(".");
    }

    digitalWrite(CWE, 0);
    delayMicroseconds(1);
    digitalWrite(CWE, 1);
  }
  
  Serial.print("\nReading...");

  readCheck();
}

void setup() {
  Serial.begin(115200);
  pinMode(MAL, OUTPUT);
  pinMode(CLK, OUTPUT);

  pinMode(CRE, OUTPUT);
  pinMode(CWE, OUTPUT);
  pinMode(RSE, OUTPUT);
  pinMode(DBS, OUTPUT);

  digitalWrite(CRE, 1);
  digitalWrite(CWE, 1);
  digitalWrite(RSE, 1);
  checkRAM();
}

void loop() {}
