#include "Nmea0183.h"
#include "AisParser.h"
#include "config.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Depth-under-keel threshold (m) for the shallow alarm flag sent to the watch.
// (The boat instrument's transducer offset should already be applied via DPT.)
#ifndef NMEA_SHALLOW_M
#define NMEA_SHALLOW_M CFG_SHALLOW_M
#endif

static inline float wrap360f(float d) { d = fmodf(d, 360.0f); return d < 0 ? d + 360.0f : d; }

// NMEA lat/lon "ddmm.mmmm" (or "dddmm.mmmm") → decimal degrees.
static float nmeaToDeg(float v) {
  float deg = floorf(v / 100.0f);
  float min = v - deg * 100.0f;
  return deg + min / 60.0f;
}

// XOR checksum of the bytes between '$' and '*'.
static uint8_t nmeaChecksum(const char* body) {   // body without '$' and '*cs'
  uint8_t c = 0;
  while (*body) { c ^= (uint8_t)*body++; }
  return c;
}

// n-th comma-separated field (0-based). Returns pointer + length (not 0-term).
static const char* nthField(const char* s, int n, int& len) {
  int f = 0; const char* start = s;
  while (*s) {
    if (*s == ',') {
      if (f == n) { len = (int)(s - start); return start; }
      f++; start = s + 1;
    }
    s++;
  }
  if (f == n) { len = (int)(s - start); return start; }
  return nullptr;
}
static float fieldFloat(const char* s, int n) {
  int len; const char* p = nthField(s, n, len);
  if (!p || len == 0) { return NAN; }
  char tmp[16]; if (len > 15) { len = 15; }
  memcpy(tmp, p, len); tmp[len] = 0;
  return (float)atof(tmp);
}
static char fieldChar(const char* s, int n) {   // first char of field n (or 0)
  int len; const char* p = nthField(s, n, len);
  return (p && len > 0) ? p[0] : 0;
}

void Nmea0183::begin(HardwareSerial* port, uint32_t baud, int rxPin, int txPin, MarineData* data) {
  _ser = port; _d = data;
  _ser->begin(baud, SERIAL_8N1, rxPin, txPin);
}

// Assemble one byte into a line; parse on CR/LF.
void Nmea0183::feedChar(char c) {
  if (c == '\r' || c == '\n') {
    if (_len > 0) { _buf[_len] = 0; handleLine(_buf); _len = 0; }
  } else if (_len < (int)sizeof(_buf) - 1) {
    _buf[_len++] = c;
  } else {
    _len = 0;   // overflow → drop line
  }
}

void Nmea0183::feed(const uint8_t* data, int len) {
  for (int i = 0; i < len; i++) { feedChar((char)data[i]); }
}

void Nmea0183::poll() {
  if (!_ser) { return; }
  while (_ser->available()) { feedChar((char)_ser->read()); }
}

