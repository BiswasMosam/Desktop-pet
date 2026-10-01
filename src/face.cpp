// The face: two rounded eyes that glance, blink, fidget, react to being
// petted, doze off when ignored, and follow what Aminal is doing.
//
// Nothing jumps. Every frame the current eye position, width and height
// ease toward a target, so every expression below is only a set of targets.

#include "pet.h"

const int EYE_W   = 36;
const int EYE_H   = 36;
const int EYE_R   = 10;   // corner radius
const int EYE_GAP = 16;   // space between the eyes

const uint32_t SLEEP_AFTER_MS = 30000;  // ignored for 30s -> falls asleep
const uint32_t NIGHT_SLEEP_MS = 10000;  // sooner at night
const uint32_t HAPPY_FOR_MS   = 2500;   // how long a pet keeps it happy

enum Mood : uint8_t { NORMAL, HAPPY, SLEEPY };
static Mood mood = NORMAL;

static Emote emote = EM_NONE;
static uint32_t emoteUntil = 0;

// Small idle habits so a pet left alone still looks alive
enum Fidget : uint8_t { FG_NONE, FG_LOOK_AROUND, FG_HOP, FG_SQUINT, FG_DOUBLE_BLINK, FG_COUNT };
static Fidget fidget = FG_NONE;
static uint32_t fidgetStart = 0, nextFidget = 0;
static bool fidgetBlinked = false;

// Current values ease toward targets, which makes movement smooth
static float curX = 0, curY = 0, curH = 2, curW = EYE_W, mouth = 0;
static float tgtX = 0, tgtY = 0, tgtH = EYE_H, tgtW = EYE_W;

static uint32_t nextBlink = 0, blinkEnd = 0, nextLook = 0, nextThink = 0;
static uint32_t happyUntil = 0, lastInteraction = 0;
static uint32_t lastTouch = 0;
static uint8_t touchStreak = 0;
static bool blinking = false;
static int8_t thinkSide = 1;

// Music and films. Each song gets one of three moods, rerolled every so
// often so a long playlist doesn't look the same all evening.
enum MusicStyle : uint8_t { MS_HEADPHONES, MS_DANCE, MS_VISUALIZER };
static MusicStyle style = MS_HEADPHONES;
static uint32_t styleUntil = 0;
static Media shownMedia = MEDIA_NONE;
static uint32_t lastBeat = 0, nextSynthBeat = 0;
static uint16_t beatNo = 0;
static uint32_t eatStart = 0, nextEat = 0, nextDrift = 0;

// Coding and gaming
static uint32_t lineStart = 0, lineLen = 1500, nextKey = 0, nextDart = 0, nextPress = 0;
static uint8_t lineNo = 0;
static float lensH = 30;          // the glasses follow the eye's open height

static bool aminalBusy() {
  return world.am == AM_LISTENING || world.am == AM_THINKING || world.am == AM_SPEAKING;
}

// Code or a game is in front and Aminal isn't using the face for itself
static bool actShown() {
  return world.act != ACT_NONE && !aminalBusy();
}

// Music or a film is on, nothing is in front of it, and Aminal isn't
// using the face. Music while coding or gaming only adds the headphones.
static bool mediaShown() {
  return world.media != MEDIA_NONE && world.act == ACT_NONE && !aminalBusy();
}

static bool gameWhoa(uint32_t now) {
  return world.act == ACT_GAME && world.jumpAt && now - world.jumpAt < 700;
}

// ---------- Moods and triggers ----------

static bool isNight() {
  struct tm t;
  if (!clockNow(t)) return false;
  return t.tm_hour >= 23 || t.tm_hour < 7;
}

void faceBegin(uint32_t now) {
  lastInteraction = now;
  nextBlink = now + 1500;
  nextLook = now + 1000;
  nextFidget = now + random(15000, 40000);
  curH = 2;               // eyes open on boot
}

void faceWake(uint32_t now) {
  lastInteraction = now;
  if (mood == SLEEPY) mood = NORMAL;
}

void faceTouch(uint32_t now) {
  faceWake(now);
  touchStreak = (now - lastTouch < 1500) ? touchStreak + 1 : 1;
  lastTouch = now;
  blinking = false;
  if (touchStreak >= 3) {
    faceEmote(EM_LOVE, 2500, now);    // keep petting and it falls for you
    touchStreak = 0;
  } else {
    mood = HAPPY;
    happyUntil = now + HAPPY_FOR_MS;
  }
}

void faceEmote(Emote e, uint32_t ms, uint32_t now) {
  emote = e;
  emoteUntil = now + ms;
  if (e != EM_SLEEPY) faceWake(now);
  blinking = false;
}

