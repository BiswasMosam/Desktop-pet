// The link to Aminal: short text commands, one per line, from any port.
//
// USB and the HC-05 speak exactly the same protocol, so the pet doesn't care
// which one Aminal is on. Replies go back down the port the command came in
// on; events (a tap, a hold) go to whichever port spoke last.
//
//   HI                          -> PET desktop-pet 2
//   PG                          -> PO            (heartbeat)
//   ST idle|listening|thinking|speaking|off
//   LV <0-100>                  voice level
//   TM <unix> <utc offset s>
//   WX <temp> <wmo code> <hi> <lo> <place>
//   TI <left s> <total s> <paused 0|1> <label>   or   TI -
//   AL <secs> <title>|<text>    or   AL -   (take it down)
//   EM <happy|love|surprised|sad|angry|wink|sleepy> [ms]
//   GO <face|clock|weather|timer|status> [secs]
//   SN                          -> SN <the 1024-byte frame buffer as hex>
//   MD music [headphones|dance|bars] | watch | -   what's playing on the PC
//   VZ <16 hex digits>          spectrum bars, 0-f each, low to high
//   BE                          a beat
//   JS                          a sudden loud moment in a film
//   LO <lat> <lon> <place>      where the weather is for; passed on to the ESP-01
//   NW joining|setup <ap>|ok <ip>|off     the ESP-01's WiFi (from the ESP-01)

#include "pet.h"

// HC-05 on USART2: PA2 -> HC-05 RXD, PA3 <- HC-05 TXD
static Uart SerialBT(PA_3, PA_2);
#define BT_BAUD 9600

// ESP-01 on USART1: PA9 -> ESP RX, PA10 <- ESP TX. It sends the time and
// weather it fetched itself, and relays Aminal when Aminal comes over WiFi.
static Uart SerialESP(PA_10, PA_9);
#define ESP_BAUD 115200

const uint32_t LINK_TIMEOUT_MS = 8000;    // no line for this long -> on our own

struct Port {
  Stream  *s;
  LinkKind kind;
  char     buf[160];
  uint8_t  n;
  bool     overflow;
};

static Port ports[] = {
  {&Serial,    LINK_USB,  {0}, 0, false},
  {&SerialBT,  LINK_BT,   {0}, 0, false},
  {&SerialESP, LINK_WIFI, {0}, 0, false},
};
static Port *replyTo = nullptr;   // where the current command came from
static Port *active  = nullptr;   // where events go

void linkBegin() {
  Serial.begin(115200);           // USB CDC; the baud rate is ignored
  SerialBT.begin(BT_BAUD);
  SerialESP.begin(ESP_BAUD);
}

static void reply(const char *text) {
  if (!replyTo) return;
  replyTo->s->print(text);
  replyTo->s->print('\n');
}

void linkEvent(const char *text) {
  if (!active || world.link == LINK_NONE) return;
  active->s->print(text);
  active->s->print('\n');
}

// ---------- Commands ----------

static void setState(const char *a, uint32_t now) {
  AmState was = world.am;
  if      (!strcmp(a, "listening")) world.am = AM_LISTENING;
  else if (!strcmp(a, "thinking"))  world.am = AM_THINKING;
  else if (!strcmp(a, "speaking"))  world.am = AM_SPEAKING;
  else if (!strcmp(a, "off"))       world.am = AM_OFF;
  else                              world.am = AM_IDLE;
  if (world.am != AM_SPEAKING && world.am != AM_LISTENING) world.level = 0;

  // Someone's talking to Aminal: come back to the face to watch
  if (world.am == AM_LISTENING && was != AM_LISTENING && screen != SCR_FACE) {
    goScreen(SCR_FACE, 0, now);
  }
}

static void setTime(char *a, uint32_t now) {
  char *end;
  uint32_t epoch = strtoul(a, &end, 10);
  if (end == a || epoch < 1600000000UL) return;           // nonsense, keep what we had
  world.epochAtSync  = epoch;
  world.tzOffset     = strtol(end, nullptr, 10);
  world.millisAtSync = now;
  world.timeValid    = true;
}

