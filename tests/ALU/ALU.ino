// =============================================================
//  74LS181 ALU Full Test Suite
//  16-bit wide (4x chips in cascade)
//  Pinout matches original barebones sketch
// =============================================================

// ---- Pin Definitions (unchanged from original) --------------
#define CLK  A6
#define nOE  A2
#define Cout A3   // Carry-out from MSB chip (input to Arduino)
#define S2   51
#define S1   49
#define S0   52
#define S3   53
#define M    48
#define nCin 50   // Active-LOW carry-in
#define INB  A4
#define INA  A5

// ---- Test Bookkeeping ---------------------------------------
static int gPassed = 0;
static int gFailed  = 0;

// =============================================================
//  Low-level I/O  (preserved from original sketch)
// =============================================================

void writeB(uint16_t val) {
  for (int i = 2; i <= 17; i++)
    digitalWrite(i, (val >> (i - 2)) & 1);
}

void writeA(uint16_t val) {
  for (int i = 18; i <= 31; i++)
    digitalWrite(i, (val >> (i - 18)) & 1);
  digitalWrite(A0, (val >> 14) & 1);
  digitalWrite(A1, (val >> 15) & 1);
}

uint16_t readResult() {
  uint16_t val = 0;
  for (int i = 32; i <= 47; i++)
    val |= ((uint16_t)digitalRead(i) << (i - 32));
  return val;
}

// =============================================================
//  Control helpers
// =============================================================

// s = 4-bit value  S3..S0
void setFunction(uint8_t s) {
  digitalWrite(S0, (s >> 0) & 1);
  digitalWrite(S1, (s >> 1) & 1);
  digitalWrite(S2, (s >> 2) & 1);
  digitalWrite(S3, (s >> 3) & 1);
}

// mode: HIGH = logic, LOW = arithmetic
void setMode(uint8_t mode) { digitalWrite(M, mode); }

// cin: true = carry present (drives nCin LOW)
//      false = no carry     (drives nCin HIGH)
void setCarryIn(bool cin) { digitalWrite(nCin, cin ? LOW : HIGH); }

// Pulse clock and capture output
uint16_t compute() {
  digitalWrite(CLK, HIGH);
  delayMicroseconds(200);          // give chips time to settle
  uint16_t result = readResult();
  bool     cout   = digitalRead(Cout);
  digitalWrite(CLK, LOW);
  delayMicroseconds(100);
  return result;
}

bool readCout() { return digitalRead(Cout) == LOW; }

// =============================================================
//  Test runner
// =============================================================

// Optionally check carry-out.  Pass checkCout=false to skip.
void runTest(const char* name,
             uint16_t a, uint16_t b,
             uint8_t func, bool logicMode, bool carryIn,
             uint16_t expected,
             bool checkCout = false, bool expectedCout = false)
{
  writeA(a);
  writeB(b);
  setFunction(func);
  setMode(logicMode ? HIGH : LOW);
  setCarryIn(carryIn);

  uint16_t result = compute();
  bool     cout   = readCout();

  bool pass = (result == expected);
  if (checkCout) pass &= (cout == expectedCout);

  if (pass) {
    gPassed++;
    Serial.print(F("  PASS  "));
    Serial.println(name);
  } else {
    gFailed++;
    Serial.print(F("  FAIL  "));
    Serial.print(name);
    Serial.print(F("  A=0x")); Serial.print(a, HEX);
    Serial.print(F(" B=0x")); Serial.print(b, HEX);
    Serial.print(F(" -> exp=0x")); Serial.print(expected, HEX);
    Serial.print(F(" got=0x")); Serial.print(result, HEX);
    if (checkCout) {
      Serial.print(F(" Cout_exp=")); Serial.print(expectedCout);
      Serial.print(F(" Cout_got=")); Serial.print(cout);
    }
    Serial.println();
  }
}

void printSectionHeader(const char* title) {
  Serial.println();
  Serial.print(F("[ "));
  Serial.print(title);
  Serial.println(F(" ]"));
}