// ---------- Targets ----------

static void startBlink(uint32_t now, uint32_t len) {
  blinking = true;
  blinkEnd = now + len;
}

static void blinkNaturally(uint32_t now, uint32_t minGap, uint32_t maxGap) {
  if (!blinking && reached(now, nextBlink)) {
    startBlink(now, 130);
    nextBlink = now + random(minGap, maxGap);
  }
}

static void idleTargets(uint32_t now) {
  tgtH = EYE_H;

  if (fidget == FG_NONE && reached(now, nextFidget)) {
    fidget = (Fidget)random(1, FG_COUNT);
    fidgetStart = now;
    fidgetBlinked = false;
    nextFidget = now + random(15000, 40000);
  }

  uint32_t t = now - fidgetStart;
  switch (fidget) {
    case FG_LOOK_AROUND:            // a good look left, then right
      tgtY = 0;
      tgtX = t < 700 ? -22 : 22;
      if (t > 1500) { fidget = FG_NONE; tgtX = 0; }
      return;
    case FG_HOP:                    // a little bounce
      tgtY = t < 180 ? -9 : 0;
      if (t > 500) fidget = FG_NONE;
      return;
    case FG_SQUINT:                 // suspicious
      tgtH = 16;
      if (t > 1000) fidget = FG_NONE;
      return;
    case FG_DOUBLE_BLINK:
      if (t < 20) startBlink(now, 110);
      if (t > 300 && !fidgetBlinked) { startBlink(now, 110); fidgetBlinked = true; }
      if (t > 600) fidget = FG_NONE;
      break;
    default:
      break;
  }

  // Glance somewhere random every few seconds
  if (reached(now, nextLook)) {
    tgtX = random(-20, 21);
    tgtY = random(-8, 9);
    nextLook = now + random(1500, 4000);
  }
  blinkNaturally(now, 2500, 6000);
}

// ---------- Music and films ----------

static void pickStyle(uint32_t now) {
  // The visualizer needs bars from the PC; without them, dance or vibe
  bool bars = world.vzAt && now - world.vzAt < 1500;
  MusicStyle was = style;
  style = (MusicStyle)random(0, bars ? 3 : 2);
  if (style == was && random(4)) style = (MusicStyle)((style + 1) % (bars ? 3 : 2));
  styleUntil = now + random(25000, 45000);
}

// Asked for by name ("MD music dance"), held for as long as a random pick
void faceMusicStyle(const char *name, uint32_t now) {
  if      (!strcmp(name, "headphones")) style = MS_HEADPHONES;
  else if (!strcmp(name, "dance"))      style = MS_DANCE;
  else if (!strcmp(name, "bars"))       style = MS_VISUALIZER;
  else return;
  shownMedia = MEDIA_MUSIC;          // so starting the music doesn't reroll it
  styleUntil = now + 40000;
}

// The PC's beats while they're coming, a steady 115 bpm of our own if not
static void tickBeat(uint32_t now) {
  bool heard = world.beatAt && now - world.beatAt < 2500;
  bool beat = false;
  if (heard && world.beatAt != lastBeat) {
    lastBeat = world.beatAt;
    beat = true;
  } else if (!heard && reached(now, nextSynthBeat)) {
    lastBeat = now;
    nextSynthBeat = now + 520;
    beat = true;
  }
  if (!beat) return;
  beatNo++;
  if (style == MS_DANCE && beatNo % 2 == 0 && world.act == ACT_NONE) notesSpawn(now);
}

static void musicTargets(uint32_t now) {
  if (reached(now, styleUntil)) pickStyle(now);
  tickBeat(now);
  bool onBeat = now - lastBeat < 140;
  tgtH = EYE_H;
  switch (style) {
    case MS_HEADPHONES:           // eyes shut, nodding along
      tgtW = 28; tgtX = 0; tgtY = onBeat ? 3 : -2;
      break;
    case MS_DANCE:                // side to side, a hop on every beat
      tgtX = (beatNo % 2) ? 12 : -12; tgtY = onBeat ? -8 : 0;
      break;
    case MS_VISUALIZER:           // the eyes step aside for the bars
      tgtX = 0; tgtY = 0;
      break;
  }
}

static void watchTargets(uint32_t now) {
  // Eyes up at the screen, following the action a little
  if (reached(now, nextDrift)) {
    tgtX = random(-6, 7);
    nextDrift = now + random(2500, 6000);
  }
  tgtY = -8;
  tgtH = 34;
  if (reached(now, nextEat)) {
    eatStart = now;
    nextEat = now + random(3500, 7000);
  }
  uint32_t eatT = now - eatStart;
  if (eatStart && eatT > 450 && eatT < 1150) tgtH = 28;   // chewing, content
  blinkNaturally(now, 3000, 7000);
}

