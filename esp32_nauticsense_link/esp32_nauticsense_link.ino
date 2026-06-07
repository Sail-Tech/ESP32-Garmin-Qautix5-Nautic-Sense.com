#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "MarineSource.h"
#include "LinkProtocol.h"
#include "BleHrLink.h"
#include "BleNauticLink.h"
#include "WifiManager.h"

// source = DEMO (simulado) or REAL (NMEA0183 / NMEA2000 / NMEA-over-WiFi),
// switchable live from the serial terminal. Defaults come from config.h.
static MarineSource source;
static LinkProtocol proto;
static WifiManager  wifi;

// Watch link transport — chosen at compile time in config.h (CFG_LINK_MODE).
#if CFG_LINK_MODE == CFG_LINK_NATIVE
static BleNauticLink nauticLink;          // custom BLE GATT service (Venu 3 etc.)
  #define LINK nauticLink
#else
static BleHrLink     bleLink;             // HR-sensor impersonation (quatix 5)
  #define LINK bleLink                    // (not 'link': clashes with POSIX link())
#endif

static const uint32_t HOLD_MS = CFG_HOLD_MS;
static uint32_t lastByteMs = 0;
static uint32_t lastSnapMs = 0;
static bool     g_showTx   = false;   // per-byte [tx] log (default off)
static bool     g_showSnap = false;   // periodic data snapshot (default off)

// ── Serial CLI ──────────────────────────────────────────────────────────────
// Interactive configuration menu (English). Type a number to toggle.
static void printMenu() {
  Serial.println();
  Serial.println(F("===== NauticSense Link — Configuration ====="));
#if CFG_LINK_MODE == CFG_LINK_NATIVE
  Serial.println(F("  Link transport ......... NATIVE BLE GATT (Venu 3 / generic-BLE watches)"));
#else
  Serial.println(F("  Link transport ......... HR sensor (quatix 5)"));
#endif
  Serial.printf ("  1) Data source ......... %s\n", source.isDemo() ? "DEMO (simulated)" : "REAL (NMEA)");
  Serial.printf ("  2) NMEA0183 ............ %s\n", source.n0183On()   ? "ON" : "off");
  Serial.printf ("  3) NMEA0183 raw echo ... %s\n", source.n0183Echo() ? "ON" : "off");
#ifdef USE_NMEA2000
  Serial.printf ("  4) NMEA2000 ............ %s\n", source.n2kOn() ? "ON" : "off");
#else
  Serial.println(F("  4) NMEA2000 ............ N/A (build with USE_NMEA2000)"));
#endif
  Serial.printf ("  5) [tx] byte log ....... %s\n", g_showTx   ? "ON" : "off");
  Serial.printf ("  6) Data snapshot (5s) .. %s\n", g_showSnap ? "ON" : "off");
  Serial.printf ("  7) NMEA over WiFi ...... %s\n", source.nmeaWifiOn() ? "ON" : "off");
  Serial.println(F("  Type 1-7 to toggle  ·  Enter or 'm' for this menu"));
  Serial.println(F("  WiFi: 'wifi scan' · 'wifi connect <ssid> <pass>' · 'wifi mode ap|sta|both' · 'wifi status'"));
  Serial.println(F("============================================"));
}

// wifi sub-commands: a = text after "wifi " (may be modified in place)
static void handleWifiCmd(char* a) {
  if      (!strcmp(a, "scan"))       { wifi.scan(Serial); }
  else if (!strcmp(a, "status"))     { wifi.printStatus(Serial); }
  else if (!strcmp(a, "disconnect")) { wifi.disconnect(Serial); }
  else if (!strcmp(a, "mode ap"))    { wifi.setMode(Serial, 0); }
  else if (!strcmp(a, "mode sta"))   { wifi.setMode(Serial, 1); }
  else if (!strcmp(a, "mode both"))  { wifi.setMode(Serial, 2); }
  else if (!strncmp(a, "connect ", 8)) {
    char* ssid = a + 8;
    char* sp = strchr(ssid, ' ');
    if (sp) { *sp = 0; wifi.connect(Serial, ssid, sp + 1); }
    else    { wifi.connect(Serial, ssid, ""); }
  } else {
    Serial.println(F("wifi: scan | connect <ssid> <pass> | disconnect | mode ap|sta|both | status"));
  }
}

