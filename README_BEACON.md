# Beacon edition — one app for every generic-BLE Garmin (2019+)

A **connectionless** variant: the ESP32 broadcasts the data in its BLE
**advertising payload** (manufacturer-specific data), and a single Connect IQ
app reads it by **scanning** — no pairing, no connection. Because it only needs
generic BLE scanning, **one binary installs on every Garmin from ~2019 on**
(fēnix 6/7/8, epix 2, Venu/Venu 2/3/4/Sq/X1, vívoactive 4/5/6, Forerunner
245…970, MARQ, Descent, D2, Edge…). quatix 6/7 and tactix install via the fēnix
product IDs (same hardware).

Trade-off vs the connection-based links: it's **one-way** (telemetry only — no
MOB back-channel), and many watches can read the same beacon at once without
blocking each other or the ESP32.

## How it works

```
   ESP32 ──(BLE advertising: manufacturer data, rotating pages)──▶  scanning watch(es)
   no connection · no pairing · any number of receivers
```

- **ESP32** (`CFG_LINK_MODE = CFG_LINK_BEACON` in `config.h`): `BleBeaconLink`
  rotates the dataset through advertising "pages" (NAV / WIND / ENV, then one
  AIS target per cycle) as manufacturer-specific data (company id `0xFFFF`),
  refreshed every `CFG_BEACON_MS` (250 ms). No GATT server.
- **Watch** (`garmin_marine_beacon/`): `BeaconDataSource` scans
  (`setScanState(SCANNING)`), reads `getManufacturerSpecificData(0xFFFF)` in
  `onScanResults`, and decodes the pages into the shared `DataModel`. Same 8-page
  responsive UI as the Venu 3 app (scales to each device's screen). AIS targets
  are plotted from the rotating AIS pages.

This is the BTHome / beacon pattern — same idea as DIY BLE thermometers that you
read without connecting.

## Build

```bash
cd garmin_marine_beacon
./build.sh fenix843mm     # or any installed listed device: venu445mm, fr965, edge540…
```

Validated to build for fēnix 8, Venu 4, Forerunner 570 and Edge 540 (one binary,
90 products in the manifest). Install: copy `bin/MarineBeacon.prg` to
`GARMIN/APPS/`.

On the ESP32 set `CFG_LINK_MODE = CFG_LINK_BEACON`, flash, and open the app —
the footer shows **LINK OK** as soon as the first advert is decoded. No pairing
step at all.

## Notes

- **Payload size:** a legacy advert holds ~24 usable bytes of manufacturer data,
  so the dataset is split across rotating pages (full refresh ~1–2 s). Heading
  and the active page update every 250 ms.
- **AIS:** every target is broadcast (one per cycle); the watch keeps them keyed
  by MMSI and prunes after 20 s.
- **One-way:** for MOB / commands use the connection-based Venu 3 variant
  (`README_VENU3.md`). A hybrid (beacon telemetry + a short connection only when
  sending a command) is a possible future addition.
- **Company id `0xFFFF`** is the "no company / development" id — fine for DIY.
