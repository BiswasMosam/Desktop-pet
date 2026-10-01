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

// ---------- What's playing on the PC ----------
enum Media : uint8_t { MEDIA_NONE, MEDIA_MUSIC, MEDIA_WATCH };
#define VZ_BARS 16

// ---------- What's in front on the PC ----------
enum Activity : uint8_t { ACT_NONE, ACT_CODE, ACT_GAME };

// ---------- Aminal taking a selfie ----------
enum Cam : uint8_t { CAM_NONE, CAM_READY, CAM_COUNT, CAM_SHOT };

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

  // The ESP-01's WiFi
  uint8_t  wifi = 0;             // 0 no module, 1 joining, 2 offline, 3 online
  char     wifiIp[16] = "";

  // Music or a film on the PC, and how it sounds
  Media    media = MEDIA_NONE;
  uint8_t  vz[VZ_BARS] = {0};    // spectrum, 0..15 per bar
  uint32_t vzAt = 0;             // millis() of the last spectrum frame
  uint32_t beatAt = 0;           // millis() of the last beat heard
  uint32_t jumpAt = 0;           // millis() of the last sudden loud moment

  // Code or a game in front on the PC, and whether the keys are going
  Activity act = ACT_NONE;
  bool     typing = false;

  // A selfie in progress: which moment, since when, and how long the
  // spoken countdown takes
  Cam      cam = CAM_NONE;
  uint32_t camAt = 0;
  uint16_t camCountMs = 0;

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
void faceMusicStyle(const char *name, uint32_t now);   // headphones, dance, bars

// media.cpp: the props for music and films
void drawHeadphones(int lx, int rx, int cy, int eyeW);
void notesSpawn(uint32_t now);
void notesDraw();
void visualizerDraw(uint32_t now);
void bucketDraw();
void kernelDraw(uint32_t eatT, int mouthX, int mouthY);
void chewDraw(uint32_t now, int mouthX, int mouthY);
void burstDraw(uint32_t sinceJump);

// activity.cpp: the props for coding and gaming
void glassesDraw(int lx, int rx, int cy, int eyeW, int lensH);
void pawTap(uint32_t now);
void keyboardDraw();
void pawsDraw(uint32_t now, bool typing);
void glyphsDraw();
void controllerPress(uint32_t now);
void controllerDraw(int dx, int dy, uint32_t now);
void sparksDraw(uint32_t since);

// camera.cpp: the camera face during a selfie
void cameraTick(uint32_t now);
void cameraDraw(uint32_t now);

// screens.cpp
void drawClock(uint32_t now);
void drawWeather(uint32_t now);
void drawTimer(uint32_t now);
void drawStatus(uint32_t now);
void drawAlert(uint32_t now);
void bigDigit(const char *s, int cx, int baseline);   // the timer's font
void showAlert(const char *title, const char *text, uint32_t secs, bool loud, uint32_t now);
void dismissAlert();

// wifi.cpp: the ESP-01, driven through its factory AT firmware
extern Uart SerialESP;
#define ESP_BAUD 115200
void wifiBegin();
void wifiTick(uint32_t now);
void wifiRestart(uint32_t now);
void wifiSetPlace(char *args, uint32_t now);   // LO <lat> <lon> <place>
void wifiJoin(char *args);                     // WF <ssid>\t<password>
extern Stream &wifiLink;                       // the bridge, over the network

// link.cpp
void linkBegin();
void linkPoll(uint32_t now);
void linkTick(uint32_t now);
void linkEvent(const char *text);         // tell the host something happened
void espReset(bool bootloader);           // pulse the ESP-01's reset pin
