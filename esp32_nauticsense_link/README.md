# ESP32 NauticSense Link

ESP32 bridge that reads the boat's marine data (**NMEA 0183** over UART and,
optionally, **NMEA 2000** over CAN, or **NMEA 0183 over WiFi**) and forwards it
to the Garmin **quatix 5** through the only channel that watch allows: it
impersonates a **native BLE heart-rate sensor** and multiplexes the data over
the "heart rate" value. It also has a **demo** source (no boat) for testing.

## Layout (Arduino IDE sketch — modular: demo now, real sensors later)

The folder is the sketch. The Arduino IDE compiles the `.ino` plus every
`.h`/`.cpp` in the root (they show up as tabs in the editor).

```
esp32_nauticsense_link.ino   # sketch: setup()/loop() + serial CLI
config.h                     # NMEA GPIOs/bauds, LED, boot defaults   <-- edit here
MarineData.h                 # marine dataset struct + per-field validity
DataSource.h                 # abstract data-source interface
MarineSource.h / .cpp        # SOURCE: switches DEMO <-> NMEA and enables 0183/2000/WiFi
DemoSource.h / .cpp          # synthetic demo generator (no boat)
Nmea0183.h / .cpp            # NMEA0183 parser (UART or fed bytes)
Nmea2000Reader.h / .cpp      # NMEA2000 parser (CAN; only with USE_NMEA2000)
NmeaWifi.h / .cpp            # NMEA0183 over IP (TCP client / UDP listen)
WifiManager.h / .cpp         # WiFi AP/STA/AP+STA, scan, connect (NVS), auto-reconnect
LinkProtocol.h / .cpp        # encodes the dataset into the tag/value (HR) protocol
BleHrLink.h / .cpp           # BLE transport (NimBLE, HR sensor 0x180D/0x2A37)
```

**Configuration:** everything (pins, bauds, boot defaults) lives in **`config.h`**.

**Data source:** boots according to `config.h` (`CFG_BOOT_DEMO`/`CFG_BOOT_N0183`/
`CFG_BOOT_N2K`/`CFG_BOOT_NMEAWIFI`) and is switched **at runtime from the
terminal** (see below). DEMO = simulated data; REAL = NMEA0183 (UART), NMEA2000
(CAN) and/or NMEA0183 over WiFi.

**Per-field validity:** a field is only transmitted (and shown on the watch)
while valid; if the sensor is absent from the bus it stays "---" instead of 0
(otherwise a `DEPTH 0.0` would falsely trip the shallow-water alarm).

## Terminal (Serial @115200) — interactive menu

Shown at **boot** and on **Enter** (empty line) or **`m`**. Type the **number**
to toggle each option:

```
===== NauticSense Link — Configuration =====
  1) Data source ......... DEMO / REAL
  2) NMEA0183 ............ ON / off
  3) NMEA0183 raw echo ... on / off     (raw sentences: [rx] $GP...)
  4) NMEA2000 ............ on / off      (if built with USE_NMEA2000)
  5) [tx] byte log ....... off          (per-BLE-byte log)
  6) Data snapshot (5s) .. off          (HDG/COG/DEPTH/... values)
  7) NMEA over WiFi ...... on / off      (NMEA0183 over TCP/UDP)
  8) AIS guard alarm ..... off/2/5/10 NM (proximity alert; press 8 to cycle)
```

By **default the terminal is quiet**: the recurring outputs (5 and 6) are
**off**. Text aliases: `demo`, `real`, `status`.

**WiFi commands:** `wifi scan` · `wifi connect <ssid> <pass>` · `wifi disconnect`
· `wifi mode ap|sta|both` · `wifi status`. Credentials are stored in NVS.

NimBLE/ESP-IDF internal `D`/`I` debug logging is silenced at boot
(`esp_log_level_set("*", ESP_LOG_WARN)`), so the monitor only shows the menu and
whatever you turn on.

## Protocol (must match the watch)

One byte every `HOLD_MS`:

| Byte | Meaning |
|------|---------|
| `210 + id` | **TAG** — announces the next field `id` |
| `0..199`   | **DATA** — encoded value of that field |

Fields and scales are in `LinkProtocol.cpp::encode()` — the **inverse** of the
decoders in `LiveDataSource.mc` (watch). Heading is interleaved in `SCHED[]` to
stay fresh (~8 s); the secondary fields rotate (~1–2 min). Shorten `SCHED[]` to
speed up the secondary fields.

> ⚠️ TAGs are 210..222 (just above a plausible HR). If the watch filters them,
> lower `TAG_BASE` / reduce the field count. If the watch **smooths** the HR,
> raise `HOLD_MS` (the watch has a debounce that counts on it). Prototype — tune
> against what actually arrives.

## Build / flash (Arduino IDE)

Prerequisites (one time):
1. **ESP32 support**: `Preferences > Additional Boards Manager URLs` →
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   → `Tools > Board > Boards Manager` → install **esp32** (Espressif).
2. **NimBLE library**: `Tools > Manage Libraries` → search **NimBLE-Arduino**
   (h2zero) → install **2.x** (latest). The code uses the 2.x API: callbacks
   with `NimBLEConnInfo&`, automatic CCCD (no `NimBLE2902`), and minimal
   discoverable advertising (`setName` + `addServiceUUID("180D")`).

