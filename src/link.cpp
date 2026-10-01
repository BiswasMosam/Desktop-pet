// The link to Aminal: short text commands, one per line, from any port.
//
// USB, the HC-05 and the WiFi module (port 7676) speak exactly the same
// protocol, so the pet doesn't care which one Aminal is on. Replies go back down the port the command came in
// on; events (a tap, a hold) go to whichever port spoke last.
//
//   HI                          -> PET desktop-pet 11
//   RF [test|poll]              -> RF <RC522 chip version, hex> | RF -;
//                               test: SPI read-back; poll: one try at a card
//   IP                          -> IP <its WiFi address> | IP -
//   PG                          -> PO            (heartbeat)
//   ST idle|listening|thinking|speaking|off
//   LV <0-100>                  voice level
//   TM <unix> <utc offset s>
//   WX <temp> <wmo code> <hi> <lo> <place>
//   TI <left s> <total s> <paused 0|1> <label>   or   TI -
//   AL <secs> <title>|<text>    or   AL -   (take it down)
//   EM <happy|love|surprised|sad|angry|wink|sleepy> [ms]
//   GO <face|clock|weather|timer|usage|status> [secs]
//   CU <session %> <its reset, unix> <week %> <its reset, unix>   Claude usage
//   SN                          -> SN <the 1024-byte frame buffer as hex>
//   MD music [headphones|dance|bars] | watch | -   what's playing on the PC
//   VZ <16 hex digits>          spectrum bars, 0-f each, low to high
//   BE                          a beat
//   JS                          a sudden loud moment in a film or a game
//   AC code [typing] | game | -   what's in front on the PC
//   SF ready | count <ms> | shot | -   a selfie: the camera opening, the
//                               spoken countdown, the shutter, done
//   LO <lat> <lon> <place>      where the weather is for (kept across power cuts)
//   WF <ssid>\t<password>      USB only: join a WiFi network (the ESP-01 keeps it)
//   ES flash|talk [baud] | reset   USB only: pass USB straight through to the
//                               ESP-01 (in its bootloader, or its own program)
//                               until 12-30 s of quiet; or just reset it.
//                               Each reset first answers "ES boot <line>":
//                               the ROM's own start-up line, read at 74880

#include "pet.h"

// HC-05 on USART2: PA2 -> HC-05 RXD, PA3 <- HC-05 TXD
static Uart SerialBT(PA_3, PA_2);
#define BT_BAUD 9600

// The ESP-01 (USART1, SerialESP) is driven by wifi.cpp in its own AT
// language, not this protocol. Here are only its reset and boot pins, for
// talking to it directly from the PC ("ES"): GPIO0 held low while it comes
// out of reset puts it in its ROM bootloader.
#define ESP_RST_PIN  PB12
#define ESP_BOOT_PIN PB13
#define ESP_EN_PIN   PA8    // its enable: held high from here, not a 3V3 pin

const uint32_t LINK_TIMEOUT_MS = 8000;    // no line for this long -> on our own

struct Port {
  Stream  *s;
  LinkKind kind;
  char     buf[160];
  uint8_t  n;
  bool     overflow;
};

static Port ports[] = {
  {&Serial,   LINK_USB,  {0}, 0, false},
  {&SerialBT, LINK_BT,   {0}, 0, false},
  {&wifiLink, LINK_WIFI, {0}, 0, false},
};
static Port *replyTo = nullptr;   // where the current command came from
static Port *active  = nullptr;   // where events go

void linkBegin() {
  Serial.begin(115200);           // USB CDC; the baud rate is ignored
  SerialBT.begin(BT_BAUD);
  pinMode(ESP_RST_PIN, OUTPUT);
  pinMode(ESP_BOOT_PIN, OUTPUT);
  digitalWrite(ESP_RST_PIN, HIGH);
  digitalWrite(ESP_BOOT_PIN, HIGH);
  pinMode(ESP_EN_PIN, OUTPUT);
  digitalWrite(ESP_EN_PIN, HIGH);
}

