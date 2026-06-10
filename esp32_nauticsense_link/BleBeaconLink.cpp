#include "BleBeaconLink.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <math.h>

#define LED_PIN  CFG_LED_PIN
#define LED_ON   CFG_LED_ON

void BleBeaconLink::begin(const char* deviceName) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  // First advertisement (empty NAV page) so something is on air immediately.
  MarineData empty;
  update(empty, AisTargets());
}

void BleBeaconLink::advertisePayload(const uint8_t* payload, int len) {
  // manufacturer data = 2-byte company id + payload
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
  a->start();   // idempotent: keeps broadcasting the new payload
}

void BleBeaconLink::update(const MarineData& d, const AisTargets& ais) {
  int seqLen = beaconSeqLen(ais);    // NAV, WIND, ENV, then one page per target
  if (_step >= seqLen) { _step = 0; }

  uint8_t b[24];
  int len = beaconBuildPage(_step, d, ais, b);
  advertisePayload(b, len);

  _step++;
  if (_step >= seqLen) { _step = 0; }
}

void BleBeaconLink::tickLed() {
  // Always broadcasting → slow blink.
  static uint32_t last = 0;
  static bool ph = false;
  if (millis() - last >= 500) {
    last = millis();
    ph = !ph;
    digitalWrite(LED_PIN, ph ? LED_ON : !LED_ON);
  }
}
