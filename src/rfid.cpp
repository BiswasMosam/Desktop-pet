// The RC522 card reader: a tap of the card goes to the bridge as
// "EV card <uid> tap", and the bridge decides what it does (and whether
// this card is the owner's).
//
// A tap is a card arriving after being away for a second and a half, so a
// card left lying on the reader counts once, however often the reader
// loses it for a moment. Gestures (double tap, hold) and a "hotel key
// slot" were both tried first and both glitched in real use: a resting
// card here drops out of reading for over a second now and then.
//
// The reader is asked every 60 ms whether a card is there. Waking the card
// each time (WUPA, then halting it again) is what makes a card that rests
// on the reader read as still there, rather than as new once and then gone.

#include "pet.h"
#include <SPI.h>
#include <MFRC522.h>

#define RFID_SS  PA4     // SPI1 is A5 SCK, A6 MISO, A7 MOSI
#define RFID_RST PB1

const uint32_t POLL_MS = 60;
const uint32_t AWAY_MS = 1500;     // gone this long before the next tap counts

// RST is driven here, not by the library: it only drives the pin when it
// happens to read low, and otherwise leaves it a floating input for good,
// which can let the chip drift into power-down and stop answering
static MFRC522 reader(RFID_SS, MFRC522::UNUSED_PIN);
static bool fitted = false;
static uint8_t version = 0;
static int rstWas = -1;           // what RST read before it was driven, for RF test

static bool near = false;         // a card is on the reader, or was just now
static uint32_t lastPoll = 0, seenAt = 0;
static char uid[24] = "";

// 0x91 / 0x92 are NXP's own chips, 0x88 a common clone; 0x00 or 0xFF is
// nothing answering on the wires
static bool answers(uint8_t v) { return v != 0x00 && v != 0xFF; }

// A clean hard reset, then held awake
static void wake() {
  pinMode(RFID_RST, INPUT);
  delay(1);
  rstWas = digitalRead(RFID_RST);
  pinMode(RFID_RST, OUTPUT);
  digitalWrite(RFID_RST, LOW);
  delay(5);
  digitalWrite(RFID_RST, HIGH);
  delay(50);
}

static void setUp() {
  wake();
  reader.PCD_Init();
  version = reader.PCD_ReadRegister(MFRC522::VersionReg);
  fitted = answers(version);
  if (!fitted) return;
  reader.PCD_SetAntennaGain(MFRC522::RxGain_max);   // fewer missed reads
  // Its timer gives up on a card after 25 ms by default, and asking with
  // no card there waits for all of it, which stalls the face. A card
  // answers in well under a millisecond, so 3 ms is plenty.
  reader.PCD_WriteRegister(MFRC522::TReloadRegH, 0x00);
  reader.PCD_WriteRegister(MFRC522::TReloadRegL, 0x78);
}

void rfidBegin() {
  SPI.begin();
  setUp();
}

// RF -> "RF <chip version in hex>" or "RF -": is it wired right?
const char *rfidStatus() {
  static char out[8];
  if (!fitted) setUp();              // wired since boot, or a loose wire fixed
  if (!fitted) return "-";
  snprintf(out, sizeof(out), "%02x", version);
  return out;
}

// RF test: is the SPI wiring sound? The version read four times (it
// should never change), a register written and read back twice, and
// whether the antenna is on (TxControlReg low bits set)
const char *rfidTest() {
  static char out[96];
  uint8_t v[4];
  for (uint8_t &x : v) x = reader.PCD_ReadRegister(MFRC522::VersionReg);
  reader.PCD_WriteRegister(MFRC522::TReloadRegL, 0x5A);
  uint8_t a = reader.PCD_ReadRegister(MFRC522::TReloadRegL);
  reader.PCD_WriteRegister(MFRC522::TReloadRegL, 0xA5);
  uint8_t b = reader.PCD_ReadRegister(MFRC522::TReloadRegL);
  reader.PCD_WriteRegister(MFRC522::TReloadRegL, 0x78);
  uint8_t tx = reader.PCD_ReadRegister(MFRC522::TxControlReg);

  // With the chip selected, does anything drive MISO? Undriven, the pin
  // follows whichever pull it is given
  digitalWrite(RFID_SS, LOW);
  pinMode(PA6, INPUT_PULLUP);
  delayMicroseconds(50);
  int up = digitalRead(PA6);
  pinMode(PA6, INPUT_PULLDOWN);
  delayMicroseconds(50);
  int down = digitalRead(PA6);
  digitalWrite(RFID_SS, HIGH);
  SPI.end();
  SPI.begin();                       // the pin back to SPI
  const char *miso = (up && !down) ? "float" : up ? "1" : "0";

  snprintf(out, sizeof(out), "v %02x %02x %02x %02x rw %02x/5a %02x/a5 tx %02x miso %s rst %d",
           v[0], v[1], v[2], v[3], a, b, tx, miso, rstWas);
  return out;
}

// RF poll: one "is a card there?", and the chip's own answer
const char *rfidPoll() {
  static char out[48];
  byte atqa[2] = {0, 0};
  byte len = sizeof(atqa);
  MFRC522::StatusCode s = reader.PICC_WakeupA(atqa, &len);
  bool serial = (s == MFRC522::STATUS_OK || s == MFRC522::STATUS_COLLISION) &&
                reader.PICC_ReadCardSerial();
  snprintf(out, sizeof(out), "wupa %d atqa %02x%02x uid %s", (int)s, atqa[0], atqa[1],
           serial ? "yes" : "no");
  if (serial) reader.PICC_HaltA();
  return out;
}

static bool readCard(char *out, size_t size) {
  byte atqa[2];
  byte len = sizeof(atqa);
  MFRC522::StatusCode s = reader.PICC_WakeupA(atqa, &len);
  if (s != MFRC522::STATUS_OK && s != MFRC522::STATUS_COLLISION) return false;
  if (!reader.PICC_ReadCardSerial()) return false;
  size_t n = 0;
  for (byte i = 0; i < reader.uid.size && n + 2 < size; i++) {
    n += snprintf(out + n, size - n, "%02X", reader.uid.uidByte[i]);
  }
  reader.PICC_HaltA();               // so the next WUPA finds it again
  return true;
}

void rfidTick(uint32_t now) {
  if (!fitted) return;
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    char card[24];
    // Two tries before calling it absent: single misses are common
    if (readCard(card, sizeof(card)) || readCard(card, sizeof(card))) {
      seenAt = now;
      if (!near) {
        near = true;
        strcpy(uid, card);
        char ev[48];
        snprintf(ev, sizeof(ev), "EV card %s tap", uid);
        linkEvent(ev);
        faceEmote(EM_SURPRISED, 600, now);
      }
    }
  }
  if (near && now - seenAt >= AWAY_MS) near = false;
}