void Nmea0183::handleLine(char* line) {
  // AIS sentences (!AIVDM / !AIVDO) → forward to the AIS parser.
  if (line[0] == '!') {
    if (_echo) { Serial.print(F("[rx] ")); Serial.println(line); }
    if (_ais) { _ais->handleSentence(line, millis()); _good++; _lastRx = millis(); }
    else      { _bad++; }
    return;
  }
  if (line[0] != '$' || strlen(line) < 7) { _bad++; return; }
  if (_echo) { Serial.print(F("[rx] ")); Serial.println(line); }
  char* star = strrchr(line, '*');
  if (!star || strlen(star) < 3) { _bad++; return; }
  *star = 0;
  uint8_t want = (uint8_t)strtol(star + 1, nullptr, 16);
  if (nmeaChecksum(line + 1) != want) { _bad++; return; }   // line+1 = no '$'
  _good++; _lastRx = millis();

  const char* t = line + 3;                 // after '$' + 2-char talker id
  MarineData* d = _d;

  if (!strncmp(t, "HDT", 3)) {              // true heading
    float h = fieldFloat(line, 1);
    if (isfinite(h)) { d->headingTrue = wrap360f(h); d->vHeading = true; }

  } else if (!strncmp(t, "HDG", 3)) {       // mag heading (+ deviation, variation)
    float mag = fieldFloat(line, 1);
    if (isfinite(mag)) {
      float var = fieldFloat(line, 4);
      char vd = fieldChar(line, 5);
      float tru = mag;
      if (isfinite(var)) { tru = mag + (vd == 'W' ? -var : var); }
      d->headingTrue = wrap360f(tru); d->vHeading = true;
    }

  } else if (!strncmp(t, "VTG", 3)) {       // COG (true) + SOG (knots)
    float cog = fieldFloat(line, 1);
    float sog = fieldFloat(line, 5);
    if (isfinite(cog)) { d->cog = wrap360f(cog); d->vCog = true; }
    if (isfinite(sog)) { d->sog = sog;            d->vSog = true; }

  } else if (!strncmp(t, "RMC", 3)) {       // fallback COG/SOG + own position
    float sog = fieldFloat(line, 7);
    float cog = fieldFloat(line, 8);
    if (isfinite(sog)) { d->sog = sog;            d->vSog = true; }
    if (isfinite(cog)) { d->cog = wrap360f(cog);  d->vCog = true; }
    float la = fieldFloat(line, 3); char ns = fieldChar(line, 4);   // own position
    float lo = fieldFloat(line, 5); char ew = fieldChar(line, 6);
    if (isfinite(la) && isfinite(lo)) {
      float latd = nmeaToDeg(la); if (ns == 'S') { latd = -latd; }
      float lond = nmeaToDeg(lo); if (ew == 'W') { lond = -lond; }
      d->lat = latd; d->lon = lond; d->vPos = true;
    }

  } else if (!strncmp(t, "MWV", 3)) {       // wind: angle, R/T, speed, units, status
    if (fieldChar(line, 5) == 'A') {        // status valid
      float ang = fieldFloat(line, 1);
      float spd = fieldFloat(line, 3);
      char ref  = fieldChar(line, 2);
      char un   = fieldChar(line, 4);
      if (isfinite(spd)) {
        if (un == 'K')      { spd *= 0.539957f; }  // km/h → kn
        else if (un == 'M') { spd *= 1.94384f; }   // m/s  → kn
      }
      if (isfinite(ang)) { ang = wrap360f(ang); }
      if (ref == 'R') {                     // relative = apparent
        if (isfinite(ang)) { d->awa = ang; d->vAwa = true; }
        if (isfinite(spd)) { d->aws = spd; d->vAws = true; }
      } else if (ref == 'T') {              // theoretical = true
        if (isfinite(ang)) { d->twa = ang; d->vTwa = true; }
        if (isfinite(spd)) { d->tws = spd; d->vTws = true; }
      }
    }

  } else if (!strncmp(t, "DBT", 3)) {       // depth below transducer (m = field 3)
    float m = fieldFloat(line, 3);
    if (isfinite(m)) {
      d->depthUnderKeel = m; d->vDepth = true;
      d->shallowAlarm = (m < NMEA_SHALLOW_M);
    }

  } else if (!strncmp(t, "DPT", 3)) {       // depth + transducer offset
    float dep = fieldFloat(line, 1);
    float off = fieldFloat(line, 2);
    if (isfinite(dep)) {
      float ukc = dep + (isfinite(off) ? off : 0.0f);
      d->depthUnderKeel = ukc; d->vDepth = true;
      d->shallowAlarm = (ukc < NMEA_SHALLOW_M);
    }

  } else if (!strncmp(t, "MTW", 3)) {       // water temperature (°C)
    float c = fieldFloat(line, 1);
    if (isfinite(c)) { d->waterTemp = c; d->vTemp = true; }

  } else if (!strncmp(t, "XTE", 3)) {       // cross-track error (mag = field 3, L/R = 4)
    float mag = fieldFloat(line, 3);
    if (isfinite(mag)) {
      char lr = fieldChar(line, 4);
      d->xte = (lr == 'L') ? -mag : mag; d->vXte = true;
    }

  } else if (!strncmp(t, "RMB", 3)) {       // range (10) + bearing (11) to destination
    float rng = fieldFloat(line, 10);
    float brg = fieldFloat(line, 11);
    if (isfinite(rng)) { d->dtw = rng;            d->vDtw = true; }
    if (isfinite(brg)) { d->bearing = wrap360f(brg); d->vBrg = true; }
  }
}
