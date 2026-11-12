// demos.h — colour / motion test patterns for the 160x120 GPU framebuffer.
//
// Frame format: 16-bit word at address (y << 8) | x, x = 0..159, y = 0..119.
// Colour is 15-bit, 5 bits per channel, in bits 0..14 (bit 15 unused).
//
// The channel order is NOT assumed. It is a runtime setting (serial 'o'/'O' to
// step through all 6 permutations, 's' to slide the 15-bit field up one bit),
// with compile-time defaults below. Use the "test card" demo (key '0') to
// calibrate: its 6th bar must be RED, 5th MAGENTA, 4th GREEN, 7th BLUE, and the
// three ramps below the bars must be red, green, blue, grey in that order.
//
// Every demo is a (begin, row) pair: begin() runs once per frame to do the
// per-frame maths, row(y, buf) fills one scanline of 160 words. Rendering a
// row at a time keeps RAM use tiny (no full frame lives in the Arduino).
#pragma once
#include "cpu_fb.h"

#define FB_W 160
#define FB_H 120

// Defaults; override with -DCOLOR_ORDER_DEFAULT=n -DCOLOR_SHIFT_DEFAULT=1.
// Order index: 0=RGB 1=RBG 2=GRB 3=GBR 4=BRG 5=BGR (name lists MSB channel first).
#ifndef COLOR_ORDER_DEFAULT
#define COLOR_ORDER_DEFAULT 0
#endif
#ifndef COLOR_SHIFT_DEFAULT
#define COLOR_SHIFT_DEFAULT 0   // 0: channels fill bits 14..0, 1: bits 15..1
#endif

// ------------------------------------------------------------------ colour

static const char* const ORDER_NAMES[6] = {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"};
// LSB position of {R, G, B} within the 15-bit field, per order.
static const uint8_t ORDER_POS[6][3] = {
  {10, 5, 0}, {10, 0, 5}, {5, 10, 0}, {0, 10, 5}, {5, 0, 10}, {0, 5, 10}};

static uint8_t g_order = COLOR_ORDER_DEFAULT;
static uint8_t g_shift = COLOR_SHIFT_DEFAULT;
static uint8_t g_rs, g_gs, g_bs;     // final shifts for R, G, B
static uint16_t g_pal[256];          // hue wheel in the current order
static int8_t g_sin[256];            // 127*sin(2*pi*i/256)
static uint8_t g_field[512];         // metaball falloff
static uint16_t g_row[FB_W];         // scanline being built
static uint16_t g_colA[FB_W], g_colB[FB_W], g_colC[FB_W];  // per-column scratch
static uint16_t g_t;                 // animation clock, 62.5 ticks/s

// r, g, b are 0..31.
static inline uint16_t pack(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)r << g_rs) | ((uint16_t)g << g_gs) | ((uint16_t)b << g_bs);
}
static inline int8_t S(uint8_t i) { return g_sin[i]; }

static void buildPalette() {
  for (uint16_t i = 0; i < 256; i++) {
    uint8_t region = i / 43;
    uint8_t rem = (i - region * 43) * 6;   // 0..252
    uint8_t r, g, b;
    switch (region) {
      case 0:  r = 255;       g = rem;       b = 0;         break;
      case 1:  r = 255 - rem; g = 255;       b = 0;         break;
      case 2:  r = 0;         g = 255;       b = rem;       break;
      case 3:  r = 0;         g = 255 - rem; b = 255;       break;
      case 4:  r = rem;       g = 0;         b = 255;       break;
      default: r = 255;       g = 0;         b = 255 - rem; break;
    }
    g_pal[i] = pack(r >> 3, g >> 3, b >> 3);
  }
}

static void applyColorMode() {
  g_rs = ORDER_POS[g_order][0] + g_shift;
  g_gs = ORDER_POS[g_order][1] + g_shift;
  g_bs = ORDER_POS[g_order][2] + g_shift;
  buildPalette();
}