static void setWeather(char *a, uint32_t now) {
  char *p = a;
  long temp = strtol(p, &p, 10);
  long code = strtol(p, &p, 10);
  long hi   = strtol(p, &p, 10);
  long lo   = strtol(p, &p, 10);
  while (*p == ' ') p++;
  world.wxTemp = temp;
  world.wxCode = code;
  world.wxHi   = hi;
  world.wxLo   = lo;
  strncpy(world.wxPlace, p, sizeof(world.wxPlace) - 1);
  world.wxPlace[sizeof(world.wxPlace) - 1] = 0;
  world.wxAt    = now;
  world.wxValid = true;
}

static void timerDone(uint32_t now) {
  world.timerOn = false;
  showAlert("Time's up", world.timerLabel[0] ? world.timerLabel : "Timer", 60, true, now);
  faceEmote(EM_SURPRISED, 3000, now);
}

static void setTimer(char *a, uint32_t now) {
  if (a[0] == '-') {
    // Gone from Aminal's list. If ours was about to ring, it finished;
    // otherwise somebody cancelled it.
    if (world.timerOn && !world.timerPaused && timerLeftMs(now) < 2500) timerDone(now);
    world.timerOn = false;
    return;
  }
  char *p = a;
  uint32_t left   = strtoul(p, &p, 10);
  uint32_t total  = strtoul(p, &p, 10);
  long     paused = strtol(p, &p, 10);
  while (*p == ' ') p++;
  world.timerOn     = true;
  world.timerLeftMs = left * 1000;
  world.timerSyncMs = now;
  world.timerTotal  = total;
  world.timerPaused = paused != 0;
  strncpy(world.timerLabel, p, sizeof(world.timerLabel) - 1);
  world.timerLabel[sizeof(world.timerLabel) - 1] = 0;
}

static void setAlert(char *a, uint32_t now) {
  if (a[0] == '-') {              // AL - takes the card down
    if (world.alertOn) dismissAlert();
    return;
  }
  char *p = a;
  uint32_t secs = strtoul(p, &p, 10);
  while (*p == ' ') p++;
  char *text = strchr(p, '|');
  if (text) *text++ = 0; else text = (char *)"";
  showAlert(p, text, secs ? secs : 6, false, now);
}

static void setEmote(char *a, uint32_t now) {
  static const char *NAMES[] = {"", "happy", "love", "surprised", "sad", "angry", "wink", "sleepy"};
  char *ms = strchr(a, ' ');
  if (ms) *ms++ = 0;
  for (uint8_t i = 1; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
    if (!strcmp(a, NAMES[i])) {
      faceEmote((Emote)i, ms ? strtoul(ms, nullptr, 10) : 3000, now);
      return;
    }
  }
}

static void setScreen(char *a, uint32_t now) {
  char *secs = strchr(a, ' ');
  if (secs) *secs++ = 0;
  for (uint8_t i = 0; i < SCR_COUNT; i++) {
    if (!strcmp(a, screenName((Screen)i))) {
      goScreen((Screen)i, secs ? strtoul(secs, nullptr, 10) * 1000 : 0, now);
      return;
    }
  }
}

// MD music [headphones|dance|bars] | MD watch | MD -
static void setMedia(char *a, uint32_t now) {
  char *style = strchr(a, ' ');
  if (style) *style++ = 0;
  if      (!strcmp(a, "music")) world.media = MEDIA_MUSIC;
  else if (!strcmp(a, "watch")) world.media = MEDIA_WATCH;
  else                          world.media = MEDIA_NONE;
  if (style && world.media == MEDIA_MUSIC) faceMusicStyle(style, now);
}

static void setBars(const char *a, uint32_t now) {
  for (int i = 0; i < VZ_BARS && a[i]; i++) {
    char c = a[i];
    world.vz[i] = c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0';
    if (world.vz[i] > 15) world.vz[i] = 0;
  }
  world.vzAt = now;
}

// NW joining | NW setup <ap name> | NW ok <ip> | NW off     (from the ESP-01)
static void setWifi(char *a, uint32_t now) {
  char *rest = strchr(a, ' ');
  if (rest) *rest++ = 0; else rest = (char *)"";
  uint8_t was = world.wifi;
  if      (!strcmp(a, "ok"))      world.wifi = 3;
  else if (!strcmp(a, "setup"))   world.wifi = 2;
  else if (!strcmp(a, "joining")) world.wifi = 1;
  else                            world.wifi = 0;
  strncpy(world.wifiIp, world.wifi == 3 ? rest : "", sizeof(world.wifiIp) - 1);
  world.wifiIp[sizeof(world.wifiIp) - 1] = 0;
  if (world.wifi == 2 && was != 2) {
    char text[80];
    snprintf(text, sizeof(text), "On your phone, join the WiFi \"%s\" to connect me",
             rest[0] ? rest : "Desktop-pet");
    showAlert("WiFi setup", text, 30, false, now);
  }
}

