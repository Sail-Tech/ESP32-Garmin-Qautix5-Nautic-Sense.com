#pragma once
#include "DataSource.h"

// Synthetic marine data, generated on the ESP32 (moved off the watch). Values
// sweep/wander so every field on the watch visibly changes. Swap this for a
// real SensorSource later — nothing else in the firmware changes.
class DemoSource : public DataSource {
public:
  void update(uint32_t nowMs) override;
private:
  uint32_t _t = 0;        // demo time in seconds
  uint32_t _lastMs = 0;
};
