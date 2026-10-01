// Desktop pet: a face on the desk that keeps Aminal company.
// Black Pill + 0.96" SSD1306 OLED (SCL -> B6, SDA -> B7, VCC -> 3V3, GND -> GND).
// HC-05 Bluetooth (optional): TXD -> A3, RXD -> A2, VCC -> 5V, GND -> GND.
//
// KEY (PA0) is the only button, or a TTP223 touch pad on B0 under the top of
// the enclosure, which does exactly the same:
//   tap         pet it (on the face), next screen (anywhere else), dismiss a card
//   double tap  next screen
//   hold        back to the face; on the timer, pause or resume it

#include <Wire.h>
#include "pet.h"

#define OLED_ADDR  0x3C   // some modules use 0x3D
#define LED_PIN    PC13   // onboard LED, on when LOW
#define KEY_PIN    PA0    // onboard KEY button, LOW when pressed
#define TOUCH_PIN  PB0    // TTP223 touch pad (optional), HIGH while touched.
#define TOUCH_VCC  PB14   // and its power: it draws a few microamps, so two
#define TOUCH_GND  PB15   // pins feed it and the 3V3/G pins go further
                          // Not PA0: the bootloader reads PA0 at reset, and a
                          // TTP223 holds its output LOW when nobody's touching.

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
World world;
Screen screen = SCR_FACE;

const uint32_t BROWSE_MS    = 20000;  // a screen you flipped to returns to the face after this
const uint32_t DOUBLE_TAP_MS = 350;
const uint32_t HOLD_MS      = 700;

static uint32_t screenUntil = 0;      // 0 = stay put
static uint32_t lastFrame = 0;

// ---------- Clock and timer ----------

bool clockNow(struct tm &out) {
  if (!world.timeValid) return false;
  time_t t = (time_t)world.epochAtSync + (millis() - world.millisAtSync) / 1000
           + world.tzOffset;
  gmtime_r(&t, &out);
  return true;
}

uint32_t timerLeftMs(uint32_t now) {
  if (!world.timerOn) return 0;
  if (world.timerPaused) return world.timerLeftMs;
  uint32_t gone = now - world.timerSyncMs;
  return gone >= world.timerLeftMs ? 0 : world.timerLeftMs - gone;
}

// ---------- Screens ----------

const char *screenName(Screen s) {
  static const char *NAMES[] = {"face", "clock", "weather", "timer", "usage", "status"};
  return s < SCR_COUNT ? NAMES[s] : "";
}

void goScreen(Screen s, uint32_t holdMs, uint32_t now) {
  screen = s;
  screenUntil = (s == SCR_FACE || holdMs == 0) ? 0 : now + holdMs;
  faceWake(now);
}

static void nextScreen(uint32_t now) {
  // The timer only while one runs, Claude's usage only once it has been heard
  Screen s = screen;
  do s = (Screen)((s + 1) % SCR_COUNT);
  while ((s == SCR_TIMER && !world.timerOn) || (s == SCR_USAGE && !world.cuValid));
  goScreen(s, BROWSE_MS, now);

  char ev[24];
  snprintf(ev, sizeof(ev), "EV next %s", screenName(s));
  linkEvent(ev);
}

// ---------- The button ----------

static bool keyDown = false, holdFired = false;
static uint32_t downAt = 0, lastTapAt = 0;

static void onTap(uint32_t now) {
  if (world.alertOn) {
    if (world.alertLoud) linkEvent("EV dismiss");
    dismissAlert();
  } else if (screen == SCR_FACE) {
    faceTouch(now);
    linkEvent("EV pet");
  } else {
    nextScreen(now);
  }
}

static void onHold(uint32_t now) {
  if (screen == SCR_TIMER && world.timerOn) {
    linkEvent("EV hold timer");       // Aminal pauses or resumes it
  } else {
    goScreen(SCR_FACE, 0, now);
    linkEvent("EV hold");
  }
}

static void handleKey(uint32_t now) {
  bool pressed = digitalRead(KEY_PIN) == LOW || digitalRead(TOUCH_PIN) == HIGH;
  if (pressed && !keyDown) {
    keyDown = true;
    holdFired = false;
    downAt = now;
  } else if (pressed && !holdFired && now - downAt >= HOLD_MS) {
    holdFired = true;
    onHold(now);
  } else if (!pressed && keyDown) {
    keyDown = false;
    if (holdFired) return;
    // The first tap acts at once, so petting never lags. A quick second tap
    // on the face also flips to the next screen.
    bool second = lastTapAt && now - lastTapAt < DOUBLE_TAP_MS;
    lastTapAt = second ? 0 : now;
    if (second && screen == SCR_FACE && !world.alertOn) nextScreen(now);
    else onTap(now);
  }
}

// ---------- Setup and loop ----------

void setup() {
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);   // reads "not touched" with no pad fitted
  pinMode(TOUCH_VCC, OUTPUT);
  digitalWrite(TOUCH_VCC, HIGH);
  pinMode(TOUCH_GND, OUTPUT);
  digitalWrite(TOUCH_GND, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off

  linkBegin();
  wifiBegin();
  rfidBegin();

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
  faceBegin(millis());

  display.clearDisplay();
  display.display();
}

void loop() {
  uint32_t now = millis();
  linkPoll(now);                      // every pass, so lines never pile up
  wifiTick(now);
  if (now - lastFrame < 20) return;   // about 50 fps, as the I2C allows
  lastFrame = now;

  handleKey(now);
  rfidTick(now);
  linkTick(now);
  if (screenUntil && reached(now, screenUntil)) goScreen(SCR_FACE, 0, now);
  if (world.alertOn && reached(now, world.alertUntil)) dismissAlert();
  faceUpdate(now);

  display.clearDisplay();
  switch (screen) {
    case SCR_CLOCK:   drawClock(now);   break;
    case SCR_WEATHER: drawWeather(now); break;
    case SCR_TIMER:   drawTimer(now);   break;
    case SCR_USAGE:   drawUsage(now);   break;
    case SCR_STATUS:  drawStatus(now);  break;
    default:          faceDraw(now);    break;
  }
  if (world.alertOn) drawAlert(now);
  display.display();
}