static void demosInit() {
  for (uint16_t i = 0; i < 256; i++) g_sin[i] = (int8_t)lround(127.0 * sin(i * 6.283185307 / 256.0));
  for (uint16_t i = 0; i < 512; i++) {
    uint32_t v = 4000UL / (i + 8);
    g_field[i] = v > 255 ? 255 : v;
  }
  applyColorMode();
}

// -------------------------------------------------------- 0: test card
static uint16_t g_bar[8], g_white;

static void cardBegin() {
  g_white = pack(31, 31, 31);
  g_bar[0] = g_white;          g_bar[1] = pack(31, 31, 0);
  g_bar[2] = pack(0, 31, 31);  g_bar[3] = pack(0, 31, 0);
  g_bar[4] = pack(31, 0, 31);  g_bar[5] = pack(31, 0, 0);
  g_bar[6] = pack(0, 0, 31);   g_bar[7] = 0;
}
static void cardRow(uint8_t y, uint16_t* b) {
  if (y < 60) {
    for (uint8_t x = 0; x < FB_W; x++) b[x] = g_bar[x / 20];
  } else {
    uint8_t band = (y - 60) / 15;   // 0 R, 1 G, 2 B, 3 grey
    for (uint8_t x = 0; x < FB_W; x++) {
      uint8_t v = ((uint16_t)x * 100) >> 9;   // 0..31
      b[x] = band == 0 ? pack(v, 0, 0) : band == 1 ? pack(0, v, 0)
           : band == 2 ? pack(0, 0, v) : pack(v, v, v);
    }
  }
  // 1-pixel white frame proves the full 160x120 address mapping
  if (y == 0 || y == FB_H - 1) { for (uint8_t x = 0; x < FB_W; x++) b[x] = g_white; }
  b[0] = g_white; b[FB_W - 1] = g_white;
}

// ------------------------------------------------------------ 1: plasma
static void plasmaBegin() {
  for (int16_t x = 0; x < FB_W; x++) g_colA[x] = (x - 80) * (x - 80);
}
static void plasmaRow(uint8_t y, uint16_t* b) {
  int8_t sy = S(y * 3 + g_t * 3);
  uint16_t dy2 = (y - 60) * (y - 60);
  for (uint8_t x = 0; x < FB_W; x++) {
    int16_t s = S(x * 2 + g_t * 2) + sy + S((x + y) * 2 - g_t)
              + S(((g_colA[x] + dy2) >> 5) + g_t);
    b[x] = g_pal[(uint8_t)((s >> 2) + (g_t >> 1))];
  }
}

// ----------------------------------------------------- 2: rainbow waves
static void wavesBegin() {}
static void wavesRow(uint8_t y, uint16_t* b) {
  uint8_t i0 = y + (S(y * 4 + g_t * 3) >> 2) + g_t * 2;
  for (uint8_t x = 0; x < FB_W; x++) b[x] = g_pal[(uint8_t)(i0 + x + (x >> 1))];
}

// ------------------------------------------- 3: two-source interference
static int16_t g_ay, g_by;
static void interfBegin() {
  int16_t ax = 80 + ((S(g_t * 3) * 60) >> 7);
  int16_t bx = 80 + ((S(g_t * 2 + 128) * 60) >> 7);
  g_ay = 60 + ((S(g_t * 2 + 64) * 45) >> 7);
  g_by = 60 + ((S(g_t * 3 + 32) * 45) >> 7);
  for (int16_t x = 0; x < FB_W; x++) {
    g_colA[x] = (x - ax) * (x - ax);
    g_colB[x] = (x - bx) * (x - bx);
  }
}
static void interfRow(uint8_t y, uint16_t* b) {
  uint16_t da = (y - g_ay) * (y - g_ay), db = (y - g_by) * (y - g_by);
  for (uint8_t x = 0; x < FB_W; x++) {
    uint16_t i = ((g_colA[x] + da) >> 3) + ((g_colB[x] + db) >> 3);
    b[x] = g_pal[(uint8_t)(i - g_t * 3)];
  }
}

