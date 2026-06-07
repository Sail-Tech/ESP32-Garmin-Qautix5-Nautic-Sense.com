#pragma once
#include <Arduino.h>
#include "MarineData.h"

class AisParser;   // forward (AIS sentences are forwarded to it)

// NMEA 0183 listener: reads sentences from a UART and fills MarineData.
// Parses HDT/HDG (heading), VTG/RMC (COG/SOG), MWV (apparent+true wind),
// DBT/DPT (depth), MTW (water temp), XTE (cross-track), RMB (bearing/dist).
//
// Wiring: connect the boat's 0183 talker output to the chosen RX pin. At
// 0183 levels (RS-422 differential) use a level shifter; on the bench a
// TTL/3.3 V talker can drive RX directly. Standard baud = 4800 (38400 for AIS).
class Nmea0183 {
public:
  void begin(HardwareSerial* port, uint32_t baud, int rxPin, int txPin, MarineData* data);
  void attach(MarineData* data) { _d = data; } // use without a UART (feed externally)
  void attachAis(AisParser* a) { _ais = a; }    // route !AIVDM/!AIVDO sentences here
  void feed(const uint8_t* data, int len);      // feed bytes from any source (e.g. WiFi)
  void poll();                                  // call often (drains UART, parses)
  uint32_t lastRxMs() const   { return _lastRx; }
  uint32_t sentenceCount() const { return _good; }
  uint32_t badCount() const   { return _bad; }

  void setEcho(bool on) { _echo = on; }         // echo raw sentences to Serial
  bool echo() const     { return _echo; }

private:
  HardwareSerial* _ser = nullptr;
  MarineData*     _d   = nullptr;
  AisParser*      _ais = nullptr;
  char     _buf[110];
  int      _len = 0;
  uint32_t _lastRx = 0, _good = 0, _bad = 0;
  bool     _echo = false;
  void feedChar(char c);
  void handleLine(char* line);
};
