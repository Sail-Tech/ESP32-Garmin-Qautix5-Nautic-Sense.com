#pragma once
#include <stdint.h>
#include "MarineData.h"

// Native BLE transport for watches that DO support generic BLE (Venu 3, other
// Connect IQ System 4/5 devices). The ESP32 is a BLE peripheral exposing a
// custom GATT service:
//
//   Service  4e415554-4943-5345-4e53-450000000001   ("NAUTICSENSE")
//     Data   4e415554-4943-5345-4e53-450000000002   NOTIFY  (ESP32 -> watch)
//     Cmd    4e415554-4943-5345-4e53-450000000003   WRITE   (watch -> ESP32)
//
// Unlike the HR hack this carries the whole dataset in one ~31-byte frame and
// is bidirectional (the watch can send commands such as MOB). The data frame
// layout MUST match BleDataSource.mc on the watch.
//
//   off  bytes  field            encoding
//   0    1      header           0xA5
//   1    2      validity bitmask  u16 LE (bit per field, see VBIT_* below)
//   3    2      heading          u16 LE  deg
//   5    2      cog              u16 LE  deg
//   7    2      sog              u16 LE  knots * 100
//   9    2      xte              i16 LE  nm * 1000   (signed: +stbd / -port)
//   11   2      bearing          u16 LE  deg
//   13   2      dtw              u16 LE  nm * 100
//   15   2      awa              i16 LE  deg
//   17   2      aws              u16 LE  knots * 100
//   19   2      twa              i16 LE  deg
//   21   2      tws              u16 LE  knots * 100
//   23   2      gust             u16 LE  knots * 100
//   25   2      depth            u16 LE  m * 100
//   27   2      temp             i16 LE  degC * 100
//   29   1      battery          u8      percent
//   30   1      flags            u8      bit0 anchor, bit1 shallow
//
#define NAUTIC_SVC_UUID   "4e415554-4943-5345-4e53-450000000001"
#define NAUTIC_DATA_UUID  "4e415554-4943-5345-4e53-450000000002"
#define NAUTIC_CMD_UUID   "4e415554-4943-5345-4e53-450000000003"

#define NAUTIC_FRAME_LEN  31
#define NAUTIC_HEADER     0xA5

// validity bitmask bit positions (match MarineData v* order)
enum NauticVBit {
  VBIT_HEADING = 0, VBIT_COG, VBIT_SOG, VBIT_XTE, VBIT_BRG, VBIT_DTW,
  VBIT_AWA, VBIT_AWS, VBIT_TWA, VBIT_TWS, VBIT_GUST, VBIT_DEPTH,
  VBIT_TEMP, VBIT_BATT
};

// commands the watch can send (Cmd characteristic, byte 0)
enum NauticCmd {
  CMD_NONE = 0x00,
  CMD_MOB  = 0x01,   // man overboard
  CMD_PING = 0x02    // keepalive / link check
};

class BleNauticLink {
public:
  void begin(const char* deviceName);
  void sendData(const MarineData& d);   // build + notify one frame
  bool connected() const;
  void tickLed();                       // solid = connected, blink = advertising

  // Poll the last command received from the watch (CMD_NONE if none new).
  uint8_t takeCommand();
};
