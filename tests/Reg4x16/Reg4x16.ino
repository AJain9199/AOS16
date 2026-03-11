#define nSEL A11
#define nOE A12
#define nWE A13
#define ADR1 A14
#define ADR0 A15

#define D0 A8
#define D1 A9
#define D2 A10

void writeAddr(uint8_t addr) {
  digitalWrite(ADR0, addr & 1);
  digitalWrite(ADR1, (addr >> 1) & 1);
}

void writeData(uint8_t data) {
  digitalWrite(nWE, 0);
  digitalWrite(nOE, 1);

  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);

  digitalWrite(D0, data & 1);
  digitalWrite(D1, (data >> 1) & 1);
  digitalWrite(D2, (data >> 2) & 1);
}

uint8_t readData() {
  digitalWrite(nWE, 1);
  digitalWrite(nOE, 0);
  pinMode(D0, INPUT);
  pinMode(D1, INPUT);
  pinMode(D2, INPUT);

  return (digitalRead(D2) << 2) | (digitalRead(D1) << 1) | digitalRead(D0);
}

void defState() {
  digitalWrite(nWE, 1);
  digitalWrite(nOE, 1); 
}

void setup() {
    Serial.begin(9600);
    pinMode(A15, OUTPUT);
    pinMode(A14, OUTPUT);
    pinMode(A13, OUTPUT);
    pinMode(A12, OUTPUT);
    pinMode(A11, OUTPUT);

    digitalWrite(nSEL, 0);
    writeData(1);

    for (int i = 0; i < 4; i++) {
      writeAddr(i);
      writeData(i);
      defState();
    }

    for (int i = 3; i >= 0; i--) {
      writeAddr(i);
      Serial.println(readData());
      defState();
    }

    defState();
}

void loop() {
  // put your main code here, to run repeatedly:

}
