#include "WifiManager.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// ── NVS-persisted WiFi config ──
static const char*    WNS      = "linkwifi";
static const uint32_t WCFG_VER = 0x00020001;
struct WCfg { uint32_t ver; uint8_t mode; char ssid[33]; char pass[65]; };
static WCfg wc;

// ── Auto-reconnect state (set in the event handler, consumed in service()) ──
static volatile bool     s_recPending = false;
static volatile uint32_t s_recAtMs    = 0;
static volatile uint8_t  s_attempt    = 0;
static volatile uint32_t s_discCount  = 0;
static volatile uint8_t  s_lastReason = 0;

static uint32_t backoffMs(uint8_t a) {
  static const uint32_t sc[] = {500, 1000, 2000, 5000, 10000, 30000, 60000};
  if (a >= 7) { a = 6; }
  return sc[a];
}

static void wifiEvent(WiFiEvent_t ev, WiFiEventInfo_t info) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WIFI] GOT_IP %s  RSSI=%ddBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      WiFi.setSleep(false);
      s_attempt = 0;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_lastReason = info.wifi_sta_disconnected.reason;
      s_discCount++;
      if (wc.mode == 1) {   // STA-only auto-reconnects with backoff
        s_recPending = true;
        s_recAtMs = millis() + backoffMs(s_attempt);
        if (s_attempt < 250) { s_attempt++; }
      }
      break;
    default: break;
  }
}

static void wcDefaults() {
  memset(&wc, 0, sizeof(wc));
  wc.ver = WCFG_VER;
  wc.mode = CFG_WIFI_BOOT_MODE;
  strncpy(wc.ssid, CFG_WIFI_STA_SSID, sizeof(wc.ssid) - 1);
  strncpy(wc.pass, CFG_WIFI_STA_PASS, sizeof(wc.pass) - 1);
}
static void wcSave() {
  Preferences p; if (!p.begin(WNS, false)) { return; }
  wc.ver = WCFG_VER; p.putBytes("cfg", &wc, sizeof(wc)); p.end();
}
static void wcLoad() {
  Preferences p;
  if (!p.begin(WNS, true)) { wcDefaults(); return; }
  if (p.getBytesLength("cfg") == sizeof(WCfg)) { p.getBytes("cfg", &wc, sizeof(wc)); }
  else { wc.ver = 0; }
  p.end();
  if (wc.ver != WCFG_VER) { wcDefaults(); wcSave(); }
  wc.ssid[32] = 0; wc.pass[64] = 0;
}

static void applyMode() {
  WiFi.persistent(false);
  static bool evInstalled = false;
  if (!evInstalled) { WiFi.onEvent(wifiEvent); evInstalled = true; }

  if (wc.mode == 0) {                                   // AP
    WiFi.mode(WIFI_AP); WiFi.setSleep(false);
    WiFi.softAP(CFG_WIFI_AP_SSID, CFG_WIFI_AP_PASS);
    Serial.printf("[WIFI] AP \"%s\"  IP=%s\n", CFG_WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  } else if (wc.mode == 2) {                            // AP+STA
    WiFi.mode(WIFI_AP_STA); WiFi.setSleep(false);
    WiFi.softAP(CFG_WIFI_AP_SSID, CFG_WIFI_AP_PASS);
    if (wc.ssid[0]) { WiFi.begin(wc.ssid, wc.pass); }
    Serial.printf("[WIFI] AP+STA  (STA→\"%s\")\n", wc.ssid);
  } else {                                              // STA
    WiFi.mode(WIFI_STA); WiFi.setSleep(false);
    if (wc.ssid[0]) {
      WiFi.begin(wc.ssid, wc.pass);
      Serial.printf("[WIFI] STA → \"%s\" (background)\n", wc.ssid);
    } else {
      Serial.println(F("[WIFI] STA without SSID — use 'wifi connect <ssid> <pass>'"));
    }
  }
}

void WifiManager::begin()   { wcLoad(); applyMode(); }

void WifiManager::service() {
  if (!s_recPending) { return; }
  if (millis() < s_recAtMs) { return; }
  s_recPending = false;
  if (wc.mode == 1 && WiFi.status() != WL_CONNECTED && wc.ssid[0]) {
    WiFi.disconnect(false, false); delay(20);
    WiFi.begin(wc.ssid, wc.pass); WiFi.setSleep(false);
  }
}

void WifiManager::scan(Stream& s) {
  wifi_mode_t prev = WiFi.getMode();
  if (prev == WIFI_AP) { WiFi.mode(WIFI_AP_STA); }
  int n = WiFi.scanNetworks();
  s.printf("[WIFI] %d networks:\n", n < 0 ? 0 : n);
  for (int i = 0; i < n && i < 20; i++) {
    s.printf("  %2d: %-32s %4ddBm %s\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
             WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "enc");
  }
  WiFi.scanDelete();
  if (prev == WIFI_AP) { WiFi.mode(WIFI_AP); }
}

void WifiManager::connect(Stream& s, const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) { s.println(F("[WIFI] empty SSID")); return; }
  strncpy(wc.ssid, ssid, sizeof(wc.ssid) - 1); wc.ssid[sizeof(wc.ssid) - 1] = 0;
  strncpy(wc.pass, pass ? pass : "", sizeof(wc.pass) - 1); wc.pass[sizeof(wc.pass) - 1] = 0;
  wc.mode = 1; wcSave(); s_attempt = 0;
  s.printf("[WIFI] connecting to \"%s\" ...\n", ssid);
  applyMode();
}

void WifiManager::disconnect(Stream& s) {
  memset(wc.ssid, 0, sizeof(wc.ssid)); memset(wc.pass, 0, sizeof(wc.pass));
  wc.mode = 0; wcSave();
  s.println(F("[WIFI] STA forgotten → AP"));
  applyMode();
}

void WifiManager::setMode(Stream& s, uint8_t m) {
  if (m > 2) { s.println(F("[WIFI] invalid mode")); return; }
  wc.mode = m; wcSave();
  s.printf("[WIFI] mode = %s\n", m == 0 ? "AP" : m == 1 ? "STA" : "AP+STA");
  applyMode();
}

bool WifiManager::staConnected() const { return WiFi.status() == WL_CONNECTED; }

void WifiManager::printStatus(Stream& s) {
  const char* m = wc.mode == 0 ? "AP" : wc.mode == 1 ? "STA" : "AP+STA";
  s.printf("  WiFi    : mode=%s", m);
  if (wc.mode == 1 || wc.mode == 2) {
    if (WiFi.status() == WL_CONNECTED) {
      s.printf("  STA \"%s\" IP=%s RSSI=%ddBm", wc.ssid,
               WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      s.printf("  STA \"%s\" (disconnected)", wc.ssid);
    }
  }
  if (wc.mode == 0 || wc.mode == 2) {
    s.printf("  AP \"%s\" IP=%s", CFG_WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
  }
  s.println();
}
