// The screens beside the face: clock, weather, timer, status, and the alert
// card that slides down over whichever one is up.

#include "pet.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ---------- Text helpers ----------

// Default 6x8 font, top-left anchored
static void small(int x, int y, const char *s) {
  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(s);
}

static void smallCentered(int cx, int y, const char *s) {
  small(cx - (int)strlen(s) * 3, y, s);
}

static void smallRight(int right, int y, const char *s) {
  small(right - (int)strlen(s) * 6, y, s);
}

// A GFX font, centred on cx, sitting on a baseline. Returns the right edge.
static int bigCentered(const char *s, int cx, int baseline, const GFXfont *f) {
  display.setFont(f);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, baseline, &x1, &y1, &w, &h);
  int x = cx - w / 2 - x1;
  display.setCursor(x, baseline);
  display.print(s);
  display.setFont(nullptr);
  return x + x1 + w;
}

void bigDigit(const char *s, int cx, int baseline) {
  bigCentered(s, cx, baseline, &FreeSansBold12pt7b);
}

static const char *linkName() {
  switch (world.link) {
    case LINK_USB:  return "USB";
    case LINK_BT:   return "BT";
    case LINK_WIFI: return "WiFi";
    default:        return "";
  }
}

// Top line shared by the info screens: a title on the left, the link on the right
static void header(const char *title) {
  small(0, 0, title);
  smallRight(SCREEN_W, 0, linkName());
}

// ---------- Clock ----------

static const char *DAYS[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void drawClock(uint32_t now) {
  struct tm t;
  if (!clockNow(t)) {
    header("Clock");
    bigCentered("--:--", SCREEN_W / 2, 42, &FreeSansBold18pt7b);
    smallCentered(SCREEN_W / 2, 54, "waiting for time");
    return;
  }

  char date[16];
  snprintf(date, sizeof(date), "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
  header(date);

  int hr = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  char hm[8];
  snprintf(hm, sizeof(hm), "%d:%02d", hr, t.tm_min);
  int right = bigCentered(hm, SCREEN_W / 2 - 6, 42, &FreeSansBold18pt7b);
  small(right + 3, 34, t.tm_hour < 12 ? "am" : "pm");

  // A hairline that fills across the minute
  display.drawFastHLine(0, 62, SCREEN_W, SSD1306_WHITE);
  display.fillRect(0, 60, (t.tm_sec * SCREEN_W) / 60, 3, SSD1306_WHITE);
}

// ---------- Weather ----------

static void sun(int cx, int cy, int r, uint32_t now) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  float spin = (now % 12000) / 12000.0f * 2 * PI / 8;     // rays turn slowly
  for (int i = 0; i < 8; i++) {
    float a = spin + i * PI / 4;
    display.drawLine(cx + cosf(a) * (r + 3), cy + sinf(a) * (r + 3),
                     cx + cosf(a) * (r + 7), cy + sinf(a) * (r + 7), SSD1306_WHITE);
  }
}

static void moon(int cx, int cy, int r, uint32_t now) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  display.fillCircle(cx + r / 2, cy - r / 3, r - 1, SSD1306_BLACK);
  if ((now / 700) % 3) display.drawPixel(cx + r + 4, cy + r - 2, SSD1306_WHITE);
  if ((now / 900) % 2) display.drawPixel(cx - r + 1, cy - r - 3, SSD1306_WHITE);
}

// A filled cloud with a black rim, so it reads in front of a sun
static void cloud(int cx, int cy, uint16_t color) {
  display.fillCircle(cx - 8, cy + 2, 7, color);
  display.fillCircle(cx + 1, cy - 3, 9, color);
  display.fillCircle(cx + 10, cy + 3, 6, color);
  display.fillRoundRect(cx - 15, cy + 2, 31, 8, 4, color);
}

