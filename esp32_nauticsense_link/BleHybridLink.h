#pragma once
#include <stdint.h>
#include "MarineData.h"
#include "AisTarget.h"
#include "BeaconFrame.h"   // shared advertising-payload builder + page constants
#include "BleNauticLink.h" // reuse NAUTIC_* service/command UUIDs + NauticCmd

// Hybrid transport: connectionless beacon telemetry (the data is rotated through
// the advertising manufacturer-specific data, exactly like BleBeaconLink) PLUS a
// small connectable GATT server exposing only the command-write characteristic
// (NAUTIC_CMD_UUID). The watch reads telemetry by scanning, and only opens a
// short connection when it needs to send a command (MOB). When a central is
// connected, advertising pauses; it resumes automatically on disconnect.
class BleHybridLink {
public:
  void begin(const char* deviceName);
  void update(const MarineData& d, const AisTargets& ais);  // refresh the beacon page
  uint8_t takeCommand();                                     // poll MOB etc. from the watch
  bool connected() const;                                    // a central is connected (brief)
  void tickLed();

private:
  int _step = 0;
  void advertisePayload(const uint8_t* payload, int len);
};