// ---------- Reflashing the ESP-01 through the pet ----------

// Reset it, in its bootloader (GPIO0 low, and held low: several ESP-01
// guides keep it grounded for the whole flash) or its own program.
void espReset(bool bootloader) {
  digitalWrite(ESP_BOOT_PIN, bootloader ? LOW : HIGH);
  digitalWrite(ESP_RST_PIN, LOW);
  delay(60);
  digitalWrite(ESP_RST_PIN, HIGH);
}

// The ESP8266's ROM prints why it started ("rst cause:2, boot mode:(1,7)")
// at 74880 baud - 115200 scaled by its 26 MHz crystal - which reads as
// garbage at any other speed. So listen at 74880 through the reset and
// hand the PC that line in plain text.
static void espResetAndReport(bool bootloader) {
  SerialESP.end();
  SerialESP.begin(74880);
  while (SerialESP.available()) SerialESP.read();
  espReset(bootloader);
  char line[160];
  int n = 0;
  uint32_t until = millis() + 350;
  while ((int32_t)(millis() - until) < 0) {
    while (SerialESP.available() && n < (int)sizeof(line) - 1) {
      char c = SerialESP.read();
      line[n++] = (c >= 32 && c < 127) ? c : ' ';
    }
  }
  line[n] = 0;
  Serial.print("ES boot ");
  Serial.print(line);
  Serial.print('\n');
}

// USB <-> ESP-01, byte for byte, until the PC has been quiet for a while.
// "ES flash [baud]" starts it in the bootloader for esptool; "ES talk
// [baud]" in its own program, to type at it. Nothing else runs meanwhile.
static void espPassthrough(bool bootloader, uint32_t baud) {
  display.clearDisplay();
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 16);
  display.print(bootloader ? "Updating my WiFi..." : "Talking to my WiFi");
  display.setCursor(10, 34);
  display.print(bootloader ? "don't unplug me" : "(quiet 30 s to end)");
  display.display();

  espResetAndReport(bootloader);
  // Changing the UART's speed lets go of the line for a moment, and the
  // ROM picks its baud rate from the narrowest pulse it sees - a blip on a
  // floating wire is narrower than any real bit. So reset it once more,
  // at the new speed, and let the SYNC be the first thing it hears.
  SerialESP.end();
  SerialESP.begin(baud);
  delay(20);
  espReset(bootloader);
  delay(250);                                  // its boot chatter, at 74880
  while (SerialESP.available()) SerialESP.read();
  Serial.print(bootloader ? "ES flashing\n" : "ES talking\n");

  uint8_t buf[64];
  uint32_t last = millis();
  bool started = false;
  while (true) {
    int n = Serial.available();
    int room = SerialESP.availableForWrite();
    if (n > 0 && room > 0) {                  // only as fast as the UART takes it
      n = min(n, min(room, (int)sizeof(buf)));
      for (int i = 0; i < n; i++) buf[i] = Serial.read();
      SerialESP.write(buf, n);
      last = millis();
      started = true;
    }
    n = SerialESP.available();
    if (n > 0) {
      n = min(n, (int)sizeof(buf));
      for (int i = 0; i < n; i++) buf[i] = SerialESP.read();
      Serial.write(buf, n);
      last = millis();
    }
    // esptool pauses while flash is erased, so give it room before giving up
    uint32_t quiet = millis() - last;
    if (quiet > (bootloader ? (started ? 12000UL : 30000UL) : 30000UL)) break;
  }
  SerialESP.end();
  SerialESP.begin(ESP_BAUD);
  espReset(false);                             // back to its own program
  wifiRestart(millis());                       // and wifi.cpp starts over
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

// AC code [typing] | AC game | AC -
static void setActivity(char *a) {
  char *more = strchr(a, ' ');
  if (more) *more++ = 0;
  if      (!strcmp(a, "code")) world.act = ACT_CODE;
  else if (!strcmp(a, "game")) world.act = ACT_GAME;
  else                         world.act = ACT_NONE;
  world.typing = more && !strcmp(more, "typing");
}