Build/upload:
1. Open `esp32_nauticsense_link.ino` (the other files appear as tabs).
2. `Tools > Board` → **ESP32 Dev Module** (or your board); pick the `Port`.
3. **Upload** (→). If the upload stalls at high speed, lower `Tools > Upload
   Speed` to 115200.
4. `Tools > Serial Monitor` at **115200** → you see the configuration menu.

## NMEA inputs

### NMEA 0183 (UART) — default, no special hardware
- Wire the **boat's 0183 talker** to **RX = GPIO16** (`Serial2`). At true 0183
  levels (RS-422 differential) use a level-shifter; on the bench a TTL/3.3 V
  talker connects directly. Standard baud **4800** (38400 for AIS) — set
  `CFG_N0183_BAUD` in `config.h`.
- Sentences parsed → `MarineData`: `HDT`/`HDG` (heading), `VTG`/`RMC` (COG/SOG),
  `MWV` (apparent R and true T wind), `DBT`/`DPT` (depth), `MTW` (water temp),
  `XTE` (cross-track), `RMB` (bearing/distance to waypoint).

### NMEA 2000 (CAN) — optional, needs hardware + libs
1. Add a **CAN transceiver** (e.g. SN65HVD230): TX=GPIO5, RX=GPIO4 (set by
   `CFG_CAN_TX_PIN`/`CFG_CAN_RX_PIN` in `config.h`).
2. Install the **NMEA2000** and **NMEA2000_esp32** libs (ttlappalainen).
3. Uncomment `#define USE_NMEA2000` in `config.h`.
- PGNs parsed → `MarineData`: 127250 (heading), 129026 (COG/SOG), 130306 (wind),
  128267 (depth), 130312/130316 (water temp), 129283 (XTE), 129284
  (bearing/distance), 127506 (battery %).

### NMEA0183 over WiFi — optional
Receive **NMEA0183 sentences over IP** from a WiFi gateway/multiplexer.
1. Join the boat's WiFi: `wifi connect <ssid> <pass>` (STA mode; stored in NVS).
2. Configure the source in `config.h`: `CFG_NMEAWIFI_TCP` (1=TCP client, 0=UDP
   listen), `CFG_NMEAWIFI_HOST` and `CFG_NMEAWIFI_PORT` (common: 10110 or 2000).
3. Enable from the menu (option **7**) or via boot default `CFG_BOOT_NMEAWIFI`.
- In TCP the board connects as a **client** to `HOST:PORT`; in UDP it **listens**.
- Same sentences as 0183-UART (HDT/HDG/VTG/RMC/MWV/DBT/DPT/MTW/XTE/RMB).
- *(NMEA2000 "over WiFi" is not covered — it would need a gateway protocol such
  as SignalK; this is NMEA0183-over-IP, which is what WiFi multiplexers emit.)*

All buses (UART, CAN, WiFi) feed the **same** `MarineData`; whichever has data
wins. If they all go quiet for > `CFG_STALE_MS` (8 s), everything resets to "---".

### AIS targets
`!AIVDM` / `!AIVDO` sentences (on the UART or WiFi NMEA stream — an AIS receiver
usually runs at **38400** baud) are decoded by `AisParser` (position reports:
message types 1/2/3/18). Targets are placed relative to **own position** (from
`RMC`) and tracked in `AisTargets`.

- **Native BLE link (Venu 3):** every target is streamed (one per frame,
  rotating) over a dedicated AIS characteristic → the watch plots them all.
- **HR link (quatix 5):** only the **nearest** target's bearing/distance fit the
  1-byte channel, so the watch shows just the closest one.
- **Guard alarm:** menu option **8** cycles the proximity ring **off → 2 → 5 →
  10 NM**. When a target comes within the set range, the AIS alarm flag is sent
  to the watch (full-screen overlay + vibrate, like the shallow alarm).

In **demo** mode a handful of synthetic targets are generated around a fixed
own-position, so the AIS page shows live targets through the real pipeline.

## Pairing on the quatix 5

`MENU > Settings > Sensors & Accessories > Add New > Search All` → choose
**ESP32-NauticSense**. Then open the **Marine Console Q5 V5** app: with the
ESP32 emitting, the footer shows **LINK OK** and the values fill in (heading
first); without a link, **NO LINK** and everything is `---`.

> After pairing a new BLE sensor, the quatix 5 may need a **reboot** before it
> starts reading the new sensor.

## Libraries (Arduino IDE)

- **NimBLE-Arduino** 2.x (always — BLE transport).
- **NMEA2000** + **NMEA2000_esp32** (ttlappalainen) — only if you use
  `USE_NMEA2000`. NMEA0183 and WiFi need no external libs (WiFi/WiFiClient/
  WiFiUDP/Preferences ship with the ESP32 core).

## Notes

- The default build (NMEA0183 only) **does not need** the NMEA2000 libs —
  `Nmea2000Reader.cpp` compiles to stubs when `USE_NMEA2000` is not defined.
- To connect to a NauticSense 10 Pro: it already emits heading/attitude/wind/
  waves over N2K and 0183 — just wire the relevant bus to this board's RX.
