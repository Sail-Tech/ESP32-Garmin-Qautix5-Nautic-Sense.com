#include "BleNauticLink.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <math.h>

static NimBLECharacteristic* s_dataChar = nullptr;
static bool                  s_connected = false;
static volatile uint8_t      s_lastCmd   = CMD_NONE;

#define LED_PIN  CFG_LED_PIN     // see config.h
#define LED_ON   CFG_LED_ON

// -- little-endian frame writers ---------------------------------------------
static void putU16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void putI16(uint8_t* p, int v)       { putU16(p, (uint16_t)((int16_t)v)); }
static uint16_t clampU16(float v) {
  if (v < 0)       { return 0; }
  if (v > 65535.0) { return 65535; }
  return (uint16_t)lroundf(v);
}
static int16_t clampI16(float v) {
  if (v < -32768.0) { return -32768; }
  if (v >  32767.0) { return  32767; }
  return (int16_t)lroundf(v);
}

// -- callbacks ---------------------------------------------------------------
class NauticServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    s_connected = true;
    digitalWrite(LED_PIN, LED_ON);
    Serial.println(">>> BLE connected (native link) — blue LED solid ON");
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    s_connected = false;
    Serial.printf(">>> BLE disconnected (reason %d), re-advertising — LED blinking\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

class NauticCmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() >= 1) {
      s_lastCmd = v[0];
      Serial.printf(">>> command from watch: 0x%02X%s\n", s_lastCmd,
                    s_lastCmd == CMD_MOB ? " (MOB)" : "");
    }
  }
};

void BleNauticLink::begin(const char* deviceName) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);   // off at boot (will blink while waiting)

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setMTU(64);          // request a larger MTU so the 31-byte frame fits one notification

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new NauticServerCallbacks());

  NimBLEService* svc = server->createService(NAUTIC_SVC_UUID);
  s_dataChar = svc->createCharacteristic(NAUTIC_DATA_UUID, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* cmd =
      svc->createCharacteristic(NAUTIC_CMD_UUID,
                                NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmd->setCallbacks(new NauticCmdCallbacks());
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(deviceName);
  adv->addServiceUUID(NAUTIC_SVC_UUID);
  adv->start();
}

void BleNauticLink::sendData(const MarineData& d) {
  if (s_dataChar == nullptr) { return; }

  uint16_t valid = 0;
  if (d.vHeading) { valid |= (1 << VBIT_HEADING); }
  if (d.vCog)     { valid |= (1 << VBIT_COG); }
  if (d.vSog)     { valid |= (1 << VBIT_SOG); }
  if (d.vXte)     { valid |= (1 << VBIT_XTE); }
  if (d.vBrg)     { valid |= (1 << VBIT_BRG); }
  if (d.vDtw)     { valid |= (1 << VBIT_DTW); }
  if (d.vAwa)     { valid |= (1 << VBIT_AWA); }
  if (d.vAws)     { valid |= (1 << VBIT_AWS); }
  if (d.vTwa)     { valid |= (1 << VBIT_TWA); }
  if (d.vTws)     { valid |= (1 << VBIT_TWS); }
  if (d.vGust)    { valid |= (1 << VBIT_GUST); }
  if (d.vDepth)   { valid |= (1 << VBIT_DEPTH); }
  if (d.vTemp)    { valid |= (1 << VBIT_TEMP); }
  if (d.vBatt)    { valid |= (1 << VBIT_BATT); }

  uint8_t f[NAUTIC_FRAME_LEN];
  f[0] = NAUTIC_HEADER;
  putU16(&f[1],  valid);
  putU16(&f[3],  clampU16(d.headingTrue));
  putU16(&f[5],  clampU16(d.cog));
  putU16(&f[7],  clampU16(d.sog * 100.0f));
  putI16(&f[9],  clampI16(d.xte * 1000.0f));
  putU16(&f[11], clampU16(d.bearing));
  putU16(&f[13], clampU16(d.dtw * 100.0f));
  putI16(&f[15], clampI16(d.awa));
  putU16(&f[17], clampU16(d.aws * 100.0f));
  putI16(&f[19], clampI16(d.twa));
  putU16(&f[21], clampU16(d.tws * 100.0f));
  putU16(&f[23], clampU16(d.gust * 100.0f));
  putU16(&f[25], clampU16(d.depthUnderKeel * 100.0f));
  putI16(&f[27], clampI16(d.waterTemp * 100.0f));
  f[29] = (uint8_t)(d.battery < 0 ? 0 : (d.battery > 100 ? 100 : d.battery));
  f[30] = (d.anchorAlarm ? 0x01 : 0) | (d.shallowAlarm ? 0x02 : 0);

  s_dataChar->setValue(f, NAUTIC_FRAME_LEN);
  if (s_connected) { s_dataChar->notify(); }
}

bool BleNauticLink::connected() const { return s_connected; }

uint8_t BleNauticLink::takeCommand() {
  uint8_t c = s_lastCmd;
  s_lastCmd = CMD_NONE;
  return c;
}

void BleNauticLink::tickLed() {
  static uint32_t last = 0;
  static bool ph = false;
  if (s_connected) {
    digitalWrite(LED_PIN, LED_ON);
  } else if (millis() - last >= 400) {
    last = millis();
    ph = !ph;
    digitalWrite(LED_PIN, ph ? LED_ON : !LED_ON);
  }
}
