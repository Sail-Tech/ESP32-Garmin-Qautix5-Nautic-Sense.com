#pragma once
#include "DataSource.h"
#include "DemoSource.h"
#include "Nmea0183.h"
#include "Nmea2000Reader.h"
#include "NmeaWifi.h"
#include "AisTarget.h"
#include "AisParser.h"
#include "config.h"

class Stream;

// Runtime-switchable data source.
//   DEMO mode → synthetic data (no boat).
//   REAL mode → boat data from NMEA0183 (UART) and/or NMEA2000 (CAN).
// Mode and per-interface enables boot from config.h and can be changed live
// via the serial CLI (demo / real / 0183 on|off / n2k on|off / status).
class MarineSource : public DataSource {
public:
  void begin() override;
  void update(uint32_t nowMs) override;

  void setDemo(bool on);
  void setN0183(bool on);
  void setN2k(bool on);
  void setNmeaWifi(bool on);
  void setN0183Echo(bool on) { _n0183.setEcho(on); _nmeaWifi.setEcho(on); }  // raw echo

  bool isDemo()     const { return _useDemo; }
  bool n0183On()    const { return _n0183On; }
  bool n2kOn()      const { return _n2kOn; }
  bool nmeaWifiOn() const { return _nmeaWifiOn; }
  bool n0183Echo()  const { return _n0183.echo(); }

  // AIS: tracked-target list (for the native BLE link) + guard-ring alarm.
  const AisTargets& ais() const { return _ais; }
  int  guardNm() const { return _guardNm; }
  void setGuardNm(int nm) { _guardNm = nm; }   // 0 = off, else 2/5/10 NM

  void printStatus(Stream& s);

private:
  DemoSource     _demo;
  Nmea0183       _n0183;
  Nmea2000Reader _n2k;
  NmeaWifi       _nmeaWifi;
  AisTargets     _ais;
  AisParser      _aisParser;
  bool _useDemo    = true;
  bool _n0183On    = true;
  bool _n2kOn      = false;
  bool _n2kBegun   = false;
  bool _nmeaWifiOn = false;
  int  _guardNm    = CFG_AIS_GUARD_NM;
  void seedDemoAis(uint32_t now);
  void updateAisSummary();
};
