#include "MarineSource.h"
#include "config.h"
#include <Arduino.h>

void MarineSource::begin() {
  _useDemo = CFG_BOOT_DEMO;
  _n0183On = CFG_BOOT_N0183;
  _n2kOn   = CFG_BOOT_N2K;
  _n2kBegun = false;

  _n0183.begin(&Serial2, CFG_N0183_BAUD, CFG_N0183_RX_PIN, CFG_N0183_TX_PIN, &_data);
#ifdef USE_NMEA2000
  if (_n2kOn) { _n2k.begin(&_data); _n2kBegun = true; }
#else
  _n2kOn = false;
#endif
  _nmeaWifi.begin(&_data);
  _nmeaWifiOn = CFG_BOOT_NMEAWIFI;
  _nmeaWifi.setEnabled(_nmeaWifiOn);

  // AIS: same parser fed by the UART and WiFi NMEA streams; uses own position.
  _aisParser.attach(&_ais, &_data);
  _n0183.attachAis(&_aisParser);
  _nmeaWifi.attachAis(&_aisParser);
}

// A handful of synthetic AIS targets around the demo own-position, so the AIS
// page shows real targets through the actual pipeline (not a hardcoded plot).
void MarineSource::seedDemoAis(uint32_t now) {
  // offsets in degrees from own pos (~1 NM ≈ 0.0167° lat); varied bearings.
  static const float dlat[5] = {  0.020f,  0.045f, -0.060f, -0.015f,  0.080f };
  static const float dlon[5] = {  0.025f, -0.050f,  0.010f, -0.090f,  0.040f };
  static const float cog [5] = {  210,     290,     10,      90,      180    };
  static const float sog [5] = {  8.2f,    4.1f,    12.0f,   0.0f,    6.5f   };
  for (int i = 0; i < 5; i++) {
    _ais.upsert(100 + i, _data.lat + dlat[i], _data.lon + dlon[i], cog[i], sog[i], now);
  }
}

// Fill the AIS summary fields (nearest target + guard alarm + count).
void MarineSource::updateAisSummary() {
  _data.aisCount = (uint8_t)_ais.count();
  float brg, dist;
  if (_data.vPos && _ais.nearest(_data.lat, _data.lon, brg, dist)) {
    _data.aisNearBrg = brg;
    _data.aisNearDist = dist;
    _data.vAisNear = true;
    _data.aisAlarm = (_guardNm > 0 && dist <= (float)_guardNm);
  } else {
    _data.vAisNear = false;
    _data.aisAlarm = false;
  }
}

void MarineSource::update(uint32_t nowMs) {
  if (_useDemo) {
    _demo.update(nowMs);
    _data = _demo.data();          // copy synthetic data into our model
    seedDemoAis(nowMs);            // keep demo targets fresh
    updateAisSummary();
    return;
  }

  // REAL: poll the enabled buses (they write _data directly via the pointer).
  if (_n0183On)    { _n0183.poll(); }
  if (_nmeaWifiOn) { _nmeaWifi.poll(); }
#ifdef USE_NMEA2000
  if (_n2kOn)      { _n2k.poll(); }
#endif

  uint32_t last = 0;
  if (_n0183On)    { uint32_t r = _n0183.lastRxMs();    if (r > last) { last = r; } }
  if (_nmeaWifiOn) { uint32_t r = _nmeaWifi.lastRxMs(); if (r > last) { last = r; } }
#ifdef USE_NMEA2000
  if (_n2kOn)      { uint32_t r = _n2k.lastRxMs();      if (r > last) { last = r; } }
#endif

  _ais.prune(nowMs, CFG_AIS_TIMEOUT_MS);   // age out silent targets

  // Whole (enabled) bus quiet → clear all (watch → "---" / NO LINK).
  if (last == 0 || (nowMs - last) > CFG_STALE_MS) {
    _data = MarineData();
  }
  updateAisSummary();
}

void MarineSource::setDemo(bool on) {
  _useDemo = on;
  _data = MarineData();            // clear on mode switch (stale values gone)
}

void MarineSource::setN0183(bool on) {
  _n0183On = on;                   // staleness clears its fields if it was the source
}

void MarineSource::setN2k(bool on) {
#ifdef USE_NMEA2000
  _n2kOn = on;
  if (on && !_n2kBegun) { _n2k.begin(&_data); _n2kBegun = true; }
#else
  (void)on;                        // N2K not compiled in
#endif
}

void MarineSource::setNmeaWifi(bool on) {
  _nmeaWifiOn = on;
  _nmeaWifi.setEnabled(on);
}

void MarineSource::printStatus(Stream& s) {
  s.println(F("── STATUS ─────────────────────────"));
  s.printf("  mode    : %s\n", _useDemo ? "DEMO (simulated)" : "REAL (NMEA)");
  s.printf("  NMEA0183: %s   (RX=GPIO%d @ %d baud, raw echo %s)\n",
           _n0183On ? "ON" : "off", CFG_N0183_RX_PIN, CFG_N0183_BAUD,
           _n0183.echo() ? "ON" : "off");
#ifdef USE_NMEA2000
  s.printf("  NMEA2000: %s   (CAN TX=GPIO%d RX=GPIO%d)\n",
           _n2kOn ? "ON" : "off", CFG_CAN_TX_PIN, CFG_CAN_RX_PIN);
#else
  s.println(F("  NMEA2000: N/A (build with USE_NMEA2000 + libs + transceiver)"));
#endif
  s.printf("  NMEA-WiFi: %s  (%s %s:%d, link %s)\n",
           _nmeaWifiOn ? "ON" : "off",
           CFG_NMEAWIFI_TCP ? "TCP" : "UDP",
           CFG_NMEAWIFI_HOST, CFG_NMEAWIFI_PORT,
           _nmeaWifi.connected() ? "up" : "down");
  if (_guardNm > 0) {
    s.printf("  AIS     : %d targets, guard %d NM%s\n",
             _ais.count(), _guardNm, _data.aisAlarm ? " [ALARM]" : "");
  } else {
    s.printf("  AIS     : %d targets, guard off\n", _ais.count());
  }
}
