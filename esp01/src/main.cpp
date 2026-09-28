// The desktop pet's WiFi: an ESP-01 wired to the Black Pill's USART1.
//
// It speaks the pet's own protocol, one line per message at 115200 baud:
//
//   to the pet     NW joining | NW setup <ap> | NW ok <ip>
//                  TM <unix> <utc offset>       from NTP
//                  WX <temp> <wmo> <hi> <lo> <place>   from Open-Meteo
//   from the pet   LO <lat> <lon> <place>       where the weather is for
//
// and relays Aminal: a TCP client on port 7676 (desktop-pet.local) is
// passed through to the pet line by line, and the pet's answers go back.
//
// WiFi is set up once from a phone: with no network saved, it opens an
// access point called "Desktop-pet" with a page to pick one (WiFiManager).

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>

static const char *AP_NAME = "Desktop-pet";
static const char *HOST_NAME = "desktop-pet";
static const uint16_t RELAY_PORT = 7676;
static const uint32_t PLACE_MAGIC = 0x5045544C;     // "PETL"

static const uint32_t TIME_EVERY_MS    = 10UL * 60 * 1000;
static const uint32_t WEATHER_EVERY_MS = 20UL * 60 * 1000;
static const uint32_t WEATHER_RETRY_MS = 5UL * 60 * 1000;

// Where the weather is for, kept in flash across power cuts
struct Place {
  uint32_t magic;
  float lat, lon;
  int32_t offset;          // seconds east of UTC, learned from Open-Meteo
  bool offsetKnown;
  char name[20];
};
static Place place;

static WiFiManager wm;
static WiFiServer relay(RELAY_PORT);
static WiFiClient aminal;
static bool online = false;
static uint32_t timeDue = 0, weatherDue = 0;
static String fromPet, fromAminal;

static void say(const String &line) {
  Serial.print(line);
  Serial.print('\n');
}

static void savePlace() {
  EEPROM.put(0, place);
  EEPROM.commit();
}

// ---------- Time and weather ----------

static void sendTime() {
  time_t now = time(nullptr);
  if (now < 1600000000 || !place.offsetKnown) return;    // not synced yet
  say("TM " + String((uint32_t)now) + " " + String(place.offset));
}

static bool fetchWeather() {
  if (place.magic != PLACE_MAGIC) return false;
  std::unique_ptr<BearSSL::WiFiClientSecure> tls(new BearSSL::WiFiClientSecure);
  tls->setInsecure();                 // no certificate store on an ESP-01
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(place.lat, 4) +
               "&longitude=" + String(place.lon, 4) +
               "&current=temperature_2m,weather_code"
               "&daily=temperature_2m_max,temperature_2m_min"
               "&forecast_days=1&timezone=auto";
  if (!http.begin(*tls, url)) return false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  JsonDocument doc;
  DeserializationError bad = deserializeJson(doc, http.getString());
  http.end();
  if (bad || doc["current"].isNull()) return false;

  int32_t offset = doc["utc_offset_seconds"] | 0;
  if (!place.offsetKnown || offset != place.offset) {
    place.offset = offset;
    place.offsetKnown = true;
    savePlace();
    sendTime();                        // the clock was waiting for this
  }
  say("WX " + String((int)lround(doc["current"]["temperature_2m"].as<float>())) + " " +
      String(doc["current"]["weather_code"].as<int>()) + " " +
      String((int)lround(doc["daily"]["temperature_2m_max"][0].as<float>())) + " " +
      String((int)lround(doc["daily"]["temperature_2m_min"][0].as<float>())) + " " +
      String(place.name));
  return true;
}

// ---------- Lines from the pet ----------

// LO <lat> <lon> <place>, sent by Aminal through the pet
static void setPlace(const String &args) {
  int a = args.indexOf(' ');
  int b = args.indexOf(' ', a + 1);
  if (a < 0 || b < 0) return;
  float lat = args.substring(0, a).toFloat();
  float lon = args.substring(a + 1, b).toFloat();
  String name = args.substring(b + 1);
  name.trim();
  bool same = place.magic == PLACE_MAGIC && fabsf(place.lat - lat) < 0.001f &&
              fabsf(place.lon - lon) < 0.001f && name == place.name;
  if (same) return;
  place.magic = PLACE_MAGIC;
  place.lat = lat;
  place.lon = lon;
  strncpy(place.name, name.c_str(), sizeof(place.name) - 1);
  place.name[sizeof(place.name) - 1] = 0;
  savePlace();
  weatherDue = millis();               // fetch for the new place now
}

static void fromPetLine(const String &line) {
  if (line.startsWith("LO ")) {
    setPlace(line.substring(3));
  } else if (aminal && aminal.connected()) {
    // Everything else is the pet answering Aminal
    aminal.print(line);
    aminal.print('\n');
  }
}

static void readPet() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (fromPet.length()) fromPetLine(fromPet);
      fromPet = "";
    } else if (fromPet.length() < 2200) {  // SN answers are 2051 bytes
      fromPet += c;
    }
  }
}

// ---------- Aminal over the network ----------

static void serveRelay() {
  if (relay.hasClient()) {
    WiFiClient next = relay.accept();
    if (aminal && aminal.connected()) aminal.stop();      // newest wins
    aminal = next;
    aminal.setNoDelay(true);
    fromAminal = "";
  }
  while (aminal && aminal.connected() && aminal.available()) {
    char c = aminal.read();
    if (c == '\r') continue;
    if (c == '\n') {
      // Whole lines only, so they never interleave with our own. Where the
      // weather is for is ours too: the pet never sends WiFi's lines back.
      if (fromAminal.startsWith("LO ")) setPlace(fromAminal.substring(3));
      if (fromAminal.length()) say(fromAminal);
      fromAminal = "";
    } else if (fromAminal.length() < 200) {
      fromAminal += c;
    }
  }
}

// ---------- WiFi ----------

static void goOnline() {
  online = true;
  say("NW ok " + WiFi.localIP().toString());
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  MDNS.begin(HOST_NAME);
  MDNS.addService("pet", "tcp", RELAY_PORT);
  relay.begin();
  relay.setNoDelay(true);
  timeDue = millis() + 3000;           // give NTP a moment
  weatherDue = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.setRxBufferSize(2048);
  Serial.setDebugOutput(false);

  EEPROM.begin(sizeof(Place));
  EEPROM.get(0, place);
  if (place.magic != PLACE_MAGIC) memset(&place, 0, sizeof(place));

  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOST_NAME);
  wm.setDebugOutput(false);
  wm.setConfigPortalBlocking(false);
  wm.setTitle("Desktop pet");
  wm.setAPCallback([](WiFiManager *) { say(String("NW setup ") + AP_NAME); });
  say("NW joining");
  if (wm.autoConnect(AP_NAME)) goOnline();
}

void loop() {
  wm.process();
  readPet();

  if (WiFi.status() == WL_CONNECTED) {
    if (!online) goOnline();
  } else if (online) {
    online = false;
    say("NW joining");
  }
  if (!online) return;

  MDNS.update();
  serveRelay();

  uint32_t now = millis();
  if ((int32_t)(now - timeDue) >= 0) {
    sendTime();
    timeDue = now + (time(nullptr) > 1600000000 ? TIME_EVERY_MS : 3000);
  }
  if ((int32_t)(now - weatherDue) >= 0) {
    weatherDue = now + (fetchWeather() ? WEATHER_EVERY_MS : WEATHER_RETRY_MS);
  }
}