static void cloudRimmed(int cx, int cy) {
  display.fillCircle(cx - 8, cy + 2, 9, SSD1306_BLACK);
  display.fillCircle(cx + 1, cy - 3, 11, SSD1306_BLACK);
  display.fillCircle(cx + 10, cy + 3, 8, SSD1306_BLACK);
  cloud(cx, cy, SSD1306_WHITE);
}

static void drops(int cx, int top, int count, int len, uint32_t now) {
  int fall = (now / 70) % 8;
  for (int i = 0; i < count; i++) {
    int x = cx - (count - 1) * 5 + i * 10;
    int y = top + ((fall + i * 3) % 8);
    display.drawLine(x, y, x - 2, y + len, SSD1306_WHITE);
  }
}

static void flakes(int cx, int top, uint32_t now) {
  int fall = (now / 150) % 10;
  for (int i = 0; i < 3; i++) {
    int x = cx - 10 + i * 10 + ((fall + i) % 3) - 1;
    int y = top + ((fall + i * 4) % 10);
    display.fillCircle(x, y, 1, SSD1306_WHITE);
  }
}

static void bolt(int cx, int top, uint32_t now) {
  if ((now / 250) % 6 == 0) return;                      // flickers
  display.fillTriangle(cx + 2, top, cx - 5, top + 9, cx + 1, top + 9, SSD1306_WHITE);
  display.fillTriangle(cx - 1, top + 8, cx + 5, top + 8, cx - 3, top + 17, SSD1306_WHITE);
}

static void fog(int cx, int cy, uint32_t now) {
  int drift = (now / 200) % 6;
  for (int i = 0; i < 4; i++) {
    int shift = (i % 2 ? drift : -drift) / 2;
    display.fillRoundRect(cx - 15 + shift + (i % 2) * 4, cy - 9 + i * 6, 26, 3, 1, SSD1306_WHITE);
  }
}

static bool isNightHour() {
  struct tm t;
  if (!clockNow(t)) return false;
  return t.tm_hour >= 19 || t.tm_hour < 6;
}

// WMO weather code -> a little animated picture, centred on (cx, cy)
static void weatherIcon(uint8_t code, int cx, int cy, uint32_t now) {
  bool night = isNightHour();
  if (code == 0) {
    if (night) moon(cx, cy, 10, now); else sun(cx, cy, 8, now);
  } else if (code <= 2) {
    if (night) moon(cx - 6, cy - 7, 8, now); else sun(cx - 6, cy - 7, 6, now);
    cloudRimmed(cx + 3, cy + 3);
  } else if (code == 3) {
    cloud(cx, cy, SSD1306_WHITE);
  } else if (code == 45 || code == 48) {
    fog(cx, cy, now);
  } else if (code >= 51 && code <= 57) {
    cloud(cx, cy - 6, SSD1306_WHITE);
    drops(cx, cy + 7, 2, 3, now);
  } else if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
    cloud(cx, cy - 6, SSD1306_WHITE);
    drops(cx, cy + 7, 3, 5, now);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    cloud(cx, cy - 6, SSD1306_WHITE);
    flakes(cx, cy + 7, now);
  } else if (code >= 95) {
    cloud(cx, cy - 6, SSD1306_WHITE);
    bolt(cx, cy + 4, now);
  } else {
    cloud(cx, cy, SSD1306_WHITE);
  }
}

static const char *weatherWords(uint8_t code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mostly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return code >= 65 ? "Heavy rain" : "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "";
}

void drawWeather(uint32_t now) {
  if (!world.wxValid) {
    header("Weather");
    cloud(SCREEN_W / 2, 30, SSD1306_WHITE);
    smallCentered(SCREEN_W / 2, 50, "no weather yet");
    return;
  }

  header(world.wxPlace[0] ? world.wxPlace : "Weather");
  weatherIcon(world.wxCode, 28, 32, now);

  char temp[8];
  snprintf(temp, sizeof(temp), "%d", world.wxTemp);
  int right = bigCentered(temp, 86, 38, &FreeSansBold18pt7b);
  display.drawCircle(right + 4, 16, 3, SSD1306_WHITE);   // the degree sign

  char hilo[16];
  snprintf(hilo, sizeof(hilo), "H%d L%d", world.wxHi, world.wxLo);
  smallCentered(88, 43, hilo);

  const char *words = weatherWords(world.wxCode);
  bool stale = now - world.wxAt > 3UL * 3600 * 1000;     // over three hours old
  char line[24];
  snprintf(line, sizeof(line), stale ? "%s (old)" : "%s", words);
  smallCentered(SCREEN_W / 2, 56, line);
}