// =============================================================
//  setup()
// =============================================================
void setup() {
  // ---- Pin modes (unchanged from original) ------------------
  pinMode(CLK,  OUTPUT);
  pinMode(nOE,  OUTPUT);
  pinMode(Cout, INPUT);
  pinMode(S0,   OUTPUT);
  pinMode(S1,   OUTPUT);
  pinMode(S2,   OUTPUT);
  pinMode(S3,   OUTPUT);
  pinMode(M,    OUTPUT);
  pinMode(nCin, OUTPUT);
  pinMode(INA,  OUTPUT);
  pinMode(INB,  OUTPUT);
  pinMode(A0,   OUTPUT);
  pinMode(A1,   OUTPUT);

  for (int i = 2;  i <= 31; i++) pinMode(i, OUTPUT);
  for (int i = 32; i <= 47; i++) pinMode(i, INPUT);

  // ---- Initial state ----------------------------------------
  digitalWrite(nOE, LOW);   // enable outputs
  digitalWrite(INA, HIGH);
  digitalWrite(INB, HIGH);
  digitalWrite(CLK, LOW);

  Serial.begin(9600);
  while (!Serial);

  Serial.println(F("================================================"));
  Serial.println(F("  74LS181  16-bit Full Test Suite"));
  Serial.println(F("================================================"));

  // ===========================================================
  //  SECTION 1 – LOGIC OPERATIONS  (M = HIGH)
  //
  //  The 74LS181 logic functions are purely bitwise;
  //  carry-in has no effect.
  //
  //  S   Function
  //  0000  ~A
  //  0001  ~(A | B)   NOR
  //  0010  ~A & B
  //  0011  0
  //  0100  ~(A & B)   NAND
  //  0101  ~B
  //  0110  A ^ B      XOR
  //  0111  A & ~B
  //  1000  ~A | B
  //  1001  ~(A ^ B)   XNOR
  //  1010  B
  //  1011  A & B      AND
  //  1100  1 (all-ones)
  //  1101  A | ~B
  //  1110  A | B      OR
  //  1111  A
  // ===========================================================

  printSectionHeader("Logic: NOT A  (S=0000, M=1)");
  runTest("~0x0000",         0x0000, 0x0000, 0b0000, true, false, 0xFFFF);
  runTest("~0xFFFF",         0xFFFF, 0x0000, 0b0000, true, false, 0x0000);
  runTest("~0xAAAA",         0xAAAA, 0x0000, 0b0000, true, false, 0x5555);
  runTest("~0x5555",         0x5555, 0x0000, 0b0000, true, false, 0xAAAA);
  runTest("~0x1234",         0x1234, 0x0000, 0b0000, true, false, 0xEDCB);

  printSectionHeader("Logic: NOR  (S=0001, M=1)");
  runTest("NOR 0x0000,0x0000", 0x0000, 0x0000, 0b0001, true, false, 0xFFFF);
  runTest("NOR 0xFFFF,0x0000", 0xFFFF, 0x0000, 0b0001, true, false, 0x0000);
  runTest("NOR 0x0000,0xFFFF", 0x0000, 0xFFFF, 0b0001, true, false, 0x0000);
  runTest("NOR 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b0001, true, false, 0x0000);
  runTest("NOR 0xAAAA,0xAAAA", 0xAAAA, 0xAAAA, 0b0001, true, false, 0x5555);

  printSectionHeader("Logic: ~A & B  (S=0010, M=1)");
  runTest("~A&B 0x0000,0x0000", 0x0000, 0x0000, 0b0010, true, false, 0x0000);
  runTest("~A&B 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b0010, true, false, 0x0000);
  runTest("~A&B 0x0000,0xFFFF", 0x0000, 0xFFFF, 0b0010, true, false, 0xFFFF);
  runTest("~A&B 0xFFFF,0x0000", 0xFFFF, 0x0000, 0b0010, true, false, 0x0000);
  runTest("~A&B 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b0010, true, false, 0x5555);
  runTest("~A&B 0x5555,0xAAAA", 0x5555, 0xAAAA, 0b0010, true, false, 0xAAAA);

  printSectionHeader("Logic: ZERO  (S=0011, M=1)");
  runTest("ZERO any A,B",   0x0000, 0x0000, 0b0011, true, false, 0x0000);
  runTest("ZERO any A,B",   0xFFFF, 0xFFFF, 0b0011, true, false, 0x0000);
  runTest("ZERO any A,B",   0xAAAA, 0x5555, 0b0011, true, false, 0x0000);

  printSectionHeader("Logic: NAND  (S=0100, M=1)");
  runTest("NAND 0x0000,0x0000", 0x0000, 0x0000, 0b0100, true, false, 0xFFFF);
  runTest("NAND 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b0100, true, false, 0x0000);
  runTest("NAND 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b0100, true, false, 0xFFFF);
  runTest("NAND 0xAAAA,0xFFFF", 0xAAAA, 0xFFFF, 0b0100, true, false, 0x5555);
  runTest("NAND 0x1234,0xFFFF", 0x1234, 0xFFFF, 0b0100, true, false, 0xEDCB);

  printSectionHeader("Logic: NOT B  (S=0101, M=1)");
  runTest("~B 0x0000",         0x0000, 0x0000, 0b0101, true, false, 0xFFFF);
  runTest("~B 0xFFFF",         0x0000, 0xFFFF, 0b0101, true, false, 0x0000);
  runTest("~B 0x5555",         0x0000, 0x5555, 0b0101, true, false, 0xAAAA);
  runTest("~B 0xAAAA",         0x0000, 0xAAAA, 0b0101, true, false, 0x5555);

  printSectionHeader("Logic: XOR  (S=0110, M=1)");
  runTest("XOR 0x0000^0x0000", 0x0000, 0x0000, 0b0110, true, false, 0x0000);
  runTest("XOR 0xFFFF^0xFFFF", 0xFFFF, 0xFFFF, 0b0110, true, false, 0x0000);
  runTest("XOR 0xFFFF^0x0000", 0xFFFF, 0x0000, 0b0110, true, false, 0xFFFF);
  runTest("XOR 0xAAAA^0x5555", 0xAAAA, 0x5555, 0b0110, true, false, 0xFFFF);
  runTest("XOR 0xAAAA^0xAAAA", 0xAAAA, 0xAAAA, 0b0110, true, false, 0x0000);
  runTest("XOR 0x1234^0xABCD", 0x1234, 0xABCD, 0b0110, true, false, 0xB9F9);

  printSectionHeader("Logic: A & ~B  (S=0111, M=1)");
  runTest("A&~B 0x0000,0x0000", 0x0000, 0x0000, 0b0111, true, false, 0x0000);
  runTest("A&~B 0xFFFF,0x0000", 0xFFFF, 0x0000, 0b0111, true, false, 0xFFFF);
  runTest("A&~B 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b0111, true, false, 0x0000);
  runTest("A&~B 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b0111, true, false, 0xAAAA);
  runTest("A&~B 0x5555,0xAAAA", 0x5555, 0xAAAA, 0b0111, true, false, 0x5555);

  printSectionHeader("Logic: ~A | B  (S=1000, M=1)");
  runTest("~A|B 0x0000,0x0000", 0x0000, 0x0000, 0b1000, true, false, 0xFFFF);
  runTest("~A|B 0xFFFF,0x0000", 0xFFFF, 0x0000, 0b1000, true, false, 0x0000);
  runTest("~A|B 0x0000,0xFFFF", 0x0000, 0xFFFF, 0b1000, true, false, 0xFFFF);
  runTest("~A|B 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b1000, true, false, 0x5555);
  runTest("~A|B 0x5555,0xAAAA", 0x5555, 0xAAAA, 0b1000, true, false, 0xAAAA);

  printSectionHeader("Logic: XNOR  (S=1001, M=1)");
  runTest("XNOR 0x0000,0x0000", 0x0000, 0x0000, 0b1001, true, false, 0xFFFF);
  runTest("XNOR 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b1001, true, false, 0xFFFF);
  runTest("XNOR 0xFFFF,0x0000", 0xFFFF, 0x0000, 0b1001, true, false, 0x0000);
  runTest("XNOR 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b1001, true, false, 0x0000);
  runTest("XNOR 0x1234,0xABCD", 0x1234, 0xABCD, 0b1001, true, false, 0x4606);

  printSectionHeader("Logic: TRANSFER B  (S=1010, M=1)");
  runTest("B=0x0000",          0xFFFF, 0x0000, 0b1010, true, false, 0x0000);
  runTest("B=0xFFFF",          0x0000, 0xFFFF, 0b1010, true, false, 0xFFFF);
  runTest("B=0x5555",          0xAAAA, 0x5555, 0b1010, true, false, 0x5555);
  runTest("B=0x1234",          0xDEAD, 0x1234, 0b1010, true, false, 0x1234);

  printSectionHeader("Logic: AND  (S=1011, M=1)");
  runTest("AND 0x0000,0x0000", 0x0000, 0x0000, 0b1011, true, false, 0x0000);
  runTest("AND 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b1011, true, false, 0xFFFF);
  runTest("AND 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b1011, true, false, 0x0000);
  runTest("AND 0xAAAA,0xFFFF", 0xAAAA, 0xFFFF, 0b1011, true, false, 0xAAAA);
  runTest("AND 0x1234,0xFF00", 0x1234, 0xFF00, 0b1011, true, false, 0x1200);

  printSectionHeader("Logic: ALL-ONES  (S=1100, M=1)");
  runTest("ALL-ONES any A,B",  0x0000, 0x0000, 0b1100, true, false, 0xFFFF);
  runTest("ALL-ONES any A,B",  0xFFFF, 0xFFFF, 0b1100, true, false, 0xFFFF);
  runTest("ALL-ONES any A,B",  0xAAAA, 0x5555, 0b1100, true, false, 0xFFFF);

  printSectionHeader("Logic: A | ~B  (S=1101, M=1)");
  runTest("A|~B 0x0000,0x0000", 0x0000, 0x0000, 0b1101, true, false, 0xFFFF);
  runTest("A|~B 0xFFFF,0xFFFF", 0xFFFF, 0xFFFF, 0b1101, true, false, 0xFFFF);
  runTest("A|~B 0x0000,0xFFFF", 0x0000, 0xFFFF, 0b1101, true, false, 0x0000);
  runTest("A|~B 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b1101, true, false, 0xAAAA);
  runTest("A|~B 0x5555,0xAAAA", 0x5555, 0xAAAA, 0b1101, true, false, 0x5555);

  printSectionHeader("Logic: OR  (S=1110, M=1)");
  runTest("OR 0x0000,0x0000",  0x0000, 0x0000, 0b1110, true, false, 0x0000);
  runTest("OR 0xFFFF,0x0000",  0xFFFF, 0x0000, 0b1110, true, false, 0xFFFF);
  runTest("OR 0xAAAA,0x5555",  0xAAAA, 0x5555, 0b1110, true, false, 0xFFFF);
  runTest("OR 0xAAAA,0xAAAA",  0xAAAA, 0xAAAA, 0b1110, true, false, 0xAAAA);
  runTest("OR 0x1200,0x0034",  0x1200, 0x0034, 0b1110, true, false, 0x1234);

  printSectionHeader("Logic: TRANSFER A  (S=1111, M=1)");
  runTest("A=0x0000",          0x0000, 0xFFFF, 0b1111, true, false, 0x0000);
  runTest("A=0xFFFF",          0xFFFF, 0x0000, 0b1111, true, false, 0xFFFF);
  runTest("A=0x5555",          0x5555, 0xAAAA, 0b1111, true, false, 0x5555);
  runTest("A=0x1234",          0x1234, 0xDEAD, 0b1111, true, false, 0x1234);

  // ===========================================================
  //  SECTION 2 – ARITHMETIC OPERATIONS  (M = LOW)
  //
  //  Carry convention:
  //    nCin = HIGH  →  Cn = 0  (no carry-in, carryIn=false)
  //    nCin = LOW   →  Cn = 1  (carry-in,    carryIn=true)
  //
  //  S      Cn=0 (no carry)          Cn=1 (carry)
  //  0000   F = A                    F = A + 1
  //  0001   F = A | B                F = (A|B) + 1
  //  0010   F = A | ~B               F = (A|~B) + 1
  //  0011   F = 0xFFFF (-1)          F = 0
  //  0100   F = A + (A & ~B)         F = A + (A & ~B) + 1
  //  0101   F = (A|B)+(A&~B)         F = (A|B)+(A&~B) + 1
  //  0110   F = A - B - 1            F = A - B      ← SUBTRACT
  //  0111   F = (A & ~B) - 1         F = A & ~B
  //  1000   F = A + (A & B)          F = A+(A&B)+1
  //  1001   F = A + B                F = A + B + 1  ← ADD
  //  1010   F = (A|~B)+(A&B)         F = (A|~B)+(A&B)+1
  //  1011   F = (A & B) - 1          F = A & B
  //  1100   F = A + A  (shift left)  F = A+A+1
  //  1101   F = (A|B) + A            F = (A|B)+A+1
  //  1110   F = (A|~B) + A           F = (A|~B)+A+1
  //  1111   F = A - 1  (decrement)   F = A
  // ===========================================================

  printSectionHeader("Arith: TRANSFER A  (S=0000, Cn=0)");
  runTest("XFER A 0x0000",     0x0000, 0x0000, 0b0000, false, false, 0x0000);
  runTest("XFER A 0x5555",     0x5555, 0xDEAD, 0b0000, false, false, 0x5555);
  runTest("XFER A 0xFFFF",     0xFFFF, 0x0000, 0b0000, false, false, 0xFFFF);
  runTest("XFER A 0x1234",     0x1234, 0xABCD, 0b0000, false, false, 0x1234);

  printSectionHeader("Arith: INCREMENT A  (S=0000, Cn=1)  F = A+1");
  runTest("INC 0x0000",        0x0000, 0x0000, 0b0000, false, true,  0x0001);
  runTest("INC 0x0001",        0x0001, 0x0000, 0b0000, false, true,  0x0002);
  runTest("INC 0x00FF",        0x00FF, 0x0000, 0b0000, false, true,  0x0100);
  runTest("INC 0x7FFF",        0x7FFF, 0x0000, 0b0000, false, true,  0x8000);
  runTest("INC 0xFFFE",        0xFFFE, 0x0000, 0b0000, false, true,  0xFFFF);
  runTest("INC 0xFFFF wraps",  0xFFFF, 0x0000, 0b0000, false, true,  0x0000,
          true, true);   // expect Cout HIGH on wrap

  printSectionHeader("Arith: A | B  (S=0001, Cn=0)");
  runTest("A|B 0x0000,0x0000", 0x0000, 0x0000, 0b0001, false, false, 0x0000);
  runTest("A|B 0xAAAA,0x5555", 0xAAAA, 0x5555, 0b0001, false, false, 0xFFFF);
  runTest("A|B 0x1200,0x0034", 0x1200, 0x0034, 0b0001, false, false, 0x1234);

  printSectionHeader("Arith: A | ~B  (S=0010, Cn=0)");
  runTest("A|~B 0xFFFF,0xFFFF",0xFFFF, 0xFFFF, 0b0010, false, false, 0xFFFF);
  runTest("A|~B 0x0000,0xFFFF",0x0000, 0xFFFF, 0b0010, false, false, 0x0000);
  runTest("A|~B 0x0000,0x0000",0x0000, 0x0000, 0b0010, false, false, 0xFFFF);
  runTest("A|~B 0xAAAA,0x5555",0xAAAA, 0x5555, 0b0010, false, false, 0xAAAA);

  printSectionHeader("Arith: MINUS ONE  (S=0011, Cn=0)  F = 0xFFFF");
  runTest("MINUS_ONE any",     0x0000, 0x0000, 0b0011, false, false, 0xFFFF);
  runTest("MINUS_ONE any",     0xAAAA, 0x5555, 0b0011, false, false, 0xFFFF);

  printSectionHeader("Arith: ZERO  (S=0011, Cn=1)  F = 0");
  runTest("ZERO any",          0x0000, 0x0000, 0b0011, false, true,  0x0000);
  runTest("ZERO any",          0xFFFF, 0xFFFF, 0b0011, false, true,  0x0000);

  printSectionHeader("Arith: ADD  (S=1001, Cn=0)  F = A + B");
  runTest("ADD 0+0",           0x0000, 0x0000, 0b1001, false, false, 0x0000,
          true, false);
  runTest("ADD 1+1",           0x0001, 0x0001, 0b1001, false, false, 0x0002);
  runTest("ADD 0x00AA+0x0055", 0x00AA, 0x0055, 0b1001, false, false, 0x00FF);
  runTest("ADD 0xAAAA+0x5555", 0xAAAA, 0x5555, 0b1001, false, false, 0xFFFF,
          true, false);
  runTest("ADD 0x1234+0xABCD", 0x1234, 0xABCD, 0b1001, false, false, 0xBE01);
  runTest("ADD 0x7FFF+0x0001", 0x7FFF, 0x0001, 0b1001, false, false, 0x8000);
  runTest("ADD carry 0xFFFF+1",0xFFFF, 0x0001, 0b1001, false, false, 0x0000,
          true, true);   // Cout should be HIGH
  runTest("ADD carry FFFF+FFFF",0xFFFF,0xFFFF, 0b1001, false, false, 0xFFFE,
          true, true);

  printSectionHeader("Arith: ADD+1  (S=1001, Cn=1)  F = A + B + 1");
  runTest("ADD+1 0+0+1",       0x0000, 0x0000, 0b1001, false, true,  0x0001);
  runTest("ADD+1 1+1+1",       0x0001, 0x0001, 0b1001, false, true,  0x0003);
  runTest("ADD+1 FFFE+1+1",    0xFFFE, 0x0001, 0b1001, false, true,  0x0000,
          true, true);

  printSectionHeader("Arith: SUBTRACT  (S=0110, Cn=1)  F = A - B");
  // A - B  =  A + ~B + 1  (two's complement), enabled by Cn=1
  runTest("SUB 0-0",           0x0000, 0x0000, 0b0110, false, true,  0x0000,
          true, true);   // borrow = Cout HIGH when no underflow
  runTest("SUB 5-3",           0x0005, 0x0003, 0b0110, false, true,  0x0002,
          true, true);
  runTest("SUB 0x00FF-0x0001", 0x00FF, 0x0001, 0b0110, false, true,  0x00FE);
  runTest("SUB 0x1000-0x0001", 0x1000, 0x0001, 0b0110, false, true,  0x0FFF);
  runTest("SUB 0xAAAA-0xAAAA", 0xAAAA, 0xAAAA, 0b0110, false, true,  0x0000,
          true, true);
  runTest("SUB 0xFFFF-0x0001", 0xFFFF, 0x0001, 0b0110, false, true,  0xFFFE);
  runTest("SUB underflow 0-1", 0x0000, 0x0001, 0b0110, false, true,  0xFFFF,
          true, false);  // Cout LOW = borrow occurred

  printSectionHeader("Arith: SUBTRACT-1  (S=0110, Cn=0)  F = A - B - 1");
  runTest("SUB-1 5-3-1",       0x0005, 0x0003, 0b0110, false, false, 0x0001);
  runTest("SUB-1 AAAA-AAAA-1", 0xAAAA, 0xAAAA, 0b0110, false, false, 0xFFFF);
  runTest("SUB-1 0001-0001-1", 0x0001, 0x0001, 0b0110, false, false, 0xFFFF);

  printSectionHeader("Arith: DECREMENT A  (S=1111, Cn=0)  F = A - 1");
  runTest("DEC 0x0001",        0x0001, 0x0000, 0b1111, false, false, 0x0000);
  runTest("DEC 0x0100",        0x0100, 0x0000, 0b1111, false, false, 0x00FF);
  runTest("DEC 0xFFFF",        0xFFFF, 0x0000, 0b1111, false, false, 0xFFFE);
  runTest("DEC 0x8000",        0x8000, 0x0000, 0b1111, false, false, 0x7FFF);
  runTest("DEC 0x0000 wraps",  0x0000, 0x0000, 0b1111, false, false, 0xFFFF);

  printSectionHeader("Arith: TRANSFER A (Cn=1)  (S=1111, Cn=1)  F = A");
  runTest("XFER_Cn1 A=0x0000", 0x0000, 0xDEAD, 0b1111, false, true,  0x0000);
  runTest("XFER_Cn1 A=0xAAAA", 0xAAAA, 0x5555, 0b1111, false, true,  0xAAAA);
  runTest("XFER_Cn1 A=0xFFFF", 0xFFFF, 0x0000, 0b1111, false, true,  0xFFFF);

  printSectionHeader("Arith: SHIFT-LEFT / DOUBLE  (S=1100, Cn=0)  F = A + A");
  runTest("SHL 0x0001",        0x0001, 0x0000, 0b1100, false, false, 0x0002);
  runTest("SHL 0x0080",        0x0080, 0x0000, 0b1100, false, false, 0x0100);
  runTest("SHL 0x5555",        0x5555, 0x0000, 0b1100, false, false, 0xAAAA);
  runTest("SHL 0xAAAA (oflow)",0xAAAA, 0x0000, 0b1100, false, false, 0x5554,
          true, true);   // Cout HIGH – top bit shifted out
  runTest("SHL 0x7FFF",        0x7FFF, 0x0000, 0b1100, false, false, 0xFFFE);
  runTest("SHL 0x8000",        0x8000, 0x0000, 0b1100, false, false, 0x0000,
          true, true);

  printSectionHeader("Arith: A+(A&B)  (S=1000, Cn=0)");
  runTest("A+(A&B) 0,0",       0x0000, 0x0000, 0b1000, false, false, 0x0000);
  runTest("A+(A&B) F,F",       0xFFFF, 0xFFFF, 0b1000, false, false, 0xFFFE,
          true, true);
  runTest("A+(A&B) A,5",       0xAAAA, 0x5555, 0b1000, false, false, 0xAAAA);
  runTest("A+(A&B) A,F",       0xAAAA, 0xFFFF, 0b1000, false, false, 0x5554,
          true, true);

  printSectionHeader("Arith: (A&B)-1  (S=1011, Cn=0)");
  runTest("(A&B)-1 F,F",       0xFFFF, 0xFFFF, 0b1011, false, false, 0xFFFE);
  runTest("(A&B)-1 A,5",       0xAAAA, 0x5555, 0b1011, false, false, 0xFFFF);
  runTest("(A&B)-1 0,0",       0x0000, 0x0000, 0b1011, false, false, 0xFFFF);

  printSectionHeader("Arith: A&B  (S=1011, Cn=1)");
  runTest("A&B Cn=1 F,F",      0xFFFF, 0xFFFF, 0b1011, false, true,  0xFFFF);
  runTest("A&B Cn=1 A,5",      0xAAAA, 0x5555, 0b1011, false, true,  0x0000);
  runTest("A&B Cn=1 A,F",      0xAAAA, 0xFFFF, 0b1011, false, true,  0xAAAA);

  // ===========================================================
  //  SECTION 3 – CARRY PROPAGATION
  //  Verifies that carries ripple correctly across all 4 chips.
  // ===========================================================

  printSectionHeader("Carry Propagation (ADD, S=1001)");
  // Add 1 to boundary values that exercise inter-chip carry paths
  runTest("Carry: 0x00FF+0x0001", 0x00FF, 0x0001, 0b1001, false, false, 0x0100);
  runTest("Carry: 0x0FFF+0x0001", 0x0FFF, 0x0001, 0b1001, false, false, 0x1000);
  runTest("Carry: 0x00FF+0x00FF", 0x00FF, 0x00FF, 0b1001, false, false, 0x01FE);
  runTest("Carry: 0x0FFF+0x0001", 0x0FFF, 0x0001, 0b1001, false, false, 0x1000);
  runTest("Carry: 0xFFFE+0x0001", 0xFFFE, 0x0001, 0b1001, false, false, 0xFFFF,
          true, false);
  runTest("Carry: 0xFFFF+0x0001 wrap", 0xFFFF, 0x0001, 0b1001, false, false, 0x0000,
          true, true);
  runTest("Carry: 0x7FFF+0x7FFF", 0x7FFF, 0x7FFF, 0b1001, false, false, 0xFFFE);
  runTest("Carry: 0x8000+0x8000", 0x8000, 0x8000, 0b1001, false, false, 0x0000,
          true, true);

  printSectionHeader("Carry Propagation (SUB, S=0110, Cn=1)");
  runTest("Borrow: 0x0100-0x0001", 0x0100, 0x0001, 0b0110, false, true, 0x00FF,
          true, true);
  runTest("Borrow: 0x1000-0x0001", 0x1000, 0x0001, 0b0110, false, true, 0x0FFF,
          true, true);
  runTest("Borrow: 0x0001-0x0002 underflow", 0x0001, 0x0002, 0b0110, false, true, 0xFFFF,
          true, false);

  // ===========================================================
  //  SECTION 4 – ALTERNATING / WALKING-BIT STRESS TESTS
  // ===========================================================

  printSectionHeader("Stress: walking 1 through A (ADD B=0)");
  {
    uint16_t bit = 1;
    for (int b = 0; b < 16; b++) {
      writeA(bit);
      writeB(0x0000);
      setFunction(0b1001);  // ADD
      setMode(LOW);
      setCarryIn(false);
      uint16_t result = compute();
      bool pass = (result == bit);
      if (pass) { gPassed++; }
      else {
        gFailed++;
        Serial.print(F("  FAIL  Walking-1 bit ")); Serial.print(b);
        Serial.print(F("  exp=0x")); Serial.print(bit, HEX);
        Serial.print(F(" got=0x")); Serial.println(result, HEX);
      }
      bit <<= 1;
    }
    Serial.println(F("  (walking-1 complete)"));
  }

  printSectionHeader("Stress: walking 1 through B (ADD A=0)");
  {
    uint16_t bit = 1;
    for (int b = 0; b < 16; b++) {
      writeA(0x0000);
      writeB(bit);
      setFunction(0b1001);
      setMode(LOW);
      setCarryIn(false);
      uint16_t result = compute();
      bool pass = (result == bit);
      if (pass) { gPassed++; }
      else {
        gFailed++;
        Serial.print(F("  FAIL  Walking-1 B bit ")); Serial.print(b);
        Serial.print(F("  exp=0x")); Serial.print(bit, HEX);
        Serial.print(F(" got=0x")); Serial.println(result, HEX);
      }
      bit <<= 1;
    }
    Serial.println(F("  (walking-1 complete)"));
  }

  printSectionHeader("Stress: exhaustive 8-bit ADD (lower byte, A=0..FF, B=0x01)");
  {
    bool sectionFail = false;
    for (uint16_t a = 0; a <= 0xFF; a++) {
      writeA(a);
      writeB(0x0001);
      setFunction(0b1001);
      setMode(LOW);
      setCarryIn(false);
      uint16_t result = compute();
      uint16_t expected = (a + 1) & 0xFFFF;
      if (result != expected) {
        gFailed++;
        sectionFail = true;
        Serial.print(F("  FAIL  ADD 0x")); Serial.print(a, HEX);
        Serial.print(F("+1 exp=0x")); Serial.print(expected, HEX);
        Serial.print(F(" got=0x")); Serial.println(result, HEX);
      } else {
        gPassed++;
      }
    }
    if (!sectionFail) Serial.println(F("  PASS  All 256 values correct"));
  }

  printSectionHeader("Stress: exhaustive 8-bit SUB (lower byte, A=0..FF, B=A)");
  {
    bool sectionFail = false;
    for (uint16_t a = 0; a <= 0xFF; a++) {
      writeA(a);
      writeB(a);
      setFunction(0b0110);   // A - B
      setMode(LOW);
      setCarryIn(true);      // Cn=1 for true subtract
      uint16_t result = compute();
      if (result != 0x0000) {
        gFailed++;
        sectionFail = true;
        Serial.print(F("  FAIL  SUB self 0x")); Serial.print(a, HEX);
        Serial.print(F(" exp=0x0000 got=0x")); Serial.println(result, HEX);
      } else {
        gPassed++;
      }
    }
    if (!sectionFail) Serial.println(F("  PASS  All 256 self-subtracts = 0"));
  }

  // ===========================================================
  //  FINAL SUMMARY
  // ===========================================================
  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F("  RESULTS"));
  Serial.print(F("  Passed : ")); Serial.println(gPassed);
  Serial.print(F("  Failed : ")); Serial.println(gFailed);
  Serial.print(F("  Total  : ")); Serial.println(gPassed + gFailed);
  Serial.println(F("------------------------------------------------"));
  if (gFailed == 0)
    Serial.println(F("  ALL TESTS PASSED — chips appear healthy."));
  else
    Serial.println(F("  FAILURES DETECTED — check wiring and chips."));
  Serial.println(F("================================================"));
}

void loop() { /* nothing */ }
