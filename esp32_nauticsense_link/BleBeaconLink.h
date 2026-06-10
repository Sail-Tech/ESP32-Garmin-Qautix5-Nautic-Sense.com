#pragma once
#include <stdint.h>
#include "MarineData.h"
#include "AisTarget.h"
#include "BeaconFrame.h"   // shared advertising-payload builder + page constants

// Connectionless beacon transport. The dataset is rotated through the BLE
// advertising payload as manufacturer-specific data (company id
// CFG_BEACON_COMPANY). No GATT server, no connection — any number of watches
// read it by scanning. One-way only (no commands back).
//
// Manufacturer-data payload (company id stripped by the receiver), little-endian.
// Common header (every page):
//   0  magic 0xB5
//   1  page id
//   2..3 validity bitmask u16 (bit per field, NauticVBit order)
//   4  flags (bit0 anchor, bit1 shallow, bit2 ais)
//   5  AIS target count
// Page 0 (NAV):  6 hdg u16 | 8 cog u16 | 10 sog u16*100 | 12 brg u16 | 14 dtw u16*100 | 16 xte i16*1000
// Page 1 (WIND): 6 awa i16 | 8 aws u16*100 | 10 twa i16 | 12 tws u16*100 | 14 gust u16*100
// Page 2 (ENV):  6 depth u16*100 | 8 temp i16*100 | 10 batt u8
// Page 3 (AIS):  6 mmsi u32 | 10 brg u16 | 12 dist u16*100 | 14 cog u16   (one target, rotating)
// (magic/page constants and the builder live in BeaconFrame.h)

class BleBeaconLink {
public:
  void begin(const char* deviceName);
  // Build the next page from the data and update the advertisement. Call every
  // CFG_BEACON_MS; it rotates NAV/WIND/ENV and one AIS target per cycle.
  void update(const MarineData& d, const AisTargets& ais);
  bool connected() const { return false; }   // beacon is connectionless
  void tickLed();

private:
  int _step = 0;
  void advertisePayload(const uint8_t* payload, int len);
};
