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

  bool aminalBusy = world.am == AM_LISTENING || world.am == AM_THINKING
                 || world.am == AM_SPEAKING;
  if (aminalBusy) faceWake(now);

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
  } else if (mood == SLEEPY) {
    tgtX = 0; tgtY = 8; tgtH = 6;
  } else {
    idleTargets(now);
  }

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

enum Look : uint8_t { LOOK_PLAIN, LOOK_HAPPY, LOOK_LOVE, LOOK_SAD, LOOK_ANGRY, LOOK_CLOSED };

static Look currentLook() {
  switch (emote) {
    case EM_HAPPY: return LOOK_HAPPY;
    case EM_LOVE:  return LOOK_LOVE;
    case EM_SAD:   return LOOK_SAD;
    case EM_ANGRY: return LOOK_ANGRY;
    case EM_NONE:  break;
    default:       return LOOK_PLAIN;
  }
  return mood == HAPPY ? LOOK_HAPPY : LOOK_PLAIN;
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
  }
}

void faceDraw(uint32_t now) {
  Look look = currentLook();
  int cy = SCREEN_H / 2 + (int)curY;
  int w = (int)curW, h = (int)curH;
  int lx = SCREEN_W / 2 - EYE_GAP / 2 - EYE_W / 2 + (int)curX;
  int rx = SCREEN_W / 2 + EYE_GAP / 2 + EYE_W / 2 + (int)curX;

  drawEye(lx, cy, w, h, -1, look, now);
  drawEye(rx, cy, w, h, +1, emote == EM_WINK ? LOOK_CLOSED : look, now);

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