static void snapshot() {
  static const char HEX_DIGITS[] = "0123456789abcdef";
  uint8_t *b = display.getBuffer();
  Stream *s = replyTo->s;
  char chunk[64];
  int k = 0;
  s->print("SN ");
  for (int i = 0; i < SCREEN_W * SCREEN_H / 8; i++) {
    chunk[k++] = HEX_DIGITS[b[i] >> 4];
    chunk[k++] = HEX_DIGITS[b[i] & 15];
    if (k == sizeof(chunk)) { s->write(chunk, k); k = 0; }
  }
  if (k) s->write(chunk, k);
  s->print('\n');
}

static void handleLine(Port &p, uint32_t now) {
  char *cmd = p.buf;
  char *args = strchr(cmd, ' ');
  if (args) *args++ = 0; else args = cmd + strlen(cmd);
  replyTo = &p;

  bool hello = false, known = true;
  if      (!strcmp(cmd, "HI")) { reply("PET desktop-pet 2"); hello = true; }
  else if (!strcmp(cmd, "PG")) reply("PO");
  else if (!strcmp(cmd, "ST")) setState(args, now);
  else if (!strcmp(cmd, "LV")) world.level = constrain(atoi(args), 0, 100);
  else if (!strcmp(cmd, "TM")) setTime(args, now);
  else if (!strcmp(cmd, "WX")) setWeather(args, now);
  else if (!strcmp(cmd, "TI")) setTimer(args, now);
  else if (!strcmp(cmd, "AL")) setAlert(args, now);
  else if (!strcmp(cmd, "EM")) setEmote(args, now);
  else if (!strcmp(cmd, "GO")) setScreen(args, now);
  else if (!strcmp(cmd, "SN")) snapshot();
  else if (!strcmp(cmd, "MD")) setMedia(args, now);
  else if (!strcmp(cmd, "VZ")) setBars(args, now);
  else if (!strcmp(cmd, "BE")) world.beatAt = now;
  else if (!strcmp(cmd, "JS")) { world.jumpAt = now; faceEmote(EM_SURPRISED, 1500, now); }
  else if (!strcmp(cmd, "NW")) setWifi(args, now);
  else if (!strcmp(cmd, "LO")) {
    // Where the weather is for: Aminal knows, the ESP-01 needs it
    if (p.kind != LINK_WIFI) { SerialESP.print("LO "); SerialESP.print(args); SerialESP.print('\n'); }
  }
  else known = false;

  if (!known) {
    // Never answer the ESP-01's own chatter (boot noise at 74880 baud)
    if (p.kind != LINK_WIFI) reply("? unknown");
    return;
  }
  // The ESP-01 sends time and weather of its own; that isn't Aminal. Only
  // Aminal relayed over WiFi says HI and PG, so only those count as a link.
  if (p.kind == LINK_WIFI && !hello && strcmp(cmd, "PG")) return;
  bool wasAlone = world.link == LINK_NONE;
  world.link = p.kind;
  world.linkSeen = now;
  active = &p;
  if (hello && wasAlone) faceEmote(EM_HAPPY, 1500, now);   // someone's here
}

void linkPoll(uint32_t now) {
  for (Port &p : ports) {
    int budget = 512;   // don't let a flood starve the animation
    while (budget-- > 0 && p.s->available()) {
      char c = p.s->read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (!p.overflow && p.n) {
          p.buf[p.n] = 0;
          handleLine(p, now);
        }
        p.n = 0;
        p.overflow = false;
      } else if (p.n < sizeof(p.buf) - 1) {
        p.buf[p.n++] = c;
      } else {
        p.overflow = true;     // too long to be ours; drop the whole line
      }
    }
  }
}

void linkTick(uint32_t now) {
  if (world.link != LINK_NONE && now - world.linkSeen > LINK_TIMEOUT_MS) {
    // Aminal went quiet: the pet carries on by itself
    world.link = LINK_NONE;
    world.am = AM_OFF;
    world.level = 0;
    active = nullptr;
  }
  if (world.timerOn && !world.timerPaused && timerLeftMs(now) == 0) timerDone(now);
}
