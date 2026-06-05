#include <NimBLEDevice.h>

// ESP32 -> Garmin quatix 5 marine link (V5 link phase).
//
// The quatix 5 has no generic BLE, so the ESP32 advertises as a native BLE
// Heart Rate sensor (service 0x180D, char 0x2A37) that the watch pairs under
// Settings > Sensors & Accessories. The watch app reads the "heart rate"
// integer via Toybox.Sensor — that is the ONLY data the watch can get.
//
// To carry a whole marine dataset over that single 0..255 number we multiplex
// with a tag/value scheme, one byte per HOLD_MS:
//   TAG  byte  = 210 + fieldId   (announces which field comes next)
//   DATA byte  = 0..199          (the encoded value for that field)
// The watch gates DATA on the last TAG, so a missed byte only skips one field
// for one cycle (self-recovering). 210..222 sit just above plausible HR; if
// the watch filters them in testing, lower TAG_BASE / shrink the schedule.
//
// Values are demo data generated here (moved off the watch). Replace
// encodeField() with real sensor reads when wiring the NauticSense.

static NimBLEServer*         pServer = nullptr;
static NimBLECharacteristic* pHr     = nullptr;
static bool                  connected = false;

// -- Status LED (onboard GPIO2) ----------------------------------------------
// The board has only one software-controllable LED (blue, GPIO2). The red one
// is the power LED (hardware) and cannot be turned off from software.
// Blue: solid = connected; blinking = waiting.
#define LED_PIN  2
#define LED_ON   HIGH    // if the LED is active-low, switch to LOW

static void tickLed(bool conn) {
  static uint32_t last = 0;
  static bool ph = false;
  if (conn) { digitalWrite(LED_PIN, LED_ON); }
  else if (millis() - last >= 400) { last = millis(); ph = !ph; digitalWrite(LED_PIN, ph ? LED_ON : !LED_ON); }
}

// ---- protocol ----
static const uint8_t TAG_BASE = 210;
enum { F_HDG = 0, F_COG, F_SOG, F_XTE, F_AWA, F_AWS, F_TWA, F_TWS,
       F_GUST, F_DEPTH, F_TEMP, F_BATT, F_FLAGS };

// Heading is interleaved so it stays fresh (~every other slot); the secondary
// fields rotate. Trim this list to speed the secondaries up.
static const uint8_t SCHED[] = {
  F_HDG, F_COG,   F_SOG,
  F_HDG, F_XTE,   F_AWA,
  F_HDG, F_AWS,   F_TWA,
  F_HDG, F_TWS,   F_GUST,
  F_HDG, F_DEPTH, F_TEMP,
  F_HDG, F_BATT,  F_FLAGS
};
static const int      SCHED_N = sizeof(SCHED) / sizeof(SCHED[0]);
static const uint32_t HOLD_MS = 2000;   // how long each byte is held on-air

// ---- demo state ----
static uint32_t demoT      = 0;     // demo time in seconds (drives evolution)
static uint32_t lastDemoMs = 0;
static int      schedIdx   = 0;
static bool     phaseTag   = true;  // alternate TAG / DATA
static uint32_t lastByteMs = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  // NimBLE-Arduino 2.x signatures.
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override { connected = true; digitalWrite(LED_PIN, LED_ON); }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override {
    connected = false;
    NimBLEDevice::startAdvertising();
  }
};

static uint8_t clampB(int v) {
  if (v < 0)   { return 0; }
  if (v > 199) { return 199; }
  return (uint8_t)v;
}

// Encoded demo value for a field (mirrors the old watch SimDataSource).
// Decoders live in the watch LiveDataSource.mc — keep the two in sync.
static uint8_t encodeField(uint8_t id) {
  uint32_t t = demoT;
  int   hdg   = (236 + (int)(t * 2)) % 360;
  float depth = 17.0f + (t % 6) * 0.5f;
  switch (id) {
    case F_HDG:   return clampB(hdg / 2);                              // *2
    case F_COG:   return clampB(((hdg + 4) % 360) / 2);               // *2
    case F_SOG:   return clampB((int)((6.6f + (t % 8) * 0.2f) * 10)); // /10
    case F_XTE:   return clampB((int)(((t % 7) * 0.01f) * 100));      // /100
    case F_AWA:   return clampB((20 + (int)((t * 4) % 50)) / 2);      // *2
    case F_AWS:   return clampB((int)((13.0f + (t % 6) * 0.5f) * 10));// /10
    case F_TWA:   return clampB((30 + (int)((t * 3) % 60)) / 2);      // *2
    case F_TWS:   return clampB((int)((15.0f + (t % 5) * 0.7f) * 10));// /10
    case F_GUST:  return clampB((int)((15.0f + (t % 5) * 0.7f + 3.5f) * 10));
    case F_DEPTH: return clampB((int)(depth * 4));                    // /4
    case F_TEMP:  return clampB((int)((21.0f + (t % 4) * 0.1f) * 4)); // /4
    case F_BATT:  return clampB(87 - (int)(t % 10));                  // direct
    case F_FLAGS: {
      uint8_t f = 0;
      if ((t % 20) >= 16) { f |= 1; }   // anchor drag
      if (depth < 18.0f)  { f |= 2; }   // shallow water
      return f;
    }
  }
  return 0;
}

static void sendByte(uint8_t b) {
  uint8_t payload[2] = { 0x00, b };     // HR Measurement: flags=0, uint8 value
  pHr->setValue(payload, 2);
  if (connected) { pHr->notify(); }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);   // off at boot (will blink while waiting)
  NimBLEDevice::init("ESP32-NauticSense");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // TX power enum
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* hr = pServer->createService("180D");
  pHr = hr->createCharacteristic("2A37", NIMBLE_PROPERTY::NOTIFY);
  // 2.x: the CCCD (0x2902) is created automatically for NOTIFY characteristics.
  NimBLECharacteristic* bodyLoc = hr->createCharacteristic("2A38", NIMBLE_PROPERTY::READ);
  uint8_t loc = 1;
  bodyLoc->setValue(&loc, 1);
  hr->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  // Minimal, discoverable advertising: name + HR service fit in the 31-byte
  // payload and NimBLE adds the "general discoverable" flags.
  adv->setName("ESP32-NauticSense");
  adv->addServiceUUID("180D");
  adv->start();

  lastDemoMs = millis();
  lastByteMs = millis();
}

void loop() {
  uint32_t now = millis();
  tickLed(connected);                   // blue: solid = connected, blink = waiting

  if (now - lastDemoMs >= 1000) {       // advance demo values ~1 Hz
    lastDemoMs += 1000;
    demoT++;
  }

  if (now - lastByteMs >= HOLD_MS) {    // emit next protocol byte
    lastByteMs = now;
    uint8_t id = SCHED[schedIdx];
    if (phaseTag) {
      sendByte(TAG_BASE + id);
      phaseTag = false;
    } else {
      sendByte(encodeField(id));
      phaseTag = true;
      schedIdx = (schedIdx + 1) % SCHED_N;
    }
  }
}
