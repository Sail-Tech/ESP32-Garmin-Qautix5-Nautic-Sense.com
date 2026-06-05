#pragma once
#include <stdint.h>
#include "MarineData.h"

// NMEA 2000 listener (listen-only). Reads the boat's N2K bus and fills
// MarineData from the standard PGNs:
//   127250 Heading · 129026 COG/SOG · 130306 Wind · 128267 Depth ·
//   130312/130316 Water temp · 129283 XTE · 129284 Nav (brg/dist) · 127506 Battery%
//
// Requires the NMEA2000 + NMEA2000_esp32 libraries AND a CAN transceiver
// (e.g. SN65HVD230) on the link board — the plain ESP32 DevKit has none.
// Compiled in only when USE_NMEA2000 is defined (see esp32_nauticsense_link.ino);
// otherwise begin()/poll() are no-ops so the rest of the firmware still builds.
class Nmea2000Reader {
public:
  void begin(MarineData* data);
  void poll();
  uint32_t lastRxMs() const;
};