// ---------- Coding and gaming ----------

// Headphones on over whatever it's doing: a nod on every beat
static void nodToMusic(uint32_t now) {
  if (world.media != MEDIA_MUSIC) return;
  tickBeat(now);
  if (now - lastBeat < 140) tgtY += 2;
}

static void codeTargets(uint32_t now) {
  tgtW = 32; tgtH = 24;
  if (world.typing) {
    // Eyes on the keys, paws going
    tgtX = 0; tgtY = -4;
    if (reached(now, nextKey)) {
      pawTap(now);
      nextKey = now + random(70, 190);
    }
  } else {
    // Reading: along a line, a quick hop back, and on down the page
    uint32_t t = now - lineStart;
    if (t > lineLen) {
      lineStart = now;
      lineLen = random(1200, 2400);
      lineNo++;
      t = 0;
    }
    tgtX = -7 + 14 * (int32_t)t / (int32_t)lineLen;
    tgtY = -7 + lineNo % 3;
  }
  blinkNaturally(now, 3500, 8000);
  nodToMusic(now);
}

static void gameTargets(uint32_t now) {
  // Darting after the action, barely blinking, thumbs never still
  if (reached(now, nextDart)) {
    tgtX = random(-12, 13);
    nextDart = now + random(250, 900);
  }
  tgtW = 34; tgtY = -8;
  tgtH = gameWhoa(now) ? 32 : 22;
  if (reached(now, nextPress)) {
    controllerPress(now);
    nextPress = now + random(90, 280);
  }
  blinkNaturally(now, 5000, 10000);
  nodToMusic(now);
}

static void emoteTargets() {
  tgtX = 0;
  tgtY = 0;
  tgtH = EYE_H;
  switch (emote) {
    case EM_HAPPY:     tgtY = -4; break;
    case EM_SURPRISED: tgtY = -2; tgtH = 42; tgtW = 32; break;
    case EM_SAD:       tgtY = 6;  tgtH = 30; break;
    case EM_ANGRY:     tgtY = 2;  tgtH = 28; tgtW = 38; break;
    case EM_WINK:      tgtY = -2; break;
    case EM_SLEEPY:    tgtY = 8;  tgtH = 6;  break;
    default: break;
  }
}

void faceUpdate(uint32_t now) {
  if (emote != EM_NONE && reached(now, emoteUntil)) emote = EM_NONE;
  if (mood == HAPPY && reached(now, happyUntil)) mood = NORMAL;

  cameraTick(now);
  if (aminalBusy() || world.media != MEDIA_NONE || world.act != ACT_NONE ||
      world.cam != CAM_NONE) faceWake(now);

  // Something new started playing
  if (world.media != shownMedia) {
    shownMedia = world.media;
    if (shownMedia == MEDIA_MUSIC) pickStyle(now);
    if (shownMedia == MEDIA_WATCH) {
      eatStart = 0;
      nextEat = now + random(1500, 3000);
      nextDrift = now;
    }
  }

  uint32_t sleepAfter = isNight() ? NIGHT_SLEEP_MS : SLEEP_AFTER_MS;
  if (mood == NORMAL && now - lastInteraction > sleepAfter) mood = SLEEPY;

  tgtW = EYE_W;
  if (emote != EM_NONE) {
    emoteTargets();
  } else if (mood == HAPPY) {
    tgtX = 0; tgtY = -4; tgtH = EYE_H;
  } else if (world.am == AM_LISTENING) {
    // Wide open and paying attention; the eyes swell with your voice
    tgtX = 0; tgtY = 0; tgtW = 38;
    tgtH = 38 + world.level * 8 / 100;
    blinkNaturally(now, 4000, 8000);
  } else if (world.am == AM_THINKING) {
    // Looking up and off to one side, switching sides now and then
    if (reached(now, nextThink)) {
      thinkSide = -thinkSide;
      nextThink = now + random(900, 1600);
    }
    tgtX = 14 * thinkSide; tgtY = -9; tgtH = 26;
  } else if (world.am == AM_SPEAKING) {
    // Eyes up to make room for the mouth, bobbing with the voice
    tgtX = 0;
    tgtY = -6 - world.level * 3 / 100;
    tgtH = 34 - world.level * 4 / 100;
    blinkNaturally(now, 3000, 7000);
  } else if (world.act == ACT_CODE) {
    codeTargets(now);
  } else if (world.act == ACT_GAME) {
    gameTargets(now);
  } else if (world.media == MEDIA_MUSIC) {
    musicTargets(now);
  } else if (world.media == MEDIA_WATCH) {
    watchTargets(now);
  } else if (mood == SLEEPY) {
    tgtX = 0; tgtY = 8; tgtH = 6;
  } else {
    idleTargets(now);
  }

  lensH += (tgtH + 6 - lensH) * 0.25f;      // before a blink shuts the target

  if (blinking && reached(now, blinkEnd)) blinking = false;
  if (blinking) tgtH = 2;

  // Ease toward the target. Blinks close fast, everything else glides.
  float speed = (blinking || tgtH < curH - 10) ? 0.6f : 0.25f;
  if (world.am == AM_LISTENING) speed = 0.45f;    // keep up with the voice
  curX += (tgtX - curX) * 0.25f;
  curY += (tgtY - curY) * 0.25f;
  curW += (tgtW - curW) * 0.25f;
  curH += (tgtH - curH) * speed;

  float want = world.am == AM_SPEAKING ? world.level / 100.0f : 0.0f;
  mouth += (want - mouth) * 0.5f;
}

