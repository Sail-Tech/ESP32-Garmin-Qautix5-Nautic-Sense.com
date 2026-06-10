#include "BleHybridLink.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

static bool                  s_connected = false;
static volatile uint8_t      s_lastCmd   = CMD_NONE;

#define LED_PIN  CFG_LED_PIN
#define LED_ON   CFG_LED_ON

class HybridServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    s_connected = true;
    digitalWrite(LED_PIN, LED_ON);
    Serial.println(">>> watch connected (MOB) — advertising paused");
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    s_connected = false;
    Serial.printf(">>> watch disconnected (reason %d) — resuming beacon\n", reason);
    NimBLEDevice::startAdvertising();   // resume broadcasting
  }
};

class HybridCmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() >= 1) {
      s_lastCmd = v[0];
      Serial.printf(">>> command from watch: 0x%02X%s\n", s_lastCmd,
                    s_lastCmd == CMD_MOB ? " (MOB)" : "");
    }
  }
};

void BleHybridLink::begin(const char* deviceName) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Connectable server with ONLY the command characteristic (telemetry is in
  // the advertisement, not in a characteristic).
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new HybridServerCallbacks());
  NimBLEService* svc = server->createService(NAUTIC_SVC_UUID);
  NimBLECharacteristic* cmd =
      svc->createCharacteristic(NAUTIC_CMD_UUID,
                                NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmd->setCallbacks(new HybridCmdCallbacks());
  svc->start();

  // First advert so something is on air immediately.
  MarineData empty;
  update(empty, AisTargets());
}

void BleHybridLink::advertisePayload(const uint8_t* payload, int len) {
  if (s_connected) { return; }   // a central is connected → advertising is paused

  uint8_t mfg[2 + 24];
  mfg[0] = CFG_BEACON_COMPANY & 0xFF;
  mfg[1] = (CFG_BEACON_COMPANY >> 8) & 0xFF;
  if (len > 24) { len = 24; }
  memcpy(mfg + 2, payload, len);

  NimBLEAdvertisementData adv;
  adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  adv.setManufacturerData(mfg, 2 + len);

  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  a->setAdvertisementData(adv);
  a->start();
}

void BleHybridLink::update(const MarineData& d, const AisTargets& ais) {
  int seqLen = beaconSeqLen(ais);
  if (_step >= seqLen) { _step = 0; }
  uint8_t b[24];
  int len = beaconBuildPage(_step, d, ais, b);
  advertisePayload(b, len);
  _step++;
  if (_step >= seqLen) { _step = 0; }
}

uint8_t BleHybridLink::takeCommand() {
  uint8_t c = s_lastCmd;
  s_lastCmd = CMD_NONE;
  return c;
}

bool BleHybridLink::connected() const { return s_connected; }

void BleHybridLink::tickLed() {
  static uint32_t last = 0;
  static bool ph = false;
  if (s_connected) {
    digitalWrite(LED_PIN, LED_ON);     // solid during the brief MOB connection
  } else if (millis() - last >= 500) {
    last = millis();
    ph = !ph;
    digitalWrite(LED_PIN, ph ? LED_ON : !LED_ON);   // slow blink while broadcasting
  }
}
