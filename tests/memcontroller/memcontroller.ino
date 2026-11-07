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
    data |= (digitalRead(i) << (i-2));
  }
  return data;
}

#define MAL 19
#define RSE 21
#define CRE 23
#define CWE 25
#define DBS 27
#define CLK 29

void setup() {
  Serial.begin(9600);
  pinMode(MAL, OUTPUT);
  pinMode(CLK, OUTPUT);

  pinMode(CRE, OUTPUT);
  pinMode(CWE, OUTPUT);

  digitalWrite(CRE, 1);
  digitalWrite(CWE, 1);
  digitalWrite(RSE, 1);

    digitalWrite(CLK, 0);
  digitalWrite(MAL, 0);

  writeData(0x1010);
  digitalWrite(MAL, 1);
  
  digitalWrite(CLK, 1);

  delay(1);

  digitalWrite(CLK, 0);

  SETMODE(INPUT);

  digitalWrite(CRE, 0);

  Serial.println(readData());
}

void loop() {}
