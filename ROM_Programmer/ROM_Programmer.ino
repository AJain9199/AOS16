/*
 * PORTD: 21, 20, 19, 18, --, --, --, 38
 * PORTE: 00, 01, --, 05, 02, 03, --, --
 * PORTH: 17, 16, --, 06, 07, 08, 09, --
 * PORTI: --, --, --, --, --, --, --, --
 * PORTC: 37, 36, 35, 34, 33, 32, 31, 30
 * PORTA: 22, 23, 24, 25, 26, 27, 28, 29
 * PORTF: A0, A1, A2, A3, A4, A5, A6, A7
 * PORTK: A8, A9, AA, AB, AC, AD, AE, AF
 * PORTB: 53, 52, 51, 50, 10, 11, 12, 13
 * PORTL: 49, 48, 47, 46, 45, 44, 43, 42
 * PORTG: 41, 40, 39, --, --, --, --, --
 *
 *
 */

#define DDR_ADDR_L   DDRA
#define DDR_ADDR_M   DDRC
#define DDR_ADDR_H   DDRG

#define DDR_DATA_L   DDRL

#define PORT_ADDR_L  PORTA
#define PORT_ADDR_M  PORTC
#define PORT_ADDR_H  PORTG

#define PORT_DATA_L  PORTL
#define PINP_DATA_L  PINL

///////////////////////////////////

#define DDR_WE       DDRD
#define PORT_WE      PORTD
#define MASK_WE      0x80u

#define DDR_OE       DDRB
#define PORT_OE      PORTB
#define MASK_OE      0x01u

//////////////////////////////

#define DATA_DIR_OUT    1
#define DATA_DIR_INP    0

#define WAIT_SINGLE() asm volatile ("nop")

#define WAIT_TCE() delay(100)
#define WAIT_TSE() delay(25)
#define WAIT_BYP() delayMicroseconds(20)

typedef struct {
  uint32_t addr;
  uint8_t data;
} cmd_step;

cmd_step CHIP_ID_ENTRY[] = {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x90}
};

cmd_step CHIP_ID_EXIT[] = {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0xF0}
};

cmd_step CHIP_ERASE[]  = {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x80},
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0x10},
};

cmd_step CHIP_WRITE[] = {
  {0x5555, 0xAA},
  {0x2AAA, 0x55},
  {0x5555, 0xA0}
};


void programmer_init()
{
  /* ADDRESS# + CE# */
  DDR_ADDR_L = 0xffu;
  DDR_ADDR_M = 0xffu;
  DDR_ADDR_H = 0x07u;

  /* DATA# */
  DDR_DATA_L = 0xffu;

  /* OE# , WE# */
  DDR_WE = MASK_WE;
  DDR_OE = MASK_OE;

  PORT_WE |= MASK_WE;
  PORT_OE |= MASK_OE;
}

void set_addr(uint32_t addr)
{
  addr &= 0x7FFFFu;
  PORT_ADDR_L = ((addr >>  0u) & 0xffu);
  PORT_ADDR_M = ((addr >>  8u) & 0xffu);
  PORT_ADDR_H = ((addr >> 16u) & 0x07u);
}

void out_data(uint8_t data)
{
  DDR_DATA_L = 0xFFu;
  WAIT_SINGLE();
  WAIT_SINGLE();
  PORT_DATA_L = data;
}

uint8_t inp_data()
{
  DDR_DATA_L = 0x00u;
  PORT_DATA_L = 0x00u;
  WAIT_SINGLE();
  WAIT_SINGLE();
  return PINP_DATA_L;
}

void set_we(bool b)
{
  if (!b) PORT_WE |= MASK_WE;
  else PORT_WE &= ~MASK_WE;
}

void set_oe(bool b)
{
  if (!b) PORT_OE |= MASK_OE;
  else PORT_OE &= ~MASK_OE;
}

void write_cycle(uint32_t addr, uint8_t data)
{
  set_oe(0);
  set_we(0);
  WAIT_SINGLE();
  set_addr(addr);
  set_we(1);
  WAIT_SINGLE();
  out_data(data);
  set_we(0);
  WAIT_SINGLE();
}