// ---------- Timer ----------

void drawTimer(uint32_t now) {
  if (!world.timerOn) {
    header("Timer");
    bigCentered("0:00", SCREEN_W / 2, 40, &FreeSansBold18pt7b);
    smallCentered(SCREEN_W / 2, 52, "no timer running");
    return;
  }

  header(world.timerLabel[0] ? world.timerLabel : "Timer");
  if (world.timerPaused) {
    // Just left of the link name the header put in the corner
    int link = strlen(linkName());
    smallRight(SCREEN_W - (link ? (link + 1) * 6 : 0), 0, "paused");
  }

  uint32_t left = (timerLeftMs(now) + 999) / 1000;       // round up, like a kitchen timer
  char buf[12];
  const GFXfont *font = &FreeSansBold18pt7b;
  if (left >= 3600) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", left / 3600, (left / 60) % 60, left % 60);
    font = &FreeSansBold12pt7b;
  } else {
    snprintf(buf, sizeof(buf), "%lu:%02lu", left / 60, left % 60);
  }

  // A paused timer blinks, the way an oven does
  if (!world.timerPaused || (now / 500) % 2) bigCentered(buf, SCREEN_W / 2, 42, font);

  // What's left, as a bar
  display.drawRoundRect(4, 52, SCREEN_W - 8, 8, 3, SSD1306_WHITE);
  if (world.timerTotal > 0) {
    uint32_t fill = ((uint64_t)(SCREEN_W - 12) * left) / world.timerTotal;
    if (fill > (uint32_t)(SCREEN_W - 12)) fill = SCREEN_W - 12;
    if (fill) display.fillRoundRect(6, 54, fill, 4, 1, SSD1306_WHITE);
  }
}

// ---------- Status ----------

static void ago(char *out, size_t n, uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60)        snprintf(out, n, "%lus ago", s);
  else if (s < 3600) snprintf(out, n, "%lum ago", s / 60);
  else               snprintf(out, n, "%luh ago", s / 3600);
}

void drawStatus(uint32_t now) {
  static const char *AM[] = {"offline", "idle", "listening", "thinking", "speaking"};
  char line[28], when[12];

  header("Desktop pet");

  // Six lines, nine pixels apart
  snprintf(line, sizeof(line), "Link    %s", world.link ? linkName() : "none");
  small(0, 10, line);

  snprintf(line, sizeof(line), "Aminal  %s", AM[world.am]);
  small(0, 19, line);

  static const char *WIFI[] = {"no module", "joining", "offline", ""};
  snprintf(line, sizeof(line), "WiFi    %s", world.wifi == 3 ? world.wifiIp : WIFI[world.wifi]);
  small(0, 28, line);

  if (world.timeValid) {
    ago(when, sizeof(when), now - world.millisAtSync);
    snprintf(line, sizeof(line), "Time    %s", when);
  } else {
    snprintf(line, sizeof(line), "Time    not set");
  }
  small(0, 37, line);

  if (world.wxValid) {
    ago(when, sizeof(when), now - world.wxAt);
    snprintf(line, sizeof(line), "Weather %s", when);
  } else {
    snprintf(line, sizeof(line), "Weather none");
  }
  small(0, 46, line);

  uint32_t up = now / 1000;
  snprintf(line, sizeof(line), "Up      %luh %02lum", up / 3600, (up / 60) % 60);
  small(0, 55, line);
}

