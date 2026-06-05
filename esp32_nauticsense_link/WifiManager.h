#pragma once
#include <stdint.h>
class Stream;

// WiFi manager (adapted from the NauticSense wifi_manager.h): AP / STA / AP+STA,
// scan, connect (creds saved to NVS), disconnect, mode switch, status, and an
// auto-reconnect event handler with backoff. Lets the link join a boat WiFi to
// receive NMEA over IP (see NmeaWifi).
class WifiManager {
public:
  void begin();                                       // load NVS + apply mode
  void service();                                     // call in loop (auto-reconnect)
  void scan(Stream& s);                               // scan + list networks
  void connect(Stream& s, const char* ssid, const char* pass);
  void disconnect(Stream& s);                         // forget STA → AP
  void setMode(Stream& s, uint8_t mode);              // 0=AP, 1=STA, 2=AP+STA
  void printStatus(Stream& s);
  bool staConnected() const;
};