// ---------- Drawing ----------

enum Look : uint8_t { LOOK_PLAIN, LOOK_HAPPY, LOOK_LOVE, LOOK_SAD, LOOK_ANGRY, LOOK_CLOSED,
                      LOOK_FOCUS };

static Look currentLook() {
  switch (emote) {
    case EM_HAPPY: return LOOK_HAPPY;
    case EM_LOVE:  return LOOK_LOVE;
    case EM_SAD:   return LOOK_SAD;
    case EM_ANGRY: return LOOK_ANGRY;
    case EM_NONE:  break;
    default:       return LOOK_PLAIN;
  }
  if (mood == HAPPY) return LOOK_HAPPY;
  if (actShown()) {
    // Game face: lids slanted in, until something big makes them pop open
    return world.act == ACT_GAME && !gameWhoa(millis()) ? LOOK_FOCUS : LOOK_PLAIN;
  }
  if (mediaShown() && world.media == MEDIA_MUSIC) {
    // Lost in it, with a peek around every nine seconds or so
    if (style == MS_HEADPHONES) return (millis() / 1500) % 6 == 0 ? LOOK_PLAIN : LOOK_HAPPY;
    if (style == MS_DANCE) return LOOK_HAPPY;
  }
  return LOOK_PLAIN;
}

static void drawHeart(int cx, int cy, int s) {
  int r = s / 4;
  display.fillCircle(cx - r, cy - r / 2, r, SSD1306_WHITE);
  display.fillCircle(cx + r, cy - r / 2, r, SSD1306_WHITE);
  display.fillTriangle(cx - 2 * r, cy - r / 2, cx + 2 * r, cy - r / 2,
                       cx, cy + 2 * r, SSD1306_WHITE);
}

// side is -1 for the left eye, +1 for the right
static void drawEye(int cx, int cy, int w, int h, int side, Look look, uint32_t now) {
  if (look == LOOK_LOVE) {
    int beat = (now / 150) % 4 == 0 ? 4 : 0;       // a heartbeat
    drawHeart(cx, cy - 2, 34 + beat);
    return;
  }
  if (look == LOOK_CLOSED) {
    display.fillRoundRect(cx - w / 2, cy + 6, w, 4, 2, SSD1306_WHITE);
    return;
  }

  if (h < 2) h = 2;
  int r = min(EYE_R, h / 2);
  int x0 = cx - w / 2, y0 = cy - h / 2;
  display.fillRoundRect(x0, y0, w, h, r, SSD1306_WHITE);

  if (look == LOOK_HAPPY) {
    // Cut a curve out of the bottom so the eye becomes a happy "^".
    // Placed from the full eye height, so a half-open eye waking from sleep
    // isn't swallowed by the cutout while it grows.
    display.fillCircle(cx, cy + EYE_H / 2 + w / 4, (w * 6) / 10, SSD1306_BLACK);
  } else if (look == LOOK_SAD) {
    // Outer corners droop
    int outer = side < 0 ? x0 - 1 : x0 + w;
    int across = side < 0 ? x0 + w * 7 / 10 : x0 + w * 3 / 10;
    display.fillTriangle(outer, y0 - 1, across, y0 - 1, outer, y0 + h / 2, SSD1306_BLACK);
  } else if (look == LOOK_ANGRY) {
    // Inner corners pulled down into a frown
    int inner = side < 0 ? x0 + w : x0 - 1;
    int across = side < 0 ? x0 + w * 3 / 10 : x0 + w * 7 / 10;
    display.fillTriangle(inner, y0 - 1, across, y0 - 1, inner, y0 + h / 2, SSD1306_BLACK);
  } else if (look == LOOK_FOCUS) {
    // A lid slanting down across the whole eye toward the nose
    int inner = side < 0 ? x0 + w : x0 - 1;
    int outer = side < 0 ? x0 - 1 : x0 + w;
    display.fillTriangle(outer, y0 - 1, inner, y0 - 1, inner, y0 + h / 3, SSD1306_BLACK);
  }
}

