#pragma once
#include <stdint.h>
#include "MarineData.h"
#include "Nmea0183.h"

class AisParser;   // forward

// Receives NMEA 0183 sentences over WiFi (TCP client or UDP listen, chosen by
// CFG_NMEAWIFI_TCP in config.h) and feeds them to an NMEA0183 parser → MarineData.
// Requires WiFi to be connected (managed by WifiManager). Typical source: a boat
// WiFi multiplexer/gateway that serves NMEA-over-IP (e.g. on port 10110).
class NmeaWifi {
public:
  void begin(MarineData* data);
  void attachAis(AisParser* a) { _parser.attachAis(a); }   // AIS over WiFi too
  void setEnabled(bool on);
  void poll();

  bool     enabled()  const { return _on; }
  bool     connected() const { return _connected; }
  uint32_t lastRxMs() const { return _parser.lastRxMs(); }
  void     setEcho(bool on) { _parser.setEcho(on); }
  bool     echo()     const { return _parser.echo(); }

private:
  Nmea0183 _parser;
  bool     _on = false;
  bool     _connected = false;
  uint32_t _lastTry = 0;
};
