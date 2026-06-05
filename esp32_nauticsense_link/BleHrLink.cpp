#include "BleHrLink.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLECharacteristic* s_hrChar = nullptr;
static bool                  s_connected = false;

// -- Status LED (onboard) ----------------------------------------------------
// The ESP32-D0WD-V3 (DevKit) board has only one software-controllable LED: the
// BLUE one on GPIO2. The RED LED is the POWER LED (hard-wired) and CANNOT be
// turned off from software. Blue LED behaviour: SOLID = connected to the
// Garmin; BLINKING = waiting (advertising).
#define LED_PIN  CFG_LED_PIN     // see config.h
#define LED_ON   CFG_LED_ON

class HrServerCallbacks : public NimBLEServerCallbacks {
  // NimBLE-Arduino 2.x signatures.
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    s_connected = true;
    digitalWrite(LED_PIN, LED_ON);
    Serial.println(">>> BLE connected (watch reading) — blue LED solid ON");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    s_connected = false;
    Serial.printf(">>> BLE disconnected (reason %d), re-advertising — LED blinking\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

void BleHrLink::begin(const char* deviceName) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);   // off at boot (will blink while waiting)

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // TX power enum

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new HrServerCallbacks());

  NimBLEService* hr = server->createService("180D");
  s_hrChar = hr->createCharacteristic("2A37", NIMBLE_PROPERTY::NOTIFY);
  // 2.x: o CCCD (0x2902) é criado automaticamente para chars NOTIFY.

  // Body Sensor Location (0x2A38) = "Other"; some hosts expect it.
  NimBLECharacteristic* bodyLoc = hr->createCharacteristic("2A38", NIMBLE_PROPERTY::READ);
  uint8_t loc = 1;
  bodyLoc->setValue(&loc, 1);

  hr->start();

  // Minimal, discoverable advertising: name + HR service fit in the 31-byte
  // (~26B) payload and NimBLE adds the "general discoverable" flags. (This is
  // the pattern that produced "LINK OK"; no appearance/scan-response, to keep
  // the watch's discovery reliable.)
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(deviceName);
  adv->addServiceUUID("180D");
  adv->start();
}

void BleHrLink::sendByte(uint8_t b) {
  if (s_hrChar == nullptr) { return; }
  uint8_t payload[2] = { 0x00, b };   // HR Measurement: flags=0, uint8 value
  s_hrChar->setValue(payload, 2);
  if (s_connected) { s_hrChar->notify(); }
}

bool BleHrLink::connected() const {
  return s_connected;
}

// Drive the status LED: solid when connected, slow blink while advertising.
void BleHrLink::tickLed() {
  static uint32_t last = 0;
  static bool ph = false;
  if (s_connected) {
    digitalWrite(LED_PIN, LED_ON);
  } else if (millis() - last >= 400) {
    last = millis();
    ph = !ph;
    digitalWrite(LED_PIN, ph ? LED_ON : !LED_ON);
  }
}
