#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "MarineData.h"
#include "AisTarget.h"

// Shared advertising-payload builder for the beacon and hybrid links. Both
// rotate the dataset through the same "pages" so a single watch decoder works
// for either. Payload (company id added by the caller), little-endian; common
// header: 0 magic | 1 page | 2..3 validity u16 | 4 flags | 5 aisCount.
#define BEACON_MAGIC  0xB5
#define BEACON_PAGE_NAV  0
#define BEACON_PAGE_WIND 1
#define BEACON_PAGE_ENV  2
#define BEACON_PAGE_AIS  3

static inline void bf_putU16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static inline void bf_putI16(uint8_t* p, int v)       { bf_putU16(p, (uint16_t)((int16_t)v)); }
static inline void bf_putU32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static inline uint16_t bf_clU16(float v) { if (v < 0) return 0; if (v > 65535.0f) return 65535; return (uint16_t)lroundf(v); }
static inline int16_t  bf_clI16(float v) { if (v < -32768.0f) return -32768; if (v > 32767.0f) return 32767; return (int16_t)lroundf(v); }

static inline uint16_t bf_validity(const MarineData& d) {
  uint16_t v = 0;
  if (d.vHeading) v |= (1 << 0);  if (d.vCog)  v |= (1 << 1);  if (d.vSog)  v |= (1 << 2);
  if (d.vXte)     v |= (1 << 3);  if (d.vBrg)  v |= (1 << 4);  if (d.vDtw)  v |= (1 << 5);
  if (d.vAwa)     v |= (1 << 6);  if (d.vAws)  v |= (1 << 7);  if (d.vTwa)  v |= (1 << 8);
  if (d.vTws)     v |= (1 << 9);  if (d.vGust) v |= (1 << 10); if (d.vDepth) v |= (1 << 11);
  if (d.vTemp)    v |= (1 << 12); if (d.vBatt) v |= (1 << 13);
  return v;
}

static inline void bf_header(uint8_t* b, uint8_t page, const MarineData& d, int aisCount) {
  b[0] = BEACON_MAGIC;
  b[1] = page;
  bf_putU16(&b[2], bf_validity(d));
  b[4] = (d.anchorAlarm ? 1 : 0) | (d.shallowAlarm ? 2 : 0) | (d.aisAlarm ? 4 : 0);
  b[5] = (uint8_t)aisCount;
}

// Length of one rotation: NAV, WIND, ENV, then one page per AIS target.
static inline int beaconSeqLen(const AisTargets& ais) { return 3 + ais.count(); }

// Build the payload for `step` into b (>=24 bytes). Returns the byte length.
static inline int beaconBuildPage(int step, const MarineData& d, const AisTargets& ais, uint8_t* b) {
  int count = ais.count();
  if (step == BEACON_PAGE_NAV) {
    bf_header(b, BEACON_PAGE_NAV, d, count);
    bf_putU16(&b[6],  bf_clU16(d.headingTrue));
    bf_putU16(&b[8],  bf_clU16(d.cog));
    bf_putU16(&b[10], bf_clU16(d.sog * 100.0f));
    bf_putU16(&b[12], bf_clU16(d.bearing));
    bf_putU16(&b[14], bf_clU16(d.dtw * 100.0f));
    bf_putI16(&b[16], bf_clI16(d.xte * 1000.0f));
    return 18;
  } else if (step == BEACON_PAGE_WIND) {
    bf_header(b, BEACON_PAGE_WIND, d, count);
    bf_putI16(&b[6],  bf_clI16(d.awa));
    bf_putU16(&b[8],  bf_clU16(d.aws * 100.0f));
    bf_putI16(&b[10], bf_clI16(d.twa));
    bf_putU16(&b[12], bf_clU16(d.tws * 100.0f));
    bf_putU16(&b[14], bf_clU16(d.gust * 100.0f));
    return 16;
  } else if (step == BEACON_PAGE_ENV) {
    bf_header(b, BEACON_PAGE_ENV, d, count);
    bf_putU16(&b[6],  bf_clU16(d.depthUnderKeel * 100.0f));
    bf_putI16(&b[8],  bf_clI16(d.waterTemp * 100.0f));
    b[10] = (uint8_t)(d.battery < 0 ? 0 : (d.battery > 100 ? 100 : d.battery));
    return 11;
  }
  // AIS target (step - 3)
  int idx = step - 3;
  uint32_t mmsi; float brg, dist, cog, sog;
  if (count > 0 && ais.relative(idx, d.lat, d.lon, mmsi, brg, dist, cog, sog)) {
    bf_header(b, BEACON_PAGE_AIS, d, count);
    bf_putU32(&b[6],  mmsi);
    bf_putU16(&b[10], bf_clU16(brg));
    bf_putU16(&b[12], bf_clU16(dist * 100.0f));
    bf_putU16(&b[14], bf_clU16(cog));
    return 16;
  }
  // fall back to NAV when own position / target is unavailable
  return beaconBuildPage(BEACON_PAGE_NAV, d, ais, b);
}