// SF ready | SF count <ms> | SF shot | SF -
static void setSelfie(char *a, uint32_t now) {
  char *ms = strchr(a, ' ');
  if (ms) *ms++ = 0;
  if      (!strcmp(a, "ready")) world.cam = CAM_READY;
  else if (!strcmp(a, "count")) {
    world.cam = CAM_COUNT;
    world.camCountMs = ms ? constrain(atoi(ms), 600, 6000) : 2100;
  }
  else if (!strcmp(a, "shot"))  world.cam = CAM_SHOT;
  else { world.cam = CAM_NONE; return; }
  world.camAt = now;
  goScreen(SCR_FACE, 0, now);
}

// CU <session %> <reset unix> <week %> <reset unix>
static void setUsage(char *a, uint32_t now) {
  char *p = a;
  long session = strtol(p, &p, 10);
  unsigned long sessionReset = strtoul(p, &p, 10);
  long week = strtol(p, &p, 10);
  unsigned long weekReset = strtoul(p, &p, 10);
  world.cuSession = constrain(session, 0, 100);
  world.cuWeek = constrain(week, 0, 100);
  world.cuSessionReset = sessionReset;
  world.cuWeekReset = weekReset;
  world.cuAt = now;
  world.cuValid = true;
}

static void setBars(const char *a, uint32_t now) {
  for (int i = 0; i < VZ_BARS && a[i]; i++) {
    char c = a[i];
    world.vz[i] = c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0';
    if (world.vz[i] > 15) world.vz[i] = 0;
  }
  world.vzAt = now;
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
  if      (!strcmp(cmd, "HI")) { reply("PET desktop-pet 11"); hello = true; }
  else if (!strcmp(cmd, "PG")) reply("PO");
  else if (!strcmp(cmd, "RF")) {
    char b[104];
    const char *r = !strcmp(args, "test") ? rfidTest() :
                    !strcmp(args, "poll") ? rfidPoll() : rfidStatus();
    snprintf(b, sizeof(b), "RF %s", r);
    reply(b);
  }
  else if (!strcmp(cmd, "IP")) {
    char b[24];
    snprintf(b, sizeof(b), "IP %s", world.wifi == 3 ? world.wifiIp : "-");
    reply(b);
  }
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
  else if (!strcmp(cmd, "JS")) {
    world.jumpAt = now;
    if (world.act == ACT_NONE) faceEmote(EM_SURPRISED, 1500, now);   // a game has its own
  }
  else if (!strcmp(cmd, "AC")) setActivity(args);
  else if (!strcmp(cmd, "SF")) setSelfie(args, now);
  else if (!strcmp(cmd, "CU")) setUsage(args, now);
  else if (!strcmp(cmd, "LO")) wifiSetPlace(args, now);
  else if (!strcmp(cmd, "WF") && p.kind == LINK_USB) wifiJoin(args);   // never over the air
  else if (!strcmp(cmd, "ES") && p.kind == LINK_USB) {
    // Only over the cable: a flash through Bluetooth would take an hour
    char *baud = strchr(args, ' ');
    if (baud) *baud++ = 0;
    uint32_t rate = baud ? strtoul(baud, nullptr, 10) : ESP_BAUD;
    if (rate < 1200) rate = ESP_BAUD;
    if      (!strcmp(args, "flash")) espPassthrough(true, rate);
    else if (!strcmp(args, "talk"))  espPassthrough(false, rate);
    else if (!strcmp(args, "reset")) espResetAndReport(false), SerialESP.end(), SerialESP.begin(ESP_BAUD);
    return;
  }
  else known = false;

  if (!known) {
    reply("? unknown");
    return;
  }
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
    world.media = MEDIA_NONE;      // nobody left to say when it stops
    world.act = ACT_NONE;
    world.cam = CAM_NONE;
    active = nullptr;
  }
  if (world.timerOn && !world.timerPaused && timerLeftMs(now) == 0) timerDone(now);
}
