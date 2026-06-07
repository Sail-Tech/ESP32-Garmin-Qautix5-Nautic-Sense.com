# Venu 3 variant — native BLE link

This variant targets the Garmin **Venu 3 / Venu 3S** (and other Connect IQ
System 4/5 watches with **generic BLE**). Unlike the quatix 5 — which has no
generic BLE and forces the heart-rate-sensor hack — the Venu 3 can be a real
**BLE central**, so the watch connects directly to the ESP32 over a custom GATT
service. That removes every quatix-5 limitation:

| | quatix 5 (HR hack) | Venu 3 (native BLE) |
|---|---|---|
| Transport | impersonated HR sensor | custom GATT service |
| Throughput | ~1 byte / second | full ~31-byte frame, 2 Hz |
| Direction | one-way (read only) | **bidirectional** |
| Multiplex protocol | tag/value `SCHED[]` | not needed (whole frame at once) |
| MOB from watch | impossible | **supported** (touch-and-hold) |
| Display | 240×240 MIP, 5 buttons | 454×454 AMOLED, touch + buttons |

## What's in this branch

### ESP32 (`esp32_nauticsense_link/`)
A second BLE transport sits alongside the HR one, chosen at compile time:

```c
// config.h
#define CFG_LINK_MODE  CFG_LINK_NATIVE   // CFG_LINK_HR for the quatix 5
```

- **`BleNauticLink.h/.cpp`** — NimBLE peripheral exposing:
  - Service `4e415554-4943-5345-4e53-450000000001`
  - Data char `…0002` (NOTIFY) — pushes the whole `MarineData` as one binary frame.
  - AIS char `…0004` (NOTIFY) — streams every AIS target (one per frame, rotating).
  - Cmd char `…0003` (WRITE) — receives commands from the watch (MOB).
- The `.ino` sends a full frame every `CFG_NATIVE_TX_MS` (500 ms) and logs any
  command received. The data layer (NMEA 0183 / 2000 / WiFi, demo) is unchanged.

### Watch app (`garmin_marine_console_venu3/`)
- **`data/BleDataSource.mc`** — the BLE **central**: registers the profile,
  scans for `ESP32-NauticSense`, pairs, subscribes to the data characteristic,
  decodes the frame into the shared `DataModel`, and `sendMob()` writes the
  command characteristic. Bidirectional.
- **`MarineConsoleVenu3View.mc`** — the same 8 pages and OCEAN look, but the
  layout is **responsive**: every coordinate scales from the 240 px design
  baseline via `P()`, so it fills the 454 px round AMOLED (and adapts to other
  round devices).
- **`MarineConsoleVenu3Delegate.mc`** — touch + buttons: swipe to page, tap to
  dismiss an alarm, **touch-and-hold to send MOB**, action button to reconnect.
- `manifest.xml` declares the `BluetoothLowEnergy` permission and the Venu 3
  products.

## Build

```bash
cd garmin_marine_console_venu3
./build.sh venu445mm   # validation against an installed Venu device
./build.sh             # production (venu3) — once that device is installed
```

> Validated: builds successfully against an installed Venu device
> (`venu445mm`). To build for `venu3`/`venu3s`, install those devices in the
> Connect IQ SDK Manager.

Flash the ESP32 with `CFG_LINK_MODE = CFG_LINK_NATIVE`, pair **ESP32-NauticSense**
on the watch, then open the app — the footer shows **LINK OK** and the values
fill in. Touch-and-hold sends a MOB back to the ESP32 (watch it on the serial
monitor).

## Notes / next steps

- The data frame layout is documented in both `BleNauticLink.h` (ESP32) and the
  header comment of `BleDataSource.mc` (watch) — keep them in sync.
- The ESP32 requests a 64-byte MTU so the 31-byte frame fits one notification.
- The MOB command is logged on the ESP32; wire it to real MOB handling (mark a
  waypoint, raise an alarm) at the `CMD_MOB` hook in the `.ino`.
- The demo screen mockups in `screens/` are the 240 px quatix-5 layout; a
  454 px Venu 3 render isn't generated yet.
- Font sizes use the device-native Garmin fonts; on the larger screen they may
  warrant per-page tuning.
