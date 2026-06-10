#include "BleBeaconLink.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <math.h>

#define LED_PIN  CFG_LED_PIN
#define LED_ON   CFG_LED_ON

// little-endian writers (same encodings as the native link)
static void putU16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void putI16(uint8_t* p, int v)       { putU16(p, (uint16_t)((int16_t)v)); }
static void putU32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static uint16_t clU16(float v) { if (v < 0) return 0; if (v > 65535.0f) return 65535; return (uint16_t)lroundf(v); }
static int16_t  clI16(float v) { if (v < -32768.0f) return -32768; if (v > 32767.0f) return 32767; return (int16_t)lroundf(v); }

static uint16_t validityBits(const MarineData& d) {
  uint16_t v = 0;
  if (d.vHeading) v |= (1 << 0);
  if (d.vCog)     v |= (1 << 1);
  if (d.vSog)     v |= (1 << 2);
  if (d.vXte)     v |= (1 << 3);
  if (d.vBrg)     v |= (1 << 4);
  if (d.vDtw)     v |= (1 << 5);
  if (d.vAwa)     v |= (1 << 6);
  if (d.vAws)     v |= (1 << 7);
  if (d.vTwa)     v |= (1 << 8);
  if (d.vTws)     v |= (1 << 9);
  if (d.vGust)    v |= (1 << 10);
  if (d.vDepth)   v |= (1 << 11);
  if (d.vTemp)    v |= (1 << 12);
  if (d.vBatt)    v |= (1 << 13);
  return v;
}

static void header(uint8_t* b, uint8_t page, const MarineData& d, int aisCount) {
  b[0] = BEACON_MAGIC;
  b[1] = page;
  putU16(&b[2], validityBits(d));
  b[4] = (d.anchorAlarm ? 1 : 0) | (d.shallowAlarm ? 2 : 0) | (d.aisAlarm ? 4 : 0);
  b[5] = (uint8_t)aisCount;
}

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
  int count = ais.count();
  int seqLen = 3 + count;            // NAV, WIND, ENV, then one page per target
  if (_step >= seqLen) { _step = 0; }

  uint8_t b[24];
  int len = 0;

  if (_step == 0) {                  // NAV
    header(b, BEACON_PAGE_NAV, d, count);
    putU16(&b[6],  clU16(d.headingTrue));
    putU16(&b[8],  clU16(d.cog));
    putU16(&b[10], clU16(d.sog * 100.0f));
    putU16(&b[12], clU16(d.bearing));
    putU16(&b[14], clU16(d.dtw * 100.0f));
    putI16(&b[16], clI16(d.xte * 1000.0f));
    len = 18;
  } else if (_step == 1) {           // WIND
    header(b, BEACON_PAGE_WIND, d, count);
    putI16(&b[6],  clI16(d.awa));
    putU16(&b[8],  clU16(d.aws * 100.0f));
    putI16(&b[10], clI16(d.twa));
    putU16(&b[12], clU16(d.tws * 100.0f));
    putU16(&b[14], clU16(d.gust * 100.0f));
    len = 16;
  } else if (_step == 2) {           // ENV
    header(b, BEACON_PAGE_ENV, d, count);
    putU16(&b[6],  clU16(d.depthUnderKeel * 100.0f));
    putI16(&b[8],  clI16(d.waterTemp * 100.0f));
    b[10] = (uint8_t)(d.battery < 0 ? 0 : (d.battery > 100 ? 100 : d.battery));
    len = 11;
  } else {                           // AIS target (_step - 3)
    int idx = _step - 3;
    uint32_t mmsi; float brg, dist, cog, sog;
    if (count > 0 && ais.relative(idx, d.lat, d.lon, mmsi, brg, dist, cog, sog)) {
      header(b, BEACON_PAGE_AIS, d, count);
      putU32(&b[6],  mmsi);
      putU16(&b[10], clU16(brg));
      putU16(&b[12], clU16(dist * 100.0f));
      putU16(&b[14], clU16(cog));
      len = 16;
    } else {                         // no own pos / no target → fall back to NAV
      header(b, BEACON_PAGE_NAV, d, count);
      putU16(&b[6],  clU16(d.headingTrue));
      putU16(&b[8],  clU16(d.cog));
      putU16(&b[10], clU16(d.sog * 100.0f));
      putU16(&b[12], clU16(d.bearing));
      putU16(&b[14], clU16(d.dtw * 100.0f));
      putI16(&b[16], clI16(d.xte * 1000.0f));
      len = 18;
    }
  }

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
