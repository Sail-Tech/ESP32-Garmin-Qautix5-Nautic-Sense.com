#pragma once
#include <stdint.h>
#include "MarineData.h"

// Tag/value multiplex over the single 0..255 integer the quatix 5 can read
// (the "heart rate"). One byte is emitted per call:
//   TAG  = TAG_BASE + fieldId   (announces the next field)
//   DATA = 0..199               (encoded value for that field)
// The watch gates each DATA on the last TAG, so a dropped byte just skips one
// field for one cycle. Field ids AND encodings MUST match the watch
// LiveDataSource.mc (applyField).
enum LinkField {
  F_HDG = 0, F_COG, F_SOG, F_XTE, F_AWA, F_AWS, F_TWA, F_TWS,
  F_GUST, F_DEPTH, F_TEMP, F_BATT, F_AISBRG, F_AISDIST, F_FLAGS, F_COUNT
};

class LinkProtocol {
public:
  static const uint8_t TAG_BASE = 210;

  // Returns the next protocol byte to transmit, advancing internal state.
  uint8_t nextByte(const MarineData& d);

  // What the last nextByte() produced — for debug logging.
  uint8_t lastField() const { return _lastField; }
  bool    lastIsTag() const { return _lastIsTag; }
  static const char* fieldName(uint8_t id);

private:
  int     _idx = 0;          // index into the schedule
  bool    _tagPhase = true;  // alternate TAG then DATA
  uint8_t _lastField = 0;
  bool    _lastIsTag = false;

  static uint8_t clampB(int v);
  static uint8_t encode(const MarineData& d, uint8_t field);
  static bool    fieldValid(const MarineData& d, uint8_t field);
};
