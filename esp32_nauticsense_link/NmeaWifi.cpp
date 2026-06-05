#include "NmeaWifi.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

static WiFiClient s_tcp;
static WiFiUDP    s_udp;
static bool       s_udpBegun = false;
static uint8_t    s_buf[256];

void NmeaWifi::begin(MarineData* data) {
  _parser.attach(data);
}

void NmeaWifi::setEnabled(bool on) {
  _on = on;
  if (!on) {
    if (s_tcp.connected()) { s_tcp.stop(); }
    if (s_udpBegun)        { s_udp.stop(); s_udpBegun = false; }
    _connected = false;
  }
}

void NmeaWifi::poll() {
  if (!_on) { return; }
  if (WiFi.status() != WL_CONNECTED) { _connected = false; return; }

#if CFG_NMEAWIFI_TCP
  // ── TCP client: connect to the gateway and stream sentences ──
  if (!s_tcp.connected()) {
    _connected = false;
    uint32_t now = millis();
    if (now - _lastTry >= 2000) {
      _lastTry = now;
      if (s_tcp.connect(CFG_NMEAWIFI_HOST, CFG_NMEAWIFI_PORT)) {
        _connected = true;
        Serial.printf("[NMEA-WiFi] TCP connected to %s:%d\n",
                      CFG_NMEAWIFI_HOST, CFG_NMEAWIFI_PORT);
      }
    }
    return;
  }
  _connected = true;
  int n = s_tcp.available();
  while (n > 0) {
    int r = s_tcp.read(s_buf, n > (int)sizeof(s_buf) ? (int)sizeof(s_buf) : n);
    if (r <= 0) { break; }
    _parser.feed(s_buf, r);
    n -= r;
  }
#else
  // ── UDP listen: receive broadcast sentences ──
  if (!s_udpBegun) {
    s_udp.begin(CFG_NMEAWIFI_PORT);
    s_udpBegun = true;
    Serial.printf("[NMEA-WiFi] UDP listening on :%d\n", CFG_NMEAWIFI_PORT);
  }
  _connected = true;
  int pkt = s_udp.parsePacket();
  while (pkt > 0) {
    int r = s_udp.read(s_buf, sizeof(s_buf));
    if (r > 0) { _parser.feed(s_buf, r); }
    pkt = s_udp.parsePacket();
  }
#endif
}