void faceDraw(uint32_t now) {
  // A selfie takes the whole face over, Aminal's speaking face included
  if (world.cam != CAM_NONE) {
    cameraDraw(now);
    return;
  }
  bool media = mediaShown();
  bool plain = emote == EM_NONE && mood != HAPPY;
  if (media && plain && world.media == MEDIA_MUSIC && style == MS_VISUALIZER) {
    visualizerDraw(now);
    return;
  }

  Look look = currentLook();
  int cy = SCREEN_H / 2 + (int)curY;
  int w = (int)curW, h = (int)curH;
  int lx = SCREEN_W / 2 - EYE_GAP / 2 - EYE_W / 2 + (int)curX;
  int rx = SCREEN_W / 2 + EYE_GAP / 2 + EYE_W / 2 + (int)curX;

  bool act = actShown();
  if (act && world.act == ACT_CODE) {        // under the eyes, in case a big emote overlaps
    keyboardDraw();
    pawsDraw(now, world.typing);
    glyphsDraw();
  }

  drawEye(lx, cy, w, h, -1, look, now);
  drawEye(rx, cy, w, h, +1, emote == EM_WINK ? LOOK_CLOSED : look, now);

  if (act) {
    if (world.act == ACT_CODE) {
      glassesDraw(lx, rx, cy, w, (int)lensH);
    } else {
      bool jolt = world.jumpAt && now - world.jumpAt < 150;
      controllerDraw((int)curX / 4, jolt ? -2 : 0, now);
      if (world.jumpAt) sparksDraw(now - world.jumpAt);
    }
    if (world.media == MEDIA_MUSIC) drawHeadphones(lx, rx, cy, w);
  } else if (media && world.media == MEDIA_MUSIC) {
    if (style == MS_HEADPHONES) drawHeadphones(lx, rx, cy, w);
    if (style == MS_DANCE) notesDraw();
  } else if (media && world.media == MEDIA_WATCH) {
    bucketDraw();
    int mouthX = SCREEN_W / 2 + (int)curX, mouthY = cy + h / 2 + 5;
    uint32_t eatT = now - eatStart;
    if (eatStart && plain) {
      kernelDraw(eatT, mouthX, mouthY);
      if (eatT >= 450 && eatT < 1150) chewDraw(now, mouthX, mouthY);
    }
    if (world.jumpAt) burstDraw(now - world.jumpAt);
  }

  bool asleep = emote == EM_SLEEPY || (emote == EM_NONE && mood == SLEEPY
                && world.am != AM_LISTENING && world.am != AM_THINKING
                && world.am != AM_SPEAKING);

  if (asleep) {
    // Little floating z that bobs up and down
    int bob = (now / 400) % 3;
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(112, 8 - bob);
    display.print("z");
    display.setCursor(118, 2 - bob / 2);
    display.print("z");

    // and the time, small, for anyone glancing over at night
    struct tm t;
    if (clockNow(t)) {
      char buf[8];
      int hr = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
      snprintf(buf, sizeof(buf), "%d:%02d", hr, t.tm_min);
      display.setCursor(0, 0);
      display.print(buf);
    }
  } else if (emote == EM_NONE && mood != HAPPY && world.am == AM_THINKING) {
    // Three dots filling in, bottom right
    int phase = (now / 250) % 4;
    for (int i = 0; i < 3; i++) {
      display.fillCircle(104 + i * 8, 57, i < phase ? 2 : 1, SSD1306_WHITE);
    }
  } else if (emote == EM_NONE && mood != HAPPY && world.am == AM_SPEAKING) {
    // A mouth that opens with the voice
    int mh = 2 + (int)(mouth * 10);
    int mw = 14 + (int)(mouth * 8);
    int my = cy + h / 2 + 5;
    display.fillRoundRect(SCREEN_W / 2 - mw / 2 + (int)curX, my, mw, mh,
                          min(mh / 2, 5), SSD1306_WHITE);
  } else if (emote == EM_SURPRISED) {
    display.fillCircle(SCREEN_W / 2 + (int)curX, cy + h / 2 + 6, 3, SSD1306_WHITE);
  }
}
