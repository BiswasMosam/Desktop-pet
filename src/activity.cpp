// The props for coding and gaming: glasses, a keyboard under two paws with
// code flying off it, a controller, and sparks. face.cpp decides when.

#include "pet.h"

// ---------- Glasses ----------

// A frame around each eye and a bridge between them. The height is the
// eye's open height, so a blink closes the eye behind the glasses, not
// the glasses with it.
void glassesDraw(int lx, int rx, int cy, int eyeW, int lensH) {
  for (int cx : {lx, rx}) {
    int x = cx - eyeW / 2 - 3, y = cy - lensH / 2;
    display.drawRoundRect(x, y, eyeW + 6, lensH, 6, SSD1306_WHITE);
    display.drawRoundRect(x + 1, y + 1, eyeW + 4, lensH - 2, 5, SSD1306_WHITE);
  }
  int from = lx + eyeW / 2 + 3, to = rx - eyeW / 2 - 3;
  int y = cy - lensH / 2 + 6;
  display.drawFastHLine(from, y, to - from, SSD1306_WHITE);
  display.drawFastHLine(from + 2, y - 1, to - from - 4, SSD1306_WHITE);
}

// ---------- A keyboard and two paws ----------

static const int KB_X = 38, KB_W = 52, KB_Y = 57;

struct Paw { int x; uint32_t downUntil; };
static Paw paws[2] = {{51, 0}, {77, 0}};

// Code flying off the ends of the keyboard while the paws type
struct Glyph { float x, y, vx; char c; bool alive; };
static Glyph glyphs[4];

static void glyphSpawn(int side) {
  static const char CODE[] = "{}();<>=/*#01";
  for (Glyph &g : glyphs) {
    if (g.alive) continue;
    g.x = side < 0 ? KB_X - 6 : KB_X + KB_W;
    g.y = KB_Y - 6;
    g.vx = side * 1.4f;
    g.c = CODE[random(sizeof(CODE) - 1)];
    g.alive = true;
    return;
  }
}

void pawTap(uint32_t now) {
  static uint8_t last = 0;
  uint8_t i = random(10) < 7 ? 1 - last : last;   // mostly one paw then the other
  last = i;
  paws[i].downUntil = now + 80;
  if (random(3) == 0) glyphSpawn(i ? 1 : -1);
}

void keyboardDraw() {
  display.fillRoundRect(KB_X, KB_Y, KB_W, 7, 2, SSD1306_WHITE);
  for (int x = KB_X + 3; x < KB_X + KB_W - 3; x += 4) {
    display.drawFastHLine(x, KB_Y + 2, 2, SSD1306_BLACK);
    display.drawFastHLine(x + 2, KB_Y + 4, 2, SSD1306_BLACK);
  }
}

// Resting on the keys while it reads; up and down while it types
void pawsDraw(uint32_t now, bool typing) {
  for (Paw &p : paws) {
    bool down = !typing || !reached(now, p.downUntil);
    int y = down ? 52 : 47;
    display.fillRoundRect(p.x - 6, y - 1, 12, 8, 3, SSD1306_BLACK);   // keeps it off the keys
    display.fillRoundRect(p.x - 5, y, 10, 6, 3, SSD1306_WHITE);
    display.drawFastVLine(p.x - 2, y + 4, 2, SSD1306_BLACK);            // toes
    display.drawFastVLine(p.x + 1, y + 4, 2, SSD1306_BLACK);
  }
}

void glyphsDraw() {
  for (Glyph &g : glyphs) {
    if (!g.alive) continue;
    g.y -= 0.5f;                      // out to the side, under the glasses
    g.x += g.vx;
    if (g.y < 42) { g.alive = false; continue; }
    display.drawChar((int)g.x, (int)g.y, g.c, SSD1306_WHITE, SSD1306_WHITE, 1);
  }
}

// ---------- A controller ----------

static uint8_t pressed = 0;     // 1-4 a face button, 5-8 the d-pad, 0 nothing
static uint32_t pressUntil = 0;

void controllerPress(uint32_t now) {
  pressed = random(1, 9);
  pressUntil = now + 120;
}

// dx leans it with the eyes; dy jolts it on a big moment
void controllerDraw(int dx, int dy, uint32_t now) {
  int cx = SCREEN_W / 2 + dx, top = 47 + dy;
  display.fillRoundRect(cx - 22, top, 44, 12, 5, SSD1306_WHITE);
  display.fillCircle(cx - 16, top + 12, 6, SSD1306_WHITE);   // the grips
  display.fillCircle(cx + 16, top + 12, 6, SSD1306_WHITE);

  uint8_t p = reached(now, pressUntil) ? 0 : pressed;
  // The d-pad, nudged toward the direction held
  int px = cx - 14, py = top + 6;
  if (p == 5) px--;
  if (p == 6) px++;
  if (p == 7) py--;
  if (p == 8) py++;
  display.fillRect(px - 4, py - 1, 9, 3, SSD1306_BLACK);
  display.fillRect(px - 1, py - 4, 3, 9, SSD1306_BLACK);

  // Four face buttons in a diamond; the one pressed swells
  static const int8_t at[4][2] = {{0, -3}, {3, 0}, {0, 3}, {-3, 0}};
  for (int i = 0; i < 4; i++) {
    display.fillCircle(cx + 14 + at[i][0], top + 6 + at[i][1], p == i + 1 ? 2 : 1,
                       SSD1306_BLACK);
  }
}

// A big moment in the game: rays burst out from beside the eyes
void sparksDraw(uint32_t since) {
  if (since > 600) return;
  float t = since / 600.0f;
  static const int8_t from[2][2] = {{9, 12}, {118, 12}};
  for (auto &c : from) {
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4 + 0.4f;
      float r0 = 2 + t * 10, r1 = r0 + 1 + 4 * (1 - t);
      display.drawLine(c[0] + cosf(a) * r0, c[1] + sinf(a) * r0,
                       c[0] + cosf(a) * r1, c[1] + sinf(a) * r1, SSD1306_WHITE);
    }
  }
}
