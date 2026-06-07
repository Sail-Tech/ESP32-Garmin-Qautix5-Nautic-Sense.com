#pragma once
#include <stdint.h>
#include "AisTarget.h"
#include "MarineData.h"

// Minimal AIS receiver: decodes !AIVDM / !AIVDO position reports (message
// types 1, 2, 3 and 18) into an AisTargets table. Static messages (type 5/24,
// names) are ignored — the plot shows no names. Multi-fragment sentences are
// reassembled (rarely needed for the position types, which are single-fragment).
//
// Own position (MarineData.lat/lon) is filled by the NMEA0183 RMC parser; the
// relative bearing/range of each target is computed later from it.
class AisParser {
public:
  void attach(AisTargets* targets, MarineData* ownData) { _targets = targets; _own = ownData; }

  // Feed one complete !AIVDM/!AIVDO line (no CR/LF). Validates the checksum.
  void handleSentence(const char* line, uint32_t now);

private:
  AisTargets* _targets = nullptr;
  MarineData* _own = nullptr;

  // multi-fragment reassembly (one message in progress)
  int  _expFrags = 0, _gotFrags = 0, _seq = -1;
  char _payload[256];
  int  _plen = 0;

  void decode(const char* payload, int plen, uint32_t now);
  static uint32_t getU(const char* p, int plen, int start, int len);
  static int32_t  getS(const char* p, int plen, int start, int len);
};
