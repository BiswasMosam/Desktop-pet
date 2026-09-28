// Shared state for the desktop pet. Everything the screens draw lives in
// `world`, and only the link (commands from Aminal) and the clock change it.

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#define SCREEN_W 128
#define SCREEN_H 64

extern Adafruit_SSD1306 display;

// True once now has passed t, even across the millis() wrap at ~49.7 days
inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

// ---------- Where commands come from ----------
enum LinkKind : uint8_t { LINK_NONE, LINK_USB, LINK_BT, LINK_WIFI };

// ---------- What Aminal is doing ----------
enum AmState : uint8_t { AM_OFF, AM_IDLE, AM_LISTENING, AM_THINKING, AM_SPEAKING };

// ---------- Faces it can pull on request ----------
enum Emote : uint8_t {
  EM_NONE, EM_HAPPY, EM_LOVE, EM_SURPRISED, EM_SAD, EM_ANGRY, EM_WINK, EM_SLEEPY
};

// ---------- Screens, in the order a double tap walks through them ----------
enum Screen : uint8_t { SCR_FACE, SCR_CLOCK, SCR_WEATHER, SCR_TIMER, SCR_STATUS, SCR_COUNT };

struct World {
  // Clock: a Unix time pinned to a millis() reading, local offset added on display
  bool     timeValid = false;
  uint32_t epochAtSync = 0;
  uint32_t millisAtSync = 0;
  int32_t  tzOffset = 0;

  // Weather, as last told
  bool     wxValid = false;
  int16_t  wxTemp = 0, wxHi = 0, wxLo = 0;
  uint8_t  wxCode = 0;
  char     wxPlace[20] = "";
  uint32_t wxAt = 0;             // millis() when it arrived

  // The timer, counted down here between updates
  bool     timerOn = false;
  bool     timerPaused = false;
  uint32_t timerTotal = 0;       // seconds
  uint32_t timerLeftMs = 0;      // left at timerSyncMs
  uint32_t timerSyncMs = 0;
  char     timerLabel[24] = "";

  // Aminal
  AmState  am = AM_OFF;
  uint8_t  level = 0;            // voice level, 0..100
  LinkKind link = LINK_NONE;
  uint32_t linkSeen = 0;         // millis() of the last line from the host

  // A card drawn over whatever screen is up
  bool     alertOn = false;
  bool     alertLoud = false;    // flashes until tapped (timer done)
  char     alertTitle[20] = "";
  char     alertText[120] = "";
  uint32_t alertStart = 0, alertUntil = 0;
};
extern World world;

// main.cpp
extern Screen screen;
void goScreen(Screen s, uint32_t holdMs, uint32_t now);
bool clockNow(struct tm &out);            // local time; false until synced once
uint32_t timerLeftMs(uint32_t now);
const char *screenName(Screen s);

// face.cpp
void faceBegin(uint32_t now);
void faceTouch(uint32_t now);             // petted
void faceEmote(Emote e, uint32_t ms, uint32_t now);
void faceWake(uint32_t now);              // something happened, stay awake
void faceUpdate(uint32_t now);
void faceDraw(uint32_t now);

// screens.cpp
void drawClock(uint32_t now);
void drawWeather(uint32_t now);
void drawTimer(uint32_t now);
void drawStatus(uint32_t now);
void drawAlert(uint32_t now);
void showAlert(const char *title, const char *text, uint32_t secs, bool loud, uint32_t now);
void dismissAlert();

// link.cpp
void linkBegin();
void linkPoll(uint32_t now);
void linkTick(uint32_t now);
void linkEvent(const char *text);         // tell the host something happened
