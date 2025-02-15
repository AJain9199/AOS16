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

void write_cycle(uint16_t **cmd, int steps) {
  SET_OE(HIGH);

  for (int i = 0; i < steps; i++) {
    setAddr(cmd[i][0]); dataOut((uint8_t)cmd[i][1]); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  }
  delay(10);
}

void chipId() {
  write_cycle((uint16_t **)CHIP_ID_ENTRY, 3);
  setAddr(0x0);
  Serial.write(dataIn());

  setAddr(0x1);
  Serial.write(dataIn());

  write_cycle((uint16_t **) CHIP_ID_EXIT, 3);
}

void setup() {
  pinMode(CLK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(DATA, OUTPUT);

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);

  Serial.begin(9600);

  chipId();
}

void loop() {

}
