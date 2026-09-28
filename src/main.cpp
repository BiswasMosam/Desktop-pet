// Desktop pet: step 1, the face.
// Black Pill + 0.96" SSD1306 OLED (SCL -> B6, SDA -> B7, VCC -> 3V3, GND -> GND).
// The KEY button (PA0) stands in for a touch sensor: press it to pet the pet.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W   128
#define SCREEN_H   64
#define OLED_ADDR  0x3C   // some modules use 0x3D
#define LED_PIN    PC13   // onboard LED, on when LOW
#define KEY_PIN    PA0    // onboard KEY button, LOW when pressed

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- Eye shape ----------
const int EYE_W   = 36;
const int EYE_H   = 36;
const int EYE_R   = 10;   // corner radius
const int EYE_GAP = 16;   // space between the eyes

// ---------- Mood and timing ----------
enum Mood { NORMAL, HAPPY, SLEEPY };
Mood mood = NORMAL;

const unsigned long SLEEP_AFTER_MS = 30000;  // ignored for 30s -> falls asleep
const unsigned long HAPPY_FOR_MS   = 2500;   // how long a pet keeps it happy

// Current values ease toward targets, which makes movement smooth
float curX = 0, curY = 0, curH = EYE_H;
float tgtX = 0, tgtY = 0, tgtH = EYE_H;

unsigned long lastFrame = 0;
unsigned long nextBlink = 0, blinkEnd = 0;
unsigned long nextLook = 0;
unsigned long happyUntil = 0;
unsigned long lastInteraction = 0;
bool blinking = false;
bool lastKey = HIGH;

// True once now has passed t, even across the millis() wrap at ~49.7 days
bool reached(unsigned long now, unsigned long t) {
  return (long)(now - t) >= 0;
}

// ---------- Drawing ----------
void drawEye(int cx, int cy, int w, int h) {
  if (h < 2) h = 2;
  int r = min(EYE_R, h / 2);
  display.fillRoundRect(cx - w / 2, cy - h / 2, w, h, r, SSD1306_WHITE);

  if (mood == HAPPY) {
    // Cut a curve out of the bottom so the eye becomes a happy "^".
    // Placed from the full eye height, so a half-open eye waking from sleep
    // isn't swallowed by the cutout while it grows.
    display.fillCircle(cx, cy + EYE_H / 2 + w / 4, (w * 6) / 10, SSD1306_BLACK);
  }
}

void drawFace() {
  display.clearDisplay();

  int cy = SCREEN_H / 2 + (int)curY;
  int lx = SCREEN_W / 2 - EYE_GAP / 2 - EYE_W / 2 + (int)curX;
  int rx = SCREEN_W / 2 + EYE_GAP / 2 + EYE_W / 2 + (int)curX;

  drawEye(lx, cy, EYE_W, (int)curH);
  drawEye(rx, cy, EYE_W, (int)curH);

  if (mood == SLEEPY) {
    // Little floating z that bobs up and down
    int bob = (millis() / 400) % 3;
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(112, 8 - bob);
    display.print("z");
    display.setCursor(118, 2 - bob / 2);
    display.print("z");
  }

  display.display();
}

// ---------- Behaviour ----------
void handleButton(unsigned long now) {
  bool key = digitalRead(KEY_PIN);
  if (lastKey == HIGH && key == LOW) {   // just pressed
    mood = HAPPY;
    happyUntil = now + HAPPY_FOR_MS;
    lastInteraction = now;
    blinking = false;
  }
  lastKey = key;
}

void updateMood(unsigned long now) {
  if (mood == HAPPY && reached(now, happyUntil)) {
    mood = NORMAL;
  }
  if (mood == NORMAL && now - lastInteraction > SLEEP_AFTER_MS) {
    mood = SLEEPY;
  }
}

void updateTargets(unsigned long now) {
  switch (mood) {
    case NORMAL:
      // Glance somewhere random every few seconds
      if (reached(now, nextLook)) {
        tgtX = random(-20, 21);
        tgtY = random(-8, 9);
        nextLook = now + random(1500, 4000);
      }
      // Blink every few seconds
      if (!blinking && reached(now, nextBlink)) {
        blinking = true;
        blinkEnd = now + 130;
        nextBlink = now + random(2500, 6000);
      }
      if (blinking && reached(now, blinkEnd)) blinking = false;
      tgtH = blinking ? 2 : EYE_H;
      break;

    case HAPPY:
      tgtX = 0;
      tgtY = -4;
      tgtH = EYE_H;
      break;

    case SLEEPY:
      tgtX = 0;
      tgtY = 8;
      tgtH = 6;
      break;
  }

  // Ease toward the target. Blinks close fast, everything else glides.
  float speed = (blinking || tgtH < curH - 10) ? 0.6f : 0.25f;
  curX += (tgtX - curX) * 0.25f;
  curY += (tgtY - curY) * 0.25f;
  curH += (tgtH - curH) * speed;
}

// ---------- Setup and loop ----------
void setup() {
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off

  Serial.begin(115200);

  Wire.setSDA(PB7);
  Wire.setSCL(PB6);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // OLED not found: fast-blink the onboard LED forever
    bool on = false;
    while (true) {
      on = !on;
      digitalWrite(LED_PIN, on ? LOW : HIGH);
      delay(100);
    }
  }

  randomSeed(analogRead(PA1));
  unsigned long now = millis();
  lastInteraction = now;
  nextBlink = now + 1500;
  nextLook = now + 1000;

  display.clearDisplay();
  display.display();
  Serial.println("Desktop pet is awake");
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < 20) return;   // about 50 fps
  lastFrame = now;

  handleButton(now);
  updateMood(now);
  updateTargets(now);
  drawFace();
}
