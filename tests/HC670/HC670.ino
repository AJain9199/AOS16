#define latchPin 8
#define clockPin 12
#define dataPin 11

int dataIn[] = {2, 3, 4, 5, 6, 7, 9, 10};

#define ra0 A1
#define ra1 A0
#define wa0 A2
#define wa1 A3

#define nwe A5
#define nre 13

void writeWA(uint8_t a) {
  digitalWrite(wa0, a & 0b1);
  digitalWrite(wa1, a & 0b10);
}

void writeRA(uint8_t a) {
  digitalWrite(ra0, a & 0b1);
  digitalWrite(ra1, a & 0b10);
}

void writeData(uint8_t data) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, data);
  digitalWrite(latchPin, HIGH);
}

uint8_t readData() {
  uint8_t data = 0;

  for (int i = 0; i < 8; i++) {
    data |= (digitalRead(dataIn[i]) << i);
  }

  return data;
}

#define writeon digitalWrite(nwe, LOW)
#define writeoff digitalWrite(nwe, HIGH)

#define readon digitalWrite(nre, LOW)
#define readoff digitalWrite(nre, HIGH)

void setup() {
  Serial.begin(9600);

  // put your setup code here, to run once:
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  for (int i  = 0; i < 8; i++) {
    pinMode(dataIn[i], INPUT_PULLUP);
  }

  pinMode(ra0, OUTPUT);
  pinMode(ra1, OUTPUT);
  pinMode(wa0, OUTPUT);
  pinMode(wa1, OUTPUT);
  pinMode(nre, OUTPUT);
  pinMode(nwe, OUTPUT);

  bool passes = true;

  readoff;

  uint8_t zeroes = readData();
  Serial.println(zeroes);
  readon;
  if (zeroes != 0xff) {
    passes = false;
  }

  for (int i = 0; i < 4; i++) {
    writeWA(i);
    writeon;
    writeData(i);
    delay(10);
    writeoff;

    writeRA(i);
    Serial.println(readData());
  }

  for (int i = 0; i < 4; i++) {
    readon;
    delay(5);
    writeRA(i);
    uint8_t data = readData();
    Serial.println(data);
    if (data != i) {
      passes = false;
    }
    readoff;
  }


  if (!passes) {
    Serial.println("Tests failed");
  } else {
    Serial.println("All passed.");
  }
}



void loop() {

}
