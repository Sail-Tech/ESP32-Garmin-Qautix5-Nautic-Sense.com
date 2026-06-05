#pragma once
#include <stdint.h>

// BLE transport: advertises the ESP32 as a native Heart Rate sensor
// (service 0x180D, char 0x2A37) so the quatix 5 pairs it under
// Settings > Sensors & Accessories. One protocol byte is sent as the HR value
// of each notification — the only datum the watch can read on this device.
//
// NimBLE objects are kept in the .cpp so this header stays free of NimBLE
// includes.
class BleHrLink {
public:
  void begin(const char* deviceName);
  void sendByte(uint8_t b);   // emits {flags=0, b} as a HR notification
  bool connected() const;
  void tickLed();             // call each loop; solid = connected, blink = waiting
};