// ------------------------------------------------------- 4: metaballs
static int16_t g_my[3];
static void ballsBegin() {
  int16_t bx[3] = {
    (int16_t)(80 + ((S(g_t * 2) * 55) >> 7)),
    (int16_t)(80 + ((S(g_t * 3 + 90) * 55) >> 7)),
    (int16_t)(80 + ((S(g_t * 5 + 200) * 55) >> 7))};
  g_my[0] = 60 + ((S(g_t * 3 + 40) * 40) >> 7);
  g_my[1] = 60 + ((S(g_t * 2 + 170) * 40) >> 7);
  g_my[2] = 60 + ((S(g_t * 4 + 10) * 40) >> 7);
  for (int16_t x = 0; x < FB_W; x += 2) {
    g_colA[x] = (x - bx[0]) * (x - bx[0]);
    g_colB[x] = (x - bx[1]) * (x - bx[1]);
    g_colC[x] = (x - bx[2]) * (x - bx[2]);
  }
}
static void ballsRow(uint8_t y, uint16_t* b) {
  uint16_t d0 = (y - g_my[0]) * (y - g_my[0]);
  uint16_t d1 = (y - g_my[1]) * (y - g_my[1]);
  uint16_t d2 = (y - g_my[2]) * (y - g_my[2]);
  uint8_t hue = g_t >> 1;
  for (uint8_t x = 0; x < FB_W; x += 2) {   // half horizontal resolution
    uint16_t r0 = (g_colA[x] + d0) >> 4; if (r0 > 511) r0 = 511;
    uint16_t r1 = (g_colB[x] + d1) >> 4; if (r1 > 511) r1 = 511;
    uint16_t r2 = (g_colC[x] + d2) >> 4; if (r2 > 511) r2 = 511;
    uint16_t s = g_field[r0] + g_field[r1] + g_field[r2];
    if (s > 255) s = 255;
    uint16_t c = s < 12 ? 0 : g_pal[(uint8_t)(s + hue)];
    b[x] = c; b[x + 1] = c;
  }
}

// -------------------------------------------------- 5: rotozoom checker
static int16_t g_dux, g_dvx, g_ou, g_ov;
static uint16_t g_chk[4];
static void rotoBegin() {
  uint8_t a = g_t >> 1;
  int16_t c = S(a + 64), s = S(a);
  uint8_t z = 2 + ((S(g_t) + 128) >> 4);       // zoom 2..17
  g_dux = (c * z) >> 2;
  g_dvx = (s * z) >> 2;
  g_ou = g_t * 40; g_ov = g_t * 25;            // scroll (wraps harmlessly)
  for (uint8_t k = 0; k < 4; k++) g_chk[k] = g_pal[(uint8_t)(g_t + k * 64)];
}
static void rotoRow(uint8_t y, uint16_t* b) {
  int16_t dy = y - 60;
  uint16_t u = g_ou - 80 * g_dux - dy * g_dvx;   // row step is (-dvx, +dux)
  uint16_t v = g_ov - 80 * g_dvx + dy * g_dux;
  for (uint8_t x = 0; x < FB_W; x++) {
    b[x] = g_chk[((u >> 11) & 1) | (((v >> 11) & 1) << 1)];
    u += g_dux; v += g_dvx;
  }
}

