// WiFi through the ESP-01's own factory firmware.
//
// The module came with Ai-Thinker's AT firmware (v1.1, 2016). Reflashing it
// through the pet failed - its ROM loader restarts on every esptool SYNC -
// so the pet drives the firmware it has, with plain text commands. The
// module remembers the network it was given (AT+CWJAP_DEF) and rejoins by
// itself; the pet then makes one plain-HTTP request to Open-Meteo every 20
// minutes, and that one reply carries everything it needs: the time (the
// Date header), the UTC offset, and today's weather.
//
// Nothing here blocks: every command is sent, then answered a line at a
// time from wifiTick(), so the face keeps moving while a request is out.

#include "pet.h"
#include <EEPROM.h>

Uart SerialESP(PA_10, PA_9);   // PA9 -> ESP RX, PA10 <- ESP TX
#define ESP_BAUD 115200

const uint32_t FETCH_EVERY_MS  = 20UL * 60 * 1000;
const uint32_t FETCH_RETRY_MS  = 5UL * 60 * 1000;
const uint32_t CHECK_ONLINE_MS = 60000;    // is it still connected?
const uint32_t CHECK_OFFLINE_MS = 10000;   // has it joined yet?

// ---------- Saved across power cuts ----------
// Where the weather is for (sent by Aminal) and its UTC offset, so a pet
// that wakes up with the PC off still knows both.

struct Saved {
  uint32_t magic;
  int32_t  lat4, lon4;        // degrees x 10000
  int32_t  offset;            // seconds east of UTC
  uint8_t  offsetKnown;
  char     place[20];
};
static const uint32_t SAVED_MAGIC = 0x5045544D;
static Saved saved;

static void loadSaved() {
  eeprom_buffer_fill();
  uint8_t *p = (uint8_t *)&saved;
  for (size_t i = 0; i < sizeof(saved); i++) p[i] = eeprom_buffered_read_byte(i);
  if (saved.magic != SAVED_MAGIC) memset(&saved, 0, sizeof(saved));
}

// One flash erase per save (a second or two), so only when something changed
static void storeSaved() {
  saved.magic = SAVED_MAGIC;
  eeprom_buffer_fill();
  const uint8_t *p = (const uint8_t *)&saved;
  for (size_t i = 0; i < sizeof(saved); i++) eeprom_buffered_write_byte(i, p[i]);
  eeprom_buffer_flush();
}

// ---------- Reading the module ----------

enum : uint8_t { R_OK = 1, R_ERR = 2, R_PROMPT = 4, R_SENT = 8, R_CONNECT = 16, R_CLOSED = 32 };
static uint8_t got;                  // what has come back since the last command

static char line[128];
static uint8_t lineLen;
static char http[1500];              // one whole reply: headers and body
static uint16_t httpLen;
static int16_t ipdLeft = -1;         // payload bytes of "+IPD,n:" still coming

static bool online = false;
static char ip[16] = "";
static int8_t modeDef = -1;          // the module's saved WiFi mode

static void onLine(uint32_t now);

static void espChar(char c, uint32_t now) {
  if (ipdLeft > 0) {                 // inside a +IPD payload: raw bytes
    if (httpLen < sizeof(http) - 1) http[httpLen++] = c;
    ipdLeft--;
    return;
  }
  if (c == '\n') {
    while (lineLen && line[lineLen - 1] == '\r') lineLen--;
    line[lineLen] = 0;
    if (lineLen) onLine(now);
    lineLen = 0;
    return;
  }
  if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
  line[lineLen] = 0;
  // Two things arrive without a newline: the send prompt, and +IPD's head
  if (lineLen == 1 && c == '>') {
    got |= R_PROMPT;
    lineLen = 0;
  } else if (c == ':' && !strncmp(line, "+IPD,", 5)) {
    ipdLeft = atoi(line + 5);
    lineLen = 0;
  }
}

// ---------- The conversation ----------

