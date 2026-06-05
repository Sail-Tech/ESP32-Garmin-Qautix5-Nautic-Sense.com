#pragma once
#include <stdint.h>
#include "MarineData.h"

// Abstract source of marine data. DemoSource synthesises it; a future
// SensorSource will read the real NauticSense (RM3100 heading, etc.). The rest
// of the firmware depends only on this interface, so swapping demo -> real is
// a one-line change in main.cpp.
class DataSource {
public:
  virtual ~DataSource() {}
  virtual void begin() {}
  virtual void update(uint32_t nowMs) = 0;   // refresh the data in place
  const MarineData& data() const { return _data; }
protected:
  MarineData _data;
};
