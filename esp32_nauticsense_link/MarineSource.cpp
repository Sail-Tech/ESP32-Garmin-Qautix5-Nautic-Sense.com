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
}

void MarineSource::update(uint32_t nowMs) {
  if (_useDemo) {
    _demo.update(nowMs);
    _data = _demo.data();          // copy synthetic data into our model
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

  // Whole (enabled) bus quiet → clear all (watch → "---" / NO LINK).
  if (last == 0 || (nowMs - last) > CFG_STALE_MS) {
    _data = MarineData();
  }
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
}
