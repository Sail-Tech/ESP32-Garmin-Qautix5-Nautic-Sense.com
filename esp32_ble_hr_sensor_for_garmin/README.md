# ESP32 → Garmin quatix 5 — marine link (legacy single-file)

> Legacy reference. The maintained firmware is **`../esp32_nauticsense_link/`**
> (modular, with NMEA0183/NMEA2000/WiFi inputs). This single-file sketch uses the
> same wire protocol and is kept as a minimal reference.

The ESP32 advertises as a **native BLE heart-rate sensor** (service `0x180D`,
characteristic `0x2A37`). The quatix 5 has no generic BLE, so this is the only
way a Connect IQ app can receive data: the watch reads the "heart rate" via
`Toybox.Sensor` — **one integer 0..255 per second**.

## Multiplex protocol (tag/value)

To push the whole marine dataset through that single number, the firmware sends
one byte at a time (every `HOLD_MS`):

| Byte | Meaning |
|------|---------|
| `210 + id` | **TAG** — announces the field `id` that follows |
| `0..199`   | **DATA** — encoded value of the announced field |

The watch only accepts a DATA after a TAG, so a missed notification just skips
one field in a cycle (it self-recovers). Fields and encoding live in
`encodeField()` — they must match `LiveDataSource.mc` on the watch.

Fields: HDG, COG, SOG, XTE, AWA, AWS, TWA, TWS, GUST, DEPTH, TEMP, BATT, FLAGS.
**Heading** is interleaved in `SCHED[]` to stay fresh (~8 s); the secondary
fields rotate (~1–2 min). Shorten `SCHED[]` to speed up the secondary fields.

> ⚠️ The TAGs (210..222) sit just above a plausible HR. If the watch filters
> them (HR sanity-check), lower `TAG_BASE` and/or reduce the field count. If the
> watch **smooths** the HR, raise `HOLD_MS` (the watch has a debounce that counts
> on it). This is a prototype — tune against what actually arrives.

## Build / flash

PlatformIO: `pio run -t upload` (lib `h2zero/NimBLE-Arduino`). Or the Arduino IDE
with the ESP32 board and NimBLE-Arduino installed.

## Pairing on the watch

`MENU > Settings > Sensors & Accessories > Add New > Search All` → choose
**ESP32-NauticSense**. Then open the **Marine Console Q5 V5** app: when the
sensor is connected and emitting, the app shows **LINK OK**; otherwise **NO LINK**
and everything is `---`.

## Next step (real data)

Replace `encodeField()` with the real NauticSense values (RM3100 heading, etc.)
instead of the demo. The watch side does not change.