// ------------------------------------------ 6: bouncing boxes (OR blend)
#define NBOX 5
static const uint8_t BOX_W[NBOX]   = {40, 30, 50, 26, 36};
static const uint8_t BOX_H[NBOX]   = {30, 40, 26, 44, 34};
static const uint8_t BOX_SX[NBOX]  = {3, 5, 2, 6, 4};
static const uint8_t BOX_SY[NBOX]  = {4, 3, 5, 2, 6};
static uint8_t g_bx[NBOX], g_by0[NBOX];
static uint16_t g_bc[NBOX];
static inline uint8_t tri(uint16_t u) { u &= 511; return u & 256 ? 511 - u : u; }
static void boxesBegin() {
  for (uint8_t k = 0; k < NBOX; k++) {
    g_bx[k]  = ((uint16_t)tri(g_t * BOX_SX[k] + k * 100) * (FB_W - BOX_W[k])) >> 8;
    g_by0[k] = ((uint16_t)tri(g_t * BOX_SY[k] + k * 70 + 50) * (FB_H - BOX_H[k])) >> 8;
    g_bc[k]  = g_pal[(uint8_t)(k * 51 + (g_t >> 1))];
  }
}
static void boxesRow(uint8_t y, uint16_t* b) {
  memset(b, 0, FB_W * 2);
  for (uint8_t k = 0; k < NBOX; k++) {
    if (y >= g_by0[k] && y < g_by0[k] + BOX_H[k])
      for (uint8_t x = g_bx[k]; x < g_bx[k] + BOX_W[k]; x++) b[x] |= g_bc[k];
  }
}

// --------------------------------------------- 7: solid fills (flip test)
static uint16_t g_solid;
static void solidBegin() {
  uint8_t i = (g_t >> 6) % 5;   // changes about once a second
  g_solid = i == 0 ? pack(31, 0, 0) : i == 1 ? pack(0, 31, 0) : i == 2 ? pack(0, 0, 31)
          : i == 3 ? pack(31, 31, 31) : 0;
}
static void solidRow(uint8_t, uint16_t* b) {
  for (uint8_t x = 0; x < FB_W; x++) b[x] = g_solid;
}

// ------------------------------------- 8: address-bit diagnostic (static)
// Left: 7 columns of 8 px show y bit 0..6 (white = 1) as a binary row counter,
// so each address bit appears as stripes of period 2,4,..,128 rows. Right: a
// hue gradient over y. A dead/stuck y bit shows as a missing stripe column or a
// gradient that repeats. Column 0 of each 8-px block is y bit 0 ... bit 6.
static void diagBegin() {}
static void diagRow(uint8_t y, uint16_t* b) {
  uint16_t w = pack(31, 31, 31);
  for (uint8_t x = 0; x < FB_W; x++) {
    if (x < 56) b[x] = ((y >> (x >> 3)) & 1) ? w : 0;
    else if (x < 64) b[x] = 0;
    else b[x] = g_pal[(uint8_t)(y * 2)];
  }
}

// ---------------------------------------------------------------- table
struct Demo {
  const char* name;
  void (*begin)();
  void (*row)(uint8_t, uint16_t*);
};
static const Demo DEMOS[] = {
  {"test card (bars/ramps/frame)", cardBegin,   cardRow},
  {"plasma",                       plasmaBegin, plasmaRow},
  {"rainbow waves",                wavesBegin,  wavesRow},
  {"interference rings",           interfBegin, interfRow},
  {"metaballs",                    ballsBegin,  ballsRow},
  {"rotozoom checker",             rotoBegin,   rotoRow},
  {"bouncing boxes",               boxesBegin,  boxesRow},
  {"solid fills R,G,B,W,K",        solidBegin,  solidRow},
  {"y-address bit diagnostic",     diagBegin,   diagRow},
};
#define NDEMOS (sizeof(DEMOS) / sizeof(DEMOS[0]))

// Renders one frame into the board port 1 currently reaches (the back buffer).
static void renderFrame(const Demo& d) {
  d.begin();
  cpuDrive();
  for (uint8_t y = 0; y < FB_H; y++) {
    d.row(y, g_row);
    uint16_t base = (uint16_t)y << 8;
    for (uint8_t x = 0; x < FB_W; x++) cpuWrite<CPU_W>(base | x, g_row[x]);
  }
  cpuRelease();
}
