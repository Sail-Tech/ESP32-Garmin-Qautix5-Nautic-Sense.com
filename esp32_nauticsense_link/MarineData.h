#pragma once
#include <stdint.h>

// The full marine dataset, in engineering units. A DataSource fills it; the
// LinkProtocol encodes it for the watch. Keep field meanings in sync with the
// watch DataModel.mc.
struct MarineData {
  // NAV
  float headingTrue  = 0;   // deg [0..359]
  float cog          = 0;   // deg [0..359]
  float sog          = 0;   // knots
  float xte          = 0;   // nautical miles
  float bearing      = 0;   // deg, bearing to waypoint
  float dtw          = 0;   // nautical miles, distance to waypoint
  // WIND
  float awa          = 0;   // deg apparent
  float aws          = 0;   // knots apparent
  float twa          = 0;   // deg true
  float tws          = 0;   // knots true
  float gust         = 0;   // knots
  // DEPTH / ENV
  float depthUnderKeel = 0; // metres
  float waterTemp      = 0; // deg C
  int   battery        = 0; // percent
  bool  anchorAlarm    = false;
  bool  shallowAlarm   = false;

  // OWN POSITION (from RMC/GGA) — needed to place AIS targets relative to us.
  float lat = 0;            // deg, + = N
  float lon = 0;            // deg, + = E
  bool  vPos = false;

  // AIS summary — the closest target (carried on the HR link to the quatix 5)
  // plus the proximity-guard alarm. The full target list lives in AisTargets
  // (sent over the native BLE link to the Venu 3).
  float aisNearBrg  = 0;    // deg true, bearing to nearest target
  float aisNearDist = 0;    // nautical miles to nearest target
  bool  vAisNear    = false;
  uint8_t aisCount  = 0;    // number of tracked targets
  bool  aisAlarm    = false; // a target is inside the guard ring

  // Per-field validity. A field is only transmitted (and shown on the watch)
  // when valid; otherwise the watch keeps showing "---". This matters when a
  // sensor is absent on the boat's NMEA bus — sending 0 would read as a real
  // value (e.g. DEPTH 0.0 m would falsely trigger the shallow alarm).
  // FLAGS (alarms) are always transmitted.
  bool vHeading = false, vCog = false, vSog = false, vXte = false, vBrg = false,
       vDtw = false, vAwa = false, vAws = false, vTwa = false, vTws = false,
       vGust = false, vDepth = false, vTemp = false, vBatt = false;
};
