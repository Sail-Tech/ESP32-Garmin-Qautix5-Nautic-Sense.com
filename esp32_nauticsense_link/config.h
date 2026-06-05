#pragma once
// ============================================================================
//  config.h — Central configuration for the ESP32-NauticSense link
//  GPIOs / bauds for the NMEA interfaces, status LED, behaviour and boot mode.
//  Anything marked as a "boot default" can be changed at runtime from the
//  serial terminal (Serial @115200): `demo | real | 0183 on|off | n2k on|off |
//  status`, plus the interactive menu (press Enter / 'm').
// ============================================================================

// -- NMEA 0183 (UART2) -------------------------------------------------------
// Wire the boat's 0183 talker to RX. Listen-only (TX is unused).
#define CFG_N0183_RX_PIN   16      // ESP32 RX  <- boat 0183 talker
#define CFG_N0183_TX_PIN   17      // ESP32 TX  (unused in listen-only)
#define CFG_N0183_BAUD     4800    // 4800 = standard 0183; 38400 = AIS

// -- NMEA 2000 (CAN) ---------------------------------------------------------
// To ENABLE the N2K parser, uncomment USE_NMEA2000. It requires:
//   * a CAN transceiver (e.g. SN65HVD230) wired to the pins below;
//   * the NMEA2000 + NMEA2000_esp32 libs (ttlappalainen) in the Arduino IDE.
// Without the define, N2K compiles to stubs (no libs needed) and stays disabled.
//#define USE_NMEA2000
#define CFG_CAN_TX_PIN     5       // ESP32 -> CTX of the transceiver
#define CFG_CAN_RX_PIN     4       // ESP32 <- CRX of the transceiver

// -- Status LED (onboard) ----------------------------------------------------
#define CFG_LED_PIN        2       // GPIO2 (onboard blue LED)
#define CFG_LED_ON         HIGH    // set to LOW if the LED is active-low

// -- Behaviour ---------------------------------------------------------------
#define CFG_SHALLOW_M      2.0f    // shallow-water alarm (metres under keel)
#define CFG_STALE_MS       8000    // clear everything if the bus(es) go quiet this long
#define CFG_HOLD_MS        2000    // dwell time of each HR-protocol byte

// -- WiFi (for receiving NMEA over WiFi) -------------------------------------
// Boot mode: 0=AP, 1=STA (join a network), 2=AP+STA.
// To receive NMEA from a gateway/multiplexer you normally use STA.
#define CFG_WIFI_BOOT_MODE 1                 // STA by default
#define CFG_WIFI_AP_SSID   "NauticSenseLink" // own AP SSID (AP mode)
#define CFG_WIFI_AP_PASS   "nautic1234"      // AP password (>=8 chars)
// STA credentials: set at runtime with `wifi connect <ssid> <pass>`
// (stored in NVS). Optionally provide defaults here (leave "" to use CLI only).
#define CFG_WIFI_STA_SSID  ""
#define CFG_WIFI_STA_PASS  ""

// -- NMEA0183 over WiFi (sentences over IP) ----------------------------------
#define CFG_NMEAWIFI_TCP   1                 // 1 = TCP client; 0 = UDP listen
#define CFG_NMEAWIFI_HOST  "192.168.4.1"     // gateway IP (TCP client mode)
#define CFG_NMEAWIFI_PORT  10110             // NMEA-over-IP port (common: 10110, 2000)

// -- Boot (defaults; changeable at runtime from the terminal) ----------------
#define CFG_BOOT_DEMO      true    // true = boot in DEMO (simulated); false = REAL (NMEA)
#define CFG_BOOT_N0183     true    // NMEA0183 (UART) enabled at boot
#define CFG_BOOT_N2K       false   // NMEA2000 enabled at boot (only if USE_NMEA2000)
#define CFG_BOOT_NMEAWIFI  false   // NMEA0183 over WiFi enabled at boot
