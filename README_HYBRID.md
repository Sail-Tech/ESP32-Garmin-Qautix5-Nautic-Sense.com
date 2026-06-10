# Hybrid edition — universal beacon + MOB back-channel

The best of both worlds. Telemetry travels **connectionless** in the BLE
advertising payload (exactly like the [beacon edition](README_BEACON.md), so it
runs on every generic-BLE Garmin from ~2019 on), but the link is **no longer
one-way**: when you send **MOB**, the watch opens a *short* BLE connection to the
ESP32, writes the command, and disconnects — then goes straight back to scanning.

```
   ESP32 ──(advertising: rotating telemetry pages)──▶  scanning watch        (always, no connection)
   ESP32 ◀──(brief connection: write MOB)─────────────  watch, on MOB only   (~1 s, then disconnect)
```

So you keep the beacon's advantages (no pairing, many receivers, sensor never
blocked) and regain the watch→boat command channel for man-overboard.

## How it works

- **ESP32** (`CFG_LINK_MODE = CFG_LINK_HYBRID`): `BleHybridLink` broadcasts the
  same rotating pages as the beacon (shared `BeaconFrame.h` builder) **and** runs
  a tiny connectable GATT server with only the command-write characteristic
  (`NAUTIC_CMD_UUID`). While a watch is connected, advertising pauses; it resumes
  on disconnect.
- **Watch** (`garmin_marine_hybrid/`): `HybridDataSource` scans and decodes the
  telemetry pages (no connection); `registerProfile` declares the command
  service so that, on **MOB** (touch-and-hold), it pairs to the next matching
  advert, writes `CMD_MOB`, and unpairs. Same responsive UI and screens as the
  beacon edition (see [README_BEACON.md](README_BEACON.md) for mockups).

## Build

```bash
cd garmin_marine_hybrid
./build.sh fenix843mm     # or any installed listed device (venu445mm, fr965, edge540…)
```

One binary, 90 products (fēnix/epix/Venu/Forerunner/Edge/MARQ/Descent/D2/Enduro;
quatix/tactix via the fēnix IDs). Validated to build for fēnix 8. Install: copy
`bin/MarineHybrid.prg` to `GARMIN/APPS/`.

On the ESP32 set `CFG_LINK_MODE = CFG_LINK_HYBRID`, flash, open the app — the
footer shows **LINK OK** from the first decoded advert (no pairing), and
touch-and-hold sends **MOB** (watch the serial monitor for the command).

## Trade-offs vs the other transports

| | Beacon | **Hybrid** | Native (Venu 3) |
|---|---|---|---|
| Reach | every 2019+ generic-BLE Garmin | **every 2019+ generic-BLE Garmin** | one paired watch |
| Telemetry | broadcast (no connection) | **broadcast (no connection)** | GATT notify (connection) |
| MOB back-channel | ✗ | **✓ (brief connection)** | ✓ (always connected) |
| Many receivers | ✓ | ✓ (except the ~1 s MOB) | ✗ |

## Notes

- During the brief MOB connection (~1 s), advertising pauses, so telemetry on
  all watches blips once — harmless (freshness timeout is 8 s).
- A watch with the hybrid app also works against a **pure beacon** ESP32: it
  reads telemetry fine; an MOB just silently fails (no command server to connect
  to).
