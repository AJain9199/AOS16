// Shift register connections
#define DATA 11
#define LATCH 8
#define CLK 12

// Data pins
const int D[] = {10, 9, 7, 2, 3, 4, 5, 6};

// Control pins
#define WE A0
#define OE A1

#define SET_OE(state) digitalWrite(OE, state)
#define SET_WE(state) digitalWrite(WE, state)

#define WAIT_SINGLE() asm volatile ("nop")

const uint16_t CHIP_ID_ENTRY[][2] {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x90}
};

const uint16_t CHIP_ID_EXIT[][2] {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0xF0}
};

const uint16_t CHIP_ERASE[][2] {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x80},
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x10}
};

const uint16_t CHIP_WRITE[][2] {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0xA0}
};

void setAddr(uint16_t addr) {
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, (uint8_t)(addr >> 8));
  shiftOut(DATA, CLK, MSBFIRST, (uint8_t)(addr & 0xff));
  digitalWrite(LATCH, HIGH);
}

void dataOut(byte data) {
  // DDRD |= 0b11111100;
  // DDRB |= 0b00000110;

  for (int i = 0; i < 8; i++) {
    pinMode(D[i], OUTPUT);
    digitalWrite(D[i], data & (1 << i));
  }
}

byte dataIn() {
  // DDRD &= 0b00000011;
  // DDRB &= 0b11111001;

  read_mode();

  byte ans;
  for (int i = 0; i < 8; i++) {
    pinMode(D[i], INPUT);
    ans |= (digitalRead(D[i]) << i);
  }
  return ans;
}

void read_mode() {
  SET_OE(LOW); SET_WE(HIGH);
}

void write_cycle_step(uint16_t addr, uint8_t data) {
  setAddr(addr); dataOut(data); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
}

void write_cycle(uint16_t **cmd, int steps) {
  SET_OE(HIGH);

  for (int i = 0; i < steps; i++) {
    write_cycle_step(cmd[i][0], cmd[i][1]);
  }
}

void chipId() {
  write_cycle((uint16_t **)CHIP_ID_ENTRY, 3);
  delay(5);
  setAddr(0x0);
  Serial.write(dataIn());

  setAddr(0x1);
  Serial.write(dataIn());

  write_cycle((uint16_t **) CHIP_ID_EXIT, 3);
}

void chipErase() {
  write_cycle((uint16_t **) CHIP_ERASE, 6);
  SET_OE(LOW);
  int c = 0; while ((dataIn() & 128) != 128 && c < 2000) { c++; delayMicroseconds(100); }
  SET_OE(HIGH);
  if (c < 2000) {
  Serial.write(0xFF);
  }
}

void chipWrite(uint16_t addr, uint8_t data) {
  SET_WE(HIGH);
  SET_OE(HIGH);
  setAddr(0x5555); dataOut(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  setAddr(0x2aaa); dataOut(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  setAddr(0x5555); dataOut(0xa0); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  setAddr(addr); dataOut(data); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  read_mode();

  SET_OE(LOW);
  int c = 0; while (((dataIn()&128) != (data&128)) && (c < 100)) c++;
  SET_OE(HIGH);
  if (c < 100) {
    Serial.write(data);
  } else {
    Serial.write(~data);
  }
}

void setup() {
  pinMode(CLK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(DATA, OUTPUT);

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  if (Serial.available() <= 0) {
    return;
  }

  switch (Serial.read()) {
    case 'c':
      chipId();
      break;
    case 'e':
      chipErase();
      return;
    case 'w': {
      delay(10);
      byte buf[3] = {0};
      Serial.readBytes(buf, 2);
      uint16_t len = (buf[0] << 8) | buf[1];
      for (int i = 0; i < len; i++) {
        Serial.readBytes(buf,3);
        uint16_t addr = (buf[0] << 8) | buf[1];
        uint8_t data = buf[2];

        chipWrite(addr, data);
      }
      
      return;
    }

    case 'r':
    read_mode();
      setAddr(0);
      delay(10);
      Serial.write(dataIn());
      break;
  }
}
