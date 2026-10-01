// The camera face, while Aminal takes a selfie: the pet turns into a
// camera, counts down inside its own lens, and flashes at the moment the
// photo is taken. The moments come from Aminal (SF ready / count / shot),
// so the flash is the real shutter, not a guess.

#include "pet.h"

const int LENS_X = 64, LENS_Y = 30;
const uint32_t FLASH_MS = 150;       // the screen goes white
const uint32_t BLINK_MS = 350;       // then the shutter closes and opens
const uint32_t CLICK_MS = 1300;      // "click!", then it's developing

// Given up on by itself if Aminal never finishes, so a selfie that went
// wrong can't leave a camera on the desk
static uint32_t camLimit() {
  switch (world.cam) {
    case CAM_READY: return 20000;
    case CAM_COUNT: return world.camCountMs + 8000;
    case CAM_SHOT:  return 12000;
    default:        return 0;
  }
}

void cameraTick(uint32_t now) {
  if (world.cam != CAM_NONE && now - world.camAt > camLimit()) world.cam = CAM_NONE;
}

static void caption(const char *s) {
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_W / 2 - strlen(s) * 3, 56);
  display.print(s);
}

static void body(bool flashLit) {
  display.drawRoundRect(10, 8, 108, 44, 6, SSD1306_WHITE);
  display.drawRoundRect(11, 9, 106, 42, 5, SSD1306_WHITE);
  display.fillRoundRect(22, 3, 22, 7, 2, SSD1306_WHITE);      // viewfinder
  display.fillRect(92, 5, 12, 4, SSD1306_WHITE);              // shutter button
  if (flashLit) display.fillRect(18, 14, 12, 7, SSD1306_WHITE);
  else          display.drawRect(18, 14, 12, 7, SSD1306_WHITE);
  display.drawCircle(LENS_X, LENS_Y, 18, SSD1306_WHITE);
  display.drawCircle(LENS_X, LENS_Y, 17, SSD1306_WHITE);
}

// The pet's eye, as the lens's iris: it looks for you, then at you
static void iris(int dx, int dy) {
  display.fillCircle(LENS_X + dx, LENS_Y + dy, 7, SSD1306_WHITE);
  display.fillCircle(LENS_X + dx - 2, LENS_Y + dy - 2, 1, SSD1306_BLACK);   // a glint
}

void cameraDraw(uint32_t now) {
  uint32_t t = now - world.camAt;

  if (world.cam == CAM_READY) {
    // Focusing: the inner ring zooms in and out while the iris searches
    body(false);
    int ring = 12 + (int)(2 * sinf(t / 260.0f));
    display.drawCircle(LENS_X, LENS_Y, ring, SSD1306_WHITE);
    iris((int)(5 * sinf(t / 700.0f)), (int)(2 * sinf(t / 450.0f)));
    caption("smile please!");
    return;
  }

  if (world.cam == CAM_COUNT) {
    body(false);
    // The words land at about these points in "Hold still - three, two,
    // one", measured off Piper's own audio
    uint32_t ms = world.camCountMs;
    int digit = t < ms * 42 / 100 ? 0 : t < ms * 62 / 100 ? 3 : t < ms * 82 / 100 ? 2 :
                t < ms ? 1 : -1;
    if (digit > 0) {
      char d[2] = {(char)('0' + digit), 0};
      bigDigit(d, LENS_X, LENS_Y + 8);
      caption("hold still!");
    } else {
      display.drawCircle(LENS_X, LENS_Y, 12, SSD1306_WHITE);
      iris(0, 0);                                  // straight at you
      caption(digit == 0 ? "hold still!" : "cheese!");
    }
    return;
  }

  // CAM_SHOT
  if (t < FLASH_MS) {
    display.fillScreen(SSD1306_WHITE);
    return;
  }
  body(t < FLASH_MS + BLINK_MS);
  if (t < FLASH_MS + BLINK_MS) {
    // The shutter: blades close over the lens and open again
    float s = (t - FLASH_MS) / (float)BLINK_MS;
    int open = (int)(15 * fabsf(2 * s - 1));
    display.fillCircle(LENS_X, LENS_Y, 15, SSD1306_WHITE);
    if (open > 0) display.fillCircle(LENS_X, LENS_Y, open, SSD1306_BLACK);
    caption("click!");
  } else if (t < CLICK_MS) {
    display.drawCircle(LENS_X, LENS_Y, 12, SSD1306_WHITE);
    iris(0, 0);
    caption("click!");
  } else {
    // Developing: the iris looks up while Aminal works on the picture
    display.drawCircle(LENS_X, LENS_Y, 12, SSD1306_WHITE);
    iris((int)(4 * sinf(t / 600.0f)), -3);
    static const char *DOTS[] = {"developing", "developing.", "developing..", "developing..."};
    caption(DOTS[(t / 350) % 4]);
  }
}
