// The props for music and films: headphones, floating notes, a spectrum
// visualizer, and a bucket of popcorn. face.cpp decides when; this draws.

#include "pet.h"

// ---------- Headphones ----------

// A band arching over the top of the screen from cup to cup, moving with
// the head so a nod takes the headphones with it
void drawHeadphones(int lx, int rx, int cy, int eyeW) {
  int leftCup = lx - eyeW / 2 - 14, rightCup = rx + eyeW / 2 + 4;
  int cupTop = cy - 12;
  display.fillRoundRect(leftCup, cupTop, 10, 24, 4, SSD1306_WHITE);
  display.fillRoundRect(rightCup, cupTop, 10, 24, 4, SSD1306_WHITE);

  // Half an ellipse from one cup to the other, two pixels thick
  float cx = (leftCup + 5 + rightCup + 5) / 2.0f;
  float rx2 = (rightCup - leftCup) / 2.0f;
  float ry = cupTop - 1;
  if (ry < 4) ry = 4;
  int px = 0, py = 0;
  for (int i = 0; i <= 20; i++) {
    float a = PI * i / 20;
    int x = cx - cosf(a) * rx2, y = cupTop - sinf(a) * ry;
    if (i) {
      display.drawLine(px, py, x, y, SSD1306_WHITE);
      display.drawLine(px, py + 1, x, y + 1, SSD1306_WHITE);
    }
    px = x; py = y;
  }
}

// ---------- Music notes floating up ----------

struct Note { float x, y; bool alive, pair; };
static Note notes[4];

void notesSpawn(uint32_t now) {
  for (Note &n : notes) {
    if (n.alive) continue;
    bool left = random(2);
    n.x = left ? random(4, 18) : random(108, 120);
    n.y = 56;
    n.pair = random(3) == 0;
    n.alive = true;
    return;
  }
}

void notesDraw() {
  for (Note &n : notes) {
    if (!n.alive) continue;
    n.y -= 0.7f;
    n.x += sinf(n.y / 6.0f) * 0.4f;
    if (n.y < 4) { n.alive = false; continue; }
    int x = n.x, y = n.y;
    display.fillCircle(x, y, 2, SSD1306_WHITE);
    display.drawFastVLine(x + 2, y - 7, 7, SSD1306_WHITE);
    if (n.pair) {                                  // a beamed pair
      display.fillCircle(x + 6, y - 1, 2, SSD1306_WHITE);
      display.drawFastVLine(x + 8, y - 8, 7, SSD1306_WHITE);
      display.drawLine(x + 2, y - 7, x + 8, y - 8, SSD1306_WHITE);
      display.drawLine(x + 2, y - 6, x + 8, y - 7, SSD1306_WHITE);
    } else {
      display.drawLine(x + 2, y - 7, x + 5, y - 4, SSD1306_WHITE);
    }
  }
}

// ---------- Spectrum visualizer ----------

void visualizerDraw(uint32_t now) {
  static float height[VZ_BARS], peak[VZ_BARS];
  bool fresh = world.vzAt && now - world.vzAt < 1500;
  for (int i = 0; i < VZ_BARS; i++) {
    float want = fresh ? world.vz[i] * 56.0f / 15.0f : 0;
    // Jump up at once, fall back gently, the way a meter does
    height[i] = want > height[i] ? want : max(want, height[i] - 1.8f);
    peak[i] = max(height[i], peak[i] - 0.5f);
    int x = i * 8 + 1, h = (int)height[i];
    if (h > 0) display.fillRect(x, 63 - h, 6, h, SSD1306_WHITE);
    display.drawFastHLine(x, 61 - (int)peak[i], 6, SSD1306_WHITE);
  }
}

// ---------- Popcorn ----------

static const int BUCKET_X = 103;   // left edge of the bucket's rim
static const int BUCKET_Y = 46;    // its rim

void bucketDraw() {
  // A striped bucket, narrower at the bottom
  for (int y = BUCKET_Y; y < SCREEN_H; y++) {
    int w = 23 - (y - BUCKET_Y) * 6 / 17;
    display.drawFastHLine(BUCKET_X + (23 - w) / 2, y, w, SSD1306_WHITE);
  }
  for (int s = 0; s < 3; s++) {
    int top = BUCKET_X + 5 + s * 6;
    display.drawLine(top, BUCKET_Y + 2, top + (s - 1), SCREEN_H - 1, SSD1306_BLACK);
  }
  // Heaped over the rim
  static const int8_t heap[][2] = {{2, -2}, {7, -4}, {12, -3}, {17, -4}, {21, -2},
                                   {9, -8}, {15, -7}};
  for (auto &k : heap) {
    display.fillCircle(BUCKET_X + k[0], BUCKET_Y + k[1], 3, SSD1306_WHITE);
    display.drawPixel(BUCKET_X + k[0], BUCKET_Y + k[1], SSD1306_BLACK);  // a crease
  }
}

// One kernel on its way from the bucket to the mouth, in the first 450 ms
void kernelDraw(uint32_t eatT, int mouthX, int mouthY) {
  if (eatT >= 450) return;
  float t = eatT / 450.0f;
  int fromX = BUCKET_X + 11, fromY = BUCKET_Y - 10;
  int x = fromX + (mouthX - fromX) * t;
  int y = fromY + (mouthY - fromY) * t - sinf(PI * t) * 16;
  display.fillCircle(x, y, 2, SSD1306_WHITE);
}

void chewDraw(uint32_t now, int mouthX, int mouthY) {
  int h = (now / 120) % 2 ? 5 : 2;
  display.fillRoundRect(mouthX - 5, mouthY, 10, h, 2, SSD1306_WHITE);
}

// A sudden loud moment: kernels leap out of the bucket
void burstDraw(uint32_t sinceJump) {
  if (sinceJump > 900) return;
  // Each kernel its own way and its own height, so they scatter
  static const float vx[] = {-2.2f, -0.9f, 0.5f, 1.6f};
  static const float vy[] = {-3.0f, -4.2f, -3.6f, -2.6f};
  float f = sinceJump / 33.0f;                      // frames since the jump
  for (int i = 0; i < 4; i++) {
    int x = BUCKET_X + 11 + vx[i] * f;
    int y = BUCKET_Y - 8 + vy[i] * f + 0.22f * f * f;
    if (y < SCREEN_H) display.fillCircle(x, y, 2, SSD1306_WHITE);
  }
}
