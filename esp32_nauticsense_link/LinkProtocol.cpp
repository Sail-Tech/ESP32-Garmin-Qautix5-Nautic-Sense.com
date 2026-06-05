#include "LinkProtocol.h"

// Heading every 3rd slot (stays fresh ~12 s); each secondary appears once per
// full cycle (~72 s at HOLD_MS=2000). Trim this list to speed secondaries up.
static const uint8_t SCHED[] = {
  F_HDG, F_COG,   F_SOG,
  F_HDG, F_XTE,   F_AWA,
  F_HDG, F_AWS,   F_TWA,
  F_HDG, F_TWS,   F_GUST,
  F_HDG, F_DEPTH, F_TEMP,
  F_HDG, F_BATT,  F_FLAGS
};
static const int SCHED_N = sizeof(SCHED) / sizeof(SCHED[0]);

uint8_t LinkProtocol::clampB(int v) {
  if (v < 0)   { return 0; }
  if (v > 199) { return 199; }
  return (uint8_t)v;
}

// Inverse of the watch decoders. Keep the scale factors in sync.
uint8_t LinkProtocol::encode(const MarineData& d, uint8_t field) {
  switch (field) {
    case F_HDG:   return clampB((int)d.headingTrue / 2);          // watch: *2
    case F_COG:   return clampB((int)d.cog / 2);                  // *2
    case F_SOG:   return clampB((int)(d.sog * 10));               // /10
    case F_XTE:   return clampB((int)(d.xte * 100));              // /100
    case F_AWA:   return clampB((int)d.awa / 2);                  // *2
    case F_AWS:   return clampB((int)(d.aws * 10));               // /10
    case F_TWA:   return clampB((int)d.twa / 2);                  // *2
    case F_TWS:   return clampB((int)(d.tws * 10));               // /10
    case F_GUST:  return clampB((int)(d.gust * 10));              // /10
    case F_DEPTH: return clampB((int)(d.depthUnderKeel * 4));     // /4
    case F_TEMP:  return clampB((int)(d.waterTemp * 4));          // /4
    case F_BATT:  return clampB(d.battery);                       // direct
    case F_FLAGS: {
      uint8_t f = 0;
      if (d.anchorAlarm)  { f |= 1; }
      if (d.shallowAlarm) { f |= 2; }
      return f;
    }
  }
  return 0;
}

// Is this field currently available to transmit?
bool LinkProtocol::fieldValid(const MarineData& d, uint8_t field) {
  switch (field) {
    case F_HDG:   return d.vHeading;
    case F_COG:   return d.vCog;
    case F_SOG:   return d.vSog;
    case F_XTE:   return d.vXte;
    case F_AWA:   return d.vAwa;
    case F_AWS:   return d.vAws;
    case F_TWA:   return d.vTwa;
    case F_TWS:   return d.vTws;
    case F_GUST:  return d.vGust;
    case F_DEPTH: return d.vDepth;
    case F_TEMP:  return d.vTemp;
    case F_BATT:  return d.vBatt;
    case F_FLAGS: return true;   // alarms always sent
  }
  return false;
}

uint8_t LinkProtocol::nextByte(const MarineData& d) {
  if (_tagPhase) {
    // Advance to the next VALID field; skip absent sensors so the watch keeps
    // showing "---" for them instead of a bogus 0.
    for (int i = 0; i < SCHED_N; i++) {
      uint8_t field = SCHED[_idx];
      if (fieldValid(d, field)) {
        _lastField = field;
        _lastIsTag = true;
        _tagPhase = false;
        return (uint8_t)(TAG_BASE + field);
      }
      _idx = (_idx + 1) % SCHED_N;
    }
    // Nothing valid yet (no boat data) → keepalive: a DATA byte with no pending
    // TAG on the watch is ignored, but BLE notifications keep flowing so the
    // sensor connection stays alive. The watch shows NO LINK until data arrives.
    _lastField = 0xFF;
    _lastIsTag = true;
    return 0;
  }
  uint8_t field = SCHED[_idx];
  _lastField = field;
  _lastIsTag = false;
  _tagPhase = true;
  _idx = (_idx + 1) % SCHED_N;
  return encode(d, field);
}

const char* LinkProtocol::fieldName(uint8_t id) {
  switch (id) {
    case F_HDG:   return "HDG";
    case F_COG:   return "COG";
    case F_SOG:   return "SOG";
    case F_XTE:   return "XTE";
    case F_AWA:   return "AWA";
    case F_AWS:   return "AWS";
    case F_TWA:   return "TWA";
    case F_TWS:   return "TWS";
    case F_GUST:  return "GUST";
    case F_DEPTH: return "DEPTH";
    case F_TEMP:  return "TEMP";
    case F_BATT:  return "BATT";
    case F_FLAGS: return "FLAGS";
  }
  return "?";
}