uint8_t read_cycle(uint32_t addr)
{
  set_we(0);
  set_oe(1);
  WAIT_SINGLE();
  set_addr(addr);
  WAIT_SINGLE();
  uint8_t r = inp_data();
  return r;
}

void writeCmd(cmd_step cmd[], int steps) {
  for (int i = 0; i < steps; i++) {
    write_cycle(cmd[i].addr, cmd[i].data);
  }
  WAIT_SINGLE();
}

uint16_t chipId() {
  writeCmd(CHIP_ID_ENTRY, 3);
  uint8_t a = read_cycle(0x0000u);
  uint8_t b = read_cycle(0x0001u);

  writeCmd(CHIP_ID_EXIT, 3);
  return (a << 8) | b;
}

uint8_t read8() {
  while (Serial.available() <= 0) {
    continue;
  }
  return Serial.read();
}

uint32_t read32() {
  return (read8() << 24) | (read8() << 16) | (read8() << 8) | read8();
}

void chip_erase() {
  writeCmd(CHIP_ERASE, 6);
  WAIT_TCE();
}

void program_byte(uint32_t addr, uint8_t data)
{
  for (int i = 0; i < 3; i++) {
    write_cycle(CHIP_WRITE[i].addr, CHIP_WRITE[i].data);
  }
  write_cycle(addr, data);
  WAIT_BYP();
}

uint32_t parse32(uint8_t *data) {
  return ((*data) << 24) | ((*(data + 1)) << 16) | ((*(data+2)) << 8) | (*(data+3));
}

#define CHUNK_SIZE 32
static uint8_t chunk[CHUNK_SIZE];

uint32_t write_seq(uint8_t *buf, uint32_t size, uint32_t start) {
  for (int i = 0; i < size; i++) {
    program_byte(start+i, buf[i]);
  }
  return start + size;
}

void setup() {
  programmer_init();
  Serial.begin(115200);

  writeCmd(CHIP_ID_EXIT, 3);
  inp_data();
  chipId();
}

void loop() {
  if (Serial.available() == 0) {
    return;
  }

  uint8_t cmd = Serial.read();

  if (cmd == 'c') {
      uint16_t id = chipId();
      Serial.write(id >> 8);
      Serial.write(id & 0xff);
      return;
  } else if (cmd == 'e') {
    chip_erase();
    Serial.write(0xff);
  } else if (cmd == 'w') {
      WAIT_SINGLE();

      while (Serial.available() < 5) {
        continue;
      }

      uint8_t buf[8];
      Serial.readBytes(buf, 8);

      uint32_t len = parse32(buf);
      uint32_t initial = parse32(buf + 4);
      uint32_t addr = 0;

      do {
        uint32_t p = 0;
        long lm = millis();

        uint8_t *ptr = chunk;

        do {
          if (Serial.available() > 0) {
            int nbytes = Serial.available();
            Serial.readBytes(ptr, nbytes);
            ptr += nbytes;
            p += nbytes;
            lm = millis();
          }
        } while (p < CHUNK_SIZE && (millis() - lm) < 1000);

        if (p > 0) {
          for (int i = 0; i < p; i++) {
            program_byte(initial+addr, chunk[i]);
            addr++;
          }
        }
      } while (addr < len);

      Serial.write((len >> 24) & 0xff);
      Serial.write((len >> 16) & 0xff);
      Serial.write((len >> 8) & 0xff);
      Serial.write(len & 0xff);
  } else if (cmd == 'r') {
    uint8_t buf[8];
    Serial.readBytes(buf, 8);


    uint32_t len = parse32(buf);
    uint32_t initial = parse32(buf +4);

    set_oe(true);
    set_we(false);
    for (uint32_t i = 0; i < len; i++) {
      set_addr(initial+i);
      WAIT_SINGLE();

      Serial.write(inp_data());
      WAIT_SINGLE();
    }
  }
}
