// Shift register connections
#define DATA 11
#define LATCH 8
#define CLK 12

// Control pins
#define WE 40
#define OE 41

#define SET_OE(state) digitalWrite(OE, state)
#define SET_WE(state) digitalWrite(WE, state)

#define WAIT_SINGLE() asm volatile ("nop")

const uint16_t CHIP_ID_ENTRY[][2] {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x90}
};

void setAddr(uint32_t addr) {
  DDRC = 0xff;
  PORTC = addr & 0xff;
  DDRL = 0xff;
  PORTL = (addr >> 8) & 0xff;
  DDRC |= 0b111;
  PORTC = (addr >> 16) & 0b111;
}

void dataOut(byte data) {
  DDRA = 0b11111111;
  PORTA = data;
}

byte dataIn() {
  DDRA = 0;
  return PINA;
}

void write_cycle(uint16_t addr, uint8_t data) {
  SET_OE(HIGH);
  SET_WE(HIGH);
  WAIT_SINGLE();

  setAddr(addr);
  SET_WE(LOW);
  WAIT_SINGLE();
  dataOut(data);
  SET_WE(HIGH);
  WAIT_SINGLE();
}

byte read_cycle(uint32_t addr) {
  SET_WE(HIGH);
  SET_OE(HIGH);
  WAIT_SINGLE();

  setAddr(addr);
  SET_OE(LOW);
  WAIT_SINGLE();

  byte r = dataIn();
  WAIT_SINGLE();
  WAIT_SINGLE();
  return r;
}

void chipId() {
  write_cycle(0x5555, 0xaa);
  write_cycle(0x2aaa, 0x55);
  write_cycle(0x5555, 0x90);

  delay(10);

  Serial.println(read_cycle(0x0), HEX);
}

void setup() {
  pinMode(OE, OUTPUT);
  pinMode(WE, OUTPUT);

  Serial.begin(115200);

  chipId();
}

void loop() {}