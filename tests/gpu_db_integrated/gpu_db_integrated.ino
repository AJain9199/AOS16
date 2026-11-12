#include "fastbus.h"
#include <SoftwareWire.h>
#include "si5351_freq.h"
#include "cpu_fb.h"
#include "demos.h"

SoftwareWire i2cbus(46, 47);

// Serial controls (115200):
//   o / O  next / previous colour order (RGB RBG GRB GBR BRG BGR)
//   s      slide the 15-bit colour field up one bit (bits 15..1 instead of 14..0)
//   n / p  next / previous demo        0-8  jump to demo
//   a      toggle auto-cycling         space  pause/resume animation
//   h      help
static uint8_t  g_demo = 0;
static bool     g_auto = true, g_paused = false;
static uint32_t g_demoStart = 0, g_fpsStart = 0, g_frames = 0;

static void printMode() {
  Serial.print(F("order=")); Serial.print(ORDER_NAMES[g_order]);
  Serial.print(F(" shift=")); Serial.print(g_shift);
  Serial.print(F("  demo ")); Serial.print(g_demo); Serial.print(F(": "));
  Serial.println(DEMOS[g_demo].name);
}

static void setDemo(uint8_t d) {
  g_demo = d % NDEMOS;
  g_demoStart = millis();
  printMode();
}

static void help() {
  Serial.println(F("o/O colour order  s shift  n/p demo  0-8 jump  a auto  space pause  h help"));
}

static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'o') { g_order = (g_order + 1) % 6; applyColorMode(); printMode(); }
    else if (c == 'O') { g_order = (g_order + 5) % 6; applyColorMode(); printMode(); }
    else if (c == 's') { g_shift ^= 1; applyColorMode(); printMode(); }
    else if (c == 'n') { g_auto = false; setDemo(g_demo + 1); }
    else if (c == 'p') { g_auto = false; setDemo(g_demo + NDEMOS - 1); }
    else if (c >= '0' && c < '0' + (int)NDEMOS) { g_auto = false; setDemo(c - '0'); }
    else if (c == 'a') { g_auto = !g_auto; Serial.println(g_auto ? F("auto on") : F("auto off")); }
    else if (c == ' ') { g_paused = !g_paused; Serial.println(g_paused ? F("paused") : F("running")); }
    else if (c == 'h' || c == '?') help();
  }
}

void setup() {
  Serial.begin(115200);
  fbInit();
  demosInit();

  i2cbus.begin();
  if (!si5351Init(i2cbus)) {
    Serial.println("Error!");
  }

  Serial.println(si5351SetFreq(i2cbus, 9000000UL));
  si5351Dump(i2cbus);
  si5351Poll(i2cbus);

  help();
  setDemo(0);
  g_fpsStart = millis();
}

// Each pass: draw the next frame into the back board, then flip SEL so the GPU
// shows it (and port 1 now points at the other board for the following frame).
void loop() {
  handleSerial();

  if (g_auto && millis() - g_demoStart > 12000UL) setDemo(g_demo + 1);
  if (!g_paused) g_t = millis() >> 4;

  renderFrame(DEMOS[g_demo]);
  fbSwap();
  g_frames++;

  uint32_t now = millis();
  if (now - g_fpsStart >= 3000UL) {
    Serial.print(F("fps ")); Serial.println((float)g_frames * 1000.0f / (now - g_fpsStart), 1);
    g_frames = 0; g_fpsStart = now;
  }
}