enum State : uint8_t { S_START, S_ECHO, S_MODEQ, S_MODE, S_MUX, S_IP, S_IDLE,
                       S_JOIN, S_TCP, S_LEN, S_RECV };
static State state = S_START;
static uint32_t deadline, nextStart, nextFetch, nextCheck;
static bool joinPending = false, everOnline = false;
static char joinCmd[180];
static char request[280];

static void send(const char *cmd, uint32_t timeout, uint32_t now) {
  got = 0;
  SerialESP.print(cmd);
  SerialESP.print("\r\n");
  deadline = now + timeout;
}

static void onLine(uint32_t now) {
  if      (!strcmp(line, "OK")) got |= R_OK;
  else if (!strcmp(line, "ERROR") || !strcmp(line, "FAIL") || strstr(line, "DNS Fail")) got |= R_ERR;
  else if (!strcmp(line, "SEND OK")) got |= R_SENT;
  else if (!strcmp(line, "CLOSED")) got |= R_CLOSED;
  else if (strstr(line, "CONNECT") && strncmp(line, "WIFI", 4)) got |= R_CONNECT;
  else if (!strcmp(line, "WIFI DISCONNECT")) { online = false; world.wifi = 1; }
  else if (!strncmp(line, "+CWMODE_DEF:", 12)) modeDef = atoi(line + 12);
  else if (!strncmp(line, "+CIFSR:STAIP,\"", 14)) {
    char *end = strchr(line + 14, '"');
    if (end) *end = 0;
    strncpy(ip, line + 14, sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = 0;
  } else if (!strcmp(line, "ready")) {
    // The module restarted (a power dip, or the pet reset it): start over
    state = S_START;
    nextStart = now + 500;
  }
}

static void askAddress(uint32_t now) {
  ip[0] = 0;
  send("AT+CIFSR", 3000, now);
  state = S_IP;
}

static void setOnline(bool on, uint32_t now) {
  bool was = online;
  online = on && ip[0] && strcmp(ip, "0.0.0.0");
  world.wifi = online ? 3 : (joinPending ? 1 : 2);
  strncpy(world.wifiIp, online ? ip : "", sizeof(world.wifiIp) - 1);
  if (online && !was) {
    nextFetch = now;                              // fetch at once
    char ev[32];
    snprintf(ev, sizeof(ev), "EV wifi ok %s", ip);
    linkEvent(ev);
    everOnline = true;
  }
}

static void appendEscaped(char *out, size_t size, const char *in) {
  size_t n = strlen(out);
  for (; *in && n + 2 < size; in++) {
    if (*in == '"' || *in == ',' || *in == '\\') out[n++] = '\\';
    out[n++] = *in;
  }
  out[n] = 0;
}

static void digits4(char *out, size_t size, int32_t v) {      // 192267 -> "19.2267"
  const char *sign = v < 0 ? "-" : "";
  if (v < 0) v = -v;
  snprintf(out, size, "%s%ld.%04ld", sign, (long)(v / 10000), (long)(v % 10000));
}

static void startFetch(uint32_t now) {
  char lat[16], lon[16];
  digits4(lat, sizeof(lat), saved.lat4);
  digits4(lon, sizeof(lon), saved.lon4);
  // HTTP/1.0 on purpose: the reply comes whole, not chunked
  snprintf(request, sizeof(request),
           "GET /v1/forecast?latitude=%s&longitude=%s"
           "&current=temperature_2m,weather_code"
           "&daily=temperature_2m_max,temperature_2m_min"
           "&forecast_days=1&timezone=auto HTTP/1.0\r\n"
           "Host: api.open-meteo.com\r\n\r\n", lat, lon);
  httpLen = 0;
  send("AT+CIPSTART=\"TCP\",\"api.open-meteo.com\",80", 10000, now);
  state = S_TCP;
}

// ---------- The reply ----------

static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

// "Mon, 28 Sep 2026 19:40:49 GMT" -> Unix time, 0 if it isn't one
static uint32_t parseHttpDate(const char *s) {
  static const char MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strchr(s, ',');
  if (!p) return 0;
  char *e;
  long day = strtol(p + 1, &e, 10);
  while (*e == ' ') e++;
  int month = 0;
  for (int i = 0; i < 12; i++) if (!strncmp(e, MONTHS + i * 3, 3)) month = i + 1;
  if (!month) return 0;
  long year = strtol(e + 3, &e, 10);
  long hh = strtol(e, &e, 10);
  long mm = strtol(e + 1, &e, 10);
  long ss = strtol(e + 1, &e, 10);
  if (year < 2024) return 0;
  return (uint32_t)daysFromCivil(year, month, day) * 86400UL + hh * 3600 + mm * 60 + ss;
}

static const char *after(const char *from, const char *key) {
  const char *p = from ? strstr(from, key) : nullptr;
  return p ? p + strlen(key) : nullptr;
}

static bool parseReply(uint32_t now) {
  http[httpLen] = 0;
  if (strncmp(http, "HTTP/1.", 7) || strncmp(http + 9, "200", 3)) return false;
  const char *body = strstr(http, "\r\n\r\n");
  const char *off = after(body, "\"utc_offset_seconds\":");
  const char *cur = after(body, "\"current\":{");
  const char *temp = after(cur, "\"temperature_2m\":");
  const char *code = after(cur, "\"weather_code\":");
  const char *hi = after(body, "\"temperature_2m_max\":[");
  const char *lo = after(body, "\"temperature_2m_min\":[");
  if (!off || !temp || !code || !hi || !lo) return false;

  int32_t offset = strtol(off, nullptr, 10);
  const char *date = strstr(http, "\nDate: ");
  uint32_t epoch = date ? parseHttpDate(date + 7) : 0;
  if (epoch) {
    world.epochAtSync = epoch;
    world.millisAtSync = now;
    world.tzOffset = offset;
    world.timeValid = true;
  }
  world.wxTemp = lroundf(strtof(temp, nullptr));
  world.wxCode = strtol(code, nullptr, 10);
  world.wxHi = lroundf(strtof(hi, nullptr));
  world.wxLo = lroundf(strtof(lo, nullptr));
  strncpy(world.wxPlace, saved.place, sizeof(world.wxPlace) - 1);
  world.wxAt = now;
  world.wxValid = true;

  if (!saved.offsetKnown || saved.offset != offset) {
    saved.offset = offset;
    saved.offsetKnown = 1;
    storeSaved();
  }
  return true;
}

static void fetchDone(bool ok, uint32_t now) {
  nextFetch = now + (ok ? FETCH_EVERY_MS : FETCH_RETRY_MS);
  nextCheck = now + CHECK_ONLINE_MS;
  state = S_IDLE;
}

// ---------- From Aminal ----------

// LO <lat> <lon> <place>
void wifiSetPlace(char *args, uint32_t now) {
  char *p = args;
  double lat = strtod(p, &p);
  double lon = strtod(p, &p);
  while (*p == ' ') p++;
  int32_t lat4 = lround(lat * 10000), lon4 = lround(lon * 10000);
  if (saved.magic == SAVED_MAGIC && saved.lat4 == lat4 && saved.lon4 == lon4 &&
      !strncmp(saved.place, p, sizeof(saved.place) - 1)) return;
  saved.lat4 = lat4;
  saved.lon4 = lon4;
  strncpy(saved.place, p, sizeof(saved.place) - 1);
  saved.place[sizeof(saved.place) - 1] = 0;
  storeSaved();
  nextFetch = now;
}

// WF <ssid>\t<password>. The module keeps them; the pet doesn't.
void wifiJoin(char *args) {
  char *tab = strchr(args, '\t');
  if (!tab) return;
  *tab = 0;
  strcpy(joinCmd, "AT+CWJAP_DEF=\"");
  appendEscaped(joinCmd, sizeof(joinCmd), args);
  strncat(joinCmd, "\",\"", sizeof(joinCmd) - strlen(joinCmd) - 1);
  appendEscaped(joinCmd, sizeof(joinCmd), tab + 1);
  strncat(joinCmd, "\"", sizeof(joinCmd) - strlen(joinCmd) - 1);
  joinPending = true;
  world.wifi = 1;
}

// ---------- Setup and the loop ----------

void wifiBegin() {
  SerialESP.begin(ESP_BAUD);
  loadSaved();
  // A pet waking with the PC off still knows its timezone
  if (saved.magic == SAVED_MAGIC && saved.offsetKnown) world.tzOffset = saved.offset;
  nextStart = millis() + 1500;               // let the module boot
}

void wifiRestart(uint32_t now) {
  state = S_START;
  nextStart = now + 1500;
  online = false;
}

void wifiTick(uint32_t now) {
  while (SerialESP.available()) espChar(SerialESP.read(), now);
  bool late = reached(now, deadline);

  switch (state) {
    case S_START:
      if (reached(now, nextStart)) { send("ATE0", 1500, now); state = S_ECHO; }
      break;
    case S_ECHO:
      if (got & R_OK) { modeDef = -1; send("AT+CWMODE_DEF?", 2000, now); state = S_MODEQ; }
      else if (late) { world.wifi = 0; nextStart = now + 15000; state = S_START; }   // no module
      break;
    case S_MODEQ:
      if (got & (R_OK | R_ERR) || late) {
        // Station only: the factory default also broadcasts an open network
        if (modeDef != 1) { send("AT+CWMODE_DEF=1", 3000, now); state = S_MODE; }
        else { send("AT+CIPMUX=0", 2000, now); state = S_MUX; }
      }
      break;
    case S_MODE:
      if (got & (R_OK | R_ERR) || late) { send("AT+CIPMUX=0", 2000, now); state = S_MUX; }
      break;
    case S_MUX:
      if (got & (R_OK | R_ERR) || late) askAddress(now);
      break;
    case S_IP:
      if (got & (R_OK | R_ERR) || late) {
        setOnline(got & R_OK, now);
        nextCheck = now + (online ? CHECK_ONLINE_MS : CHECK_OFFLINE_MS);
        state = S_IDLE;
      }
      break;
    case S_IDLE:
      if (!everOnline && world.wifi == 2 && now > 60000) {
        everOnline = true;                          // say it once
        showAlert("WiFi", "Not connected yet. On the PC, run: pet_bridge.py --wifi", 15, false, now);
      }
      if (joinPending) {
        joinPending = false;
        send(joinCmd, 20000, now);
        memset(joinCmd, 0, sizeof(joinCmd));        // the password is the module's now
        state = S_JOIN;
      } else if (online && saved.magic == SAVED_MAGIC && reached(now, nextFetch)) {
        startFetch(now);
      } else if (reached(now, nextCheck)) {
        askAddress(now);
      }
      break;
    case S_JOIN:
      if (got & R_OK) {
        askAddress(now);
      } else if (got & R_ERR || late) {
        world.wifi = 2;
        linkEvent("EV wifi fail");
        showAlert("WiFi", "Couldn't join. Check the name and password (2.4 GHz only).", 10, false, now);
        state = S_IDLE;
        nextCheck = now + CHECK_OFFLINE_MS;
      }
      break;
    case S_TCP:
      if (got & R_OK || (got & R_CONNECT && got & R_ERR)) {   // connected (or already was)
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)strlen(request));
        send(cmd, 5000, now);
        state = S_LEN;
      } else if (got & R_ERR || late) {
        fetchDone(false, now);
      }
      break;
    case S_LEN:
      if (got & R_PROMPT) {
        got = 0;
        httpLen = 0;
        SerialESP.print(request);
        deadline = now + 15000;
        state = S_RECV;
      } else if (got & R_ERR || late) {
        fetchDone(false, now);
      }
      break;
    case S_RECV:
      if (got & R_CLOSED) {
        fetchDone(parseReply(now), now);
      } else if (late) {
        send("AT+CIPCLOSE", 2000, now);
        fetchDone(parseReply(now), now);
      }
      break;
  }
}