// ---------- Alert card ----------

void showAlert(const char *title, const char *text, uint32_t secs, bool loud, uint32_t now) {
  // A ringing timer outranks a passing notification
  if (world.alertOn && world.alertLoud && !loud) return;
  strncpy(world.alertTitle, title, sizeof(world.alertTitle) - 1);
  world.alertTitle[sizeof(world.alertTitle) - 1] = 0;
  strncpy(world.alertText, text, sizeof(world.alertText) - 1);
  world.alertText[sizeof(world.alertText) - 1] = 0;
  world.alertOn = true;
  world.alertLoud = loud;
  world.alertStart = now;
  world.alertUntil = now + secs * 1000;
  faceWake(now);
}

void dismissAlert() {
  world.alertOn = false;
  world.alertLoud = false;
  display.invertDisplay(false);
}

// Word-wrap into lines of at most `width` characters
static int wrap(const char *text, char lines[][22], int maxLines, int width) {
  int count = 0;
  const char *p = text;
  while (*p && count < maxLines) {
    while (*p == ' ') p++;
    int len = strlen(p);
    int take = len <= width ? len : width;
    if (len > width) {
      int cut = take;
      while (cut > 0 && p[cut] != ' ') cut--;
      if (cut > 0) take = cut;
    }
    memcpy(lines[count], p, take);
    lines[count][take] = 0;
    count++;
    p += take;
  }
  return count;
}

// A bell, shaking side to side, with ring marks on the side it swings to
static void bell(int cx, int top, uint32_t now) {
  int swing = (now / 90) % 2 ? -2 : 2;
  cx += swing;
  display.fillCircle(cx, top + 1, 1, SSD1306_WHITE);              // handle
  display.fillCircle(cx, top + 8, 7, SSD1306_WHITE);              // dome
  display.fillRect(cx - 7, top + 8, 15, 6, SSD1306_WHITE);        // body
  display.fillRoundRect(cx - 10, top + 13, 21, 3, 1, SSD1306_WHITE);  // rim
  display.fillCircle(cx - swing, top + 18, 2, SSD1306_WHITE);     // clapper
  int side = swing > 0 ? 1 : -1;
  display.drawLine(cx + side * 13, top + 3, cx + side * 16, top, SSD1306_WHITE);
  display.drawLine(cx + side * 14, top + 8, cx + side * 18, top + 8, SSD1306_WHITE);
}

void drawAlert(uint32_t now) {
  uint32_t shown = now - world.alertStart;

  if (world.alertLoud) {
    // The whole screen: this one has to be seen from across the room
    display.fillRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_BLACK);
    bell(SCREEN_W / 2, 2, now);
    bigCentered(world.alertTitle, SCREEN_W / 2, 44, &FreeSansBold12pt7b);
    smallCentered(SCREEN_W / 2, 54, world.alertText);
    display.invertDisplay((now / 500) % 2);     // and it flashes
    return;
  }

  // Slides down from the top over 150 ms
  int drop = shown < 150 ? (int)(shown * 64 / 150) - 64 : 0;

  display.fillRoundRect(0, drop, SCREEN_W, SCREEN_H, 6, SSD1306_BLACK);
  display.drawRoundRect(0, drop, SCREEN_W, SCREEN_H, 6, SSD1306_WHITE);
  display.fillRoundRect(0, drop, SCREEN_W, 13, 6, SSD1306_WHITE);
  display.fillRect(0, drop + 7, SCREEN_W, 6, SSD1306_WHITE);

  display.setFont(nullptr);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(6, drop + 3);
  display.print(world.alertTitle);

  static char lines[8][22];
  int n = wrap(world.alertText, lines, 8, 20);
  // Four lines fit; longer text scrolls a line every 1.5 s
  int first = 0;
  if (n > 4) first = (shown / 1500) % (n - 3);
  for (int i = 0; i < 4 && first + i < n; i++) small(4, drop + 17 + i * 11, lines[first + i]);
}