static void cliExec(char* c) {
  if      (!strcmp(c, "1")) { source.setDemo(!source.isDemo());         printMenu(); }
  else if (!strcmp(c, "2")) { source.setN0183(!source.n0183On());       printMenu(); }
  else if (!strcmp(c, "3")) { source.setN0183Echo(!source.n0183Echo()); printMenu(); }
  else if (!strcmp(c, "4")) { source.setN2k(!source.n2kOn());           printMenu(); }
  else if (!strcmp(c, "5")) { g_showTx   = !g_showTx;                   printMenu(); }
  else if (!strcmp(c, "6")) { g_showSnap = !g_showSnap;                 printMenu(); }
  else if (!strcmp(c, "7")) { source.setNmeaWifi(!source.nmeaWifiOn()); printMenu(); }
  else if (!strcmp(c, "m") || !strcmp(c, "menu") || !strcmp(c, "help") || !strcmp(c, "?")) { printMenu(); }
  else if (!strncmp(c, "wifi ", 5)) { handleWifiCmd(c + 5); }
  // text aliases
  else if (!strcmp(c, "demo")) { source.setDemo(true);  printMenu(); }
  else if (!strcmp(c, "real")) { source.setDemo(false); printMenu(); }
  else if (!strcmp(c, "status")) { source.printStatus(Serial); wifi.printStatus(Serial); }
  else { Serial.printf("? '%s' — type 'm' for the menu\n", c); }
}
static char cliBuf[96];
static int  cliLen = 0;
static void cliService() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (cliLen > 0) { cliBuf[cliLen] = 0; cliExec(cliBuf); cliLen = 0; }
      else { printMenu(); }
    } else if (cliLen < (int)sizeof(cliBuf) - 1) {
      cliBuf[cliLen++] = ch;
    } else {
      cliLen = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  // Silence NimBLE / ESP-IDF internal debug+info logging (D/I) — keep W/E.
  // (Our own CLI/[tx]/snapshot use Serial.printf directly and are unaffected.)
  esp_log_level_set("*", ESP_LOG_WARN);
  Serial.println(F("ESP32-NauticSense link"));
  wifi.begin();           // WiFi (for NMEA-over-WiFi) — STA/AP per config + NVS
  source.begin();
  LINK.begin("ESP32-NauticSense");
  printMenu();
}

void loop() {
  uint32_t now = millis();

  cliService();           // terminal: demo/real, enable/disable interfaces
  wifi.service();         // WiFi auto-reconnect (STA)
  LINK.tickLed();         // LED: solid = connected, blink = waiting
  source.update(now);     // demo or NMEA -> MarineData

#if CFG_LINK_MODE == CFG_LINK_NATIVE
  // Native BLE: handle any command from the watch, then push a full frame.
  uint8_t cmd = nauticLink.takeCommand();
  if (cmd == CMD_MOB) {
    Serial.println(F("[cmd] MOB (man overboard) from watch"));
    // Hook: trigger MOB handling here (mark a waypoint, raise an alarm, etc.).
  }
  if (now - lastByteMs >= CFG_NATIVE_TX_MS) {
    lastByteMs = now;
    nauticLink.sendData(source.data());
    if (g_showTx) {
      Serial.printf("[tx] frame (%d B)   link=%s\n",
                    NAUTIC_FRAME_LEN, nauticLink.connected() ? "UP" : "down");
    }
  }
#else
  // HR link: one multiplexed byte per HOLD_MS.
  if (now - lastByteMs >= HOLD_MS) {
    lastByteMs = now;
    uint8_t b = proto.nextByte(source.data());
    bleLink.sendByte(b);
    if (g_showTx) {
      Serial.printf("[tx] %-5s %s  byte=%3u   link=%s\n",
                    LinkProtocol::fieldName(proto.lastField()),
                    proto.lastIsTag() ? "TAG " : "DATA",
                    b, bleLink.connected() ? "UP" : "down");
    }
  }
#endif

  if (g_showSnap && (now - lastSnapMs >= 5000)) {   // human-readable snapshot
    lastSnapMs = now;
    const MarineData& d = source.data();
    Serial.println(F("--------------------------------------------------"));
    Serial.printf("  src=%s  link=%s\n",
                  source.isDemo() ? "DEMO" : "REAL",
                  LINK.connected() ? "UP (watch reading)" : "down (advertising)");
    Serial.printf("  HDG %.0f  COG %.0f  SOG %.1f  DTW %.1f  XTE %.2f  BRG %.0f\n",
                  d.headingTrue, d.cog, d.sog, d.dtw, d.xte, d.bearing);
    Serial.printf("  AWA %.0f  AWS %.1f   TWA %.0f  TWS %.1f   GUST %.1f\n",
                  d.awa, d.aws, d.twa, d.tws, d.gust);
    Serial.printf("  DEPTH %.1f m  TEMP %.1f C  BATT %d%%  shallow=%d\n",
                  d.depthUnderKeel, d.waterTemp, d.battery, d.shallowAlarm);
  }
}
