#pragma once
#include <stdint.h>
#include <math.h>
#include "config.h"

// A tracked AIS target (last known geographic position + motion). Bearing and
// range relative to own ship are computed on demand from MarineData.lat/lon.
struct AisTarget {
  uint32_t mmsi = 0;
  float    lat = 0, lon = 0;   // deg
  float    cog = 0, sog = 0;   // deg true, knots
  uint32_t lastMs = 0;
  bool     used = false;
};

// Small fixed-size tracker. upsert() by MMSI; prune() ages out silent targets.
class AisTargets {
public:
  void clear() {
    for (int i = 0; i < CFG_AIS_MAX; i++) { _t[i].used = false; }
  }

  void upsert(uint32_t mmsi, float lat, float lon, float cog, float sog, uint32_t now) {
    int slot = -1, oldest = -1; uint32_t oldestMs = 0xFFFFFFFF;
    for (int i = 0; i < CFG_AIS_MAX; i++) {
      if (_t[i].used && _t[i].mmsi == mmsi) { slot = i; break; }
      if (!_t[i].used && slot < 0)          { slot = i; }
      if (_t[i].used && _t[i].lastMs < oldestMs) { oldestMs = _t[i].lastMs; oldest = i; }
    }
    if (slot < 0) { slot = (oldest >= 0) ? oldest : 0; }   // evict oldest if full
    _t[slot].used = true;
    _t[slot].mmsi = mmsi;
    _t[slot].lat = lat; _t[slot].lon = lon;
    _t[slot].cog = cog; _t[slot].sog = sog;
    _t[slot].lastMs = now;
  }

  void prune(uint32_t now, uint32_t timeoutMs) {
    for (int i = 0; i < CFG_AIS_MAX; i++) {
      if (_t[i].used && (now - _t[i].lastMs) > timeoutMs) { _t[i].used = false; }
    }
  }

  int count() const {
    int n = 0;
    for (int i = 0; i < CFG_AIS_MAX; i++) { if (_t[i].used) { n++; } }
    return n;
  }

  // Relative bearing(deg true)/distance(NM) of the i-th used target (0-based).
  bool relative(int i, float ownLat, float ownLon,
                uint32_t& mmsi, float& brg, float& dist, float& cog, float& sog) const {
    int k = -1;
    for (int s = 0; s < CFG_AIS_MAX; s++) {
      if (_t[s].used) {
        k++;
        if (k == i) {
          rangeBrg(ownLat, ownLon, _t[s].lat, _t[s].lon, brg, dist);
          mmsi = _t[s].mmsi; cog = _t[s].cog; sog = _t[s].sog;
          return true;
        }
      }
    }
    return false;
  }

  // Nearest target's bearing/distance; false if none.
  bool nearest(float ownLat, float ownLon, float& brg, float& dist) const {
    bool any = false; float best = 1e9f, bb = 0;
    for (int s = 0; s < CFG_AIS_MAX; s++) {
      if (!_t[s].used) { continue; }
      float b, dd; rangeBrg(ownLat, ownLon, _t[s].lat, _t[s].lon, b, dd);
      if (dd < best) { best = dd; bb = b; any = true; }
    }
    if (any) { brg = bb; dist = best; }
    return any;
  }

private:
  AisTarget _t[CFG_AIS_MAX];

  // Equirectangular approximation — accurate enough at AIS ranges (< ~40 NM).
  static void rangeBrg(float lat1, float lon1, float lat2, float lon2,
                       float& brgDeg, float& distNm) {
    const float toRad = (float)M_PI / 180.0f;
    float dLat = (lat2 - lat1) * toRad;
    float dLon = (lon2 - lon1) * toRad;
    float mLat = (lat1 + lat2) * 0.5f * toRad;
    float x = dLon * cosf(mLat);   // east
    float y = dLat;                // north
    distNm = sqrtf(x * x + y * y) * 3440.065f;   // earth radius in NM
    float b = atan2f(x, y) * 180.0f / (float)M_PI;
    if (b < 0) { b += 360.0f; }
    brgDeg = b;
  }
};
