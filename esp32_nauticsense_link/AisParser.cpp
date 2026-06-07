#include "AisParser.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

// XOR checksum of the chars between the leading '!' and the '*'.
static bool aisChecksumOk(const char* line) {
  const char* star = strrchr(line, '*');
  if (!star || (int)strlen(star) < 3) { return false; }
  uint8_t c = 0;
  for (const char* p = line + 1; p < star; p++) { c ^= (uint8_t)*p; }
  uint8_t want = (uint8_t)strtol(star + 1, nullptr, 16);
  return c == want;
}

// n-th comma field (0-based): copies into out (NUL-terminated, truncated).
static void field(const char* s, int n, char* out, int outSz) {
  int f = 0; const char* start = s; out[0] = 0;
  for (const char* p = s; ; p++) {
    if (*p == ',' || *p == '*' || *p == 0) {
      if (f == n) {
        int len = (int)(p - start);
        if (len > outSz - 1) { len = outSz - 1; }
        memcpy(out, start, len); out[len] = 0;
        return;
      }
      if (*p == 0 || *p == '*') { return; }
      f++; start = p + 1;
    }
  }
}

// One 6-bit value from an armored AIS char.
static inline uint8_t sixbit(char c) {
  int v = (int)c - 48;
  if (v > 40) { v -= 8; }
  return (uint8_t)(v & 0x3F);
}

uint32_t AisParser::getU(const char* p, int plen, int start, int len) {
  uint32_t r = 0;
  for (int i = start; i < start + len; i++) {
    int ci = i / 6;
    if (ci >= plen) { return r << (start + len - i); }   // ran past payload
    uint8_t v = sixbit(p[ci]);
    int bit = (v >> (5 - (i % 6))) & 1;
    r = (r << 1) | bit;
  }
  return r;
}

int32_t AisParser::getS(const char* p, int plen, int start, int len) {
  uint32_t u = getU(p, plen, start, len);
  if (u & (1u << (len - 1))) { return (int32_t)u - (int32_t)(1u << len); }
  return (int32_t)u;
}

void AisParser::handleSentence(const char* line, uint32_t now) {
  if (!_targets) { return; }
  if (!aisChecksumOk(line)) { return; }

  char f1[6], f2[6], f3[6], payload[80];
  field(line, 1, f1, sizeof(f1));        // fragment count
  field(line, 2, f2, sizeof(f2));        // fragment number
  field(line, 3, f3, sizeof(f3));        // sequential message id (may be empty)
  field(line, 5, payload, sizeof(payload));

  int frags = atoi(f1);
  int num   = atoi(f2);
  int seq   = (f3[0]) ? atoi(f3) : -1;

  if (frags <= 1) {                      // single fragment → decode now
    decode(payload, (int)strlen(payload), now);
    return;
  }
  // multi-fragment reassembly
  if (num == 1) {
    _expFrags = frags; _gotFrags = 1; _seq = seq;
    _plen = 0; _payload[0] = 0;
    strncpy(_payload, payload, sizeof(_payload) - 1);
    _plen = (int)strlen(_payload);
  } else if (_expFrags == frags && _seq == seq && num == _gotFrags + 1) {
    int add = (int)strlen(payload);
    if (_plen + add < (int)sizeof(_payload)) {
      memcpy(_payload + _plen, payload, add + 1);
      _plen += add; _gotFrags++;
    }
  } else {
    _expFrags = 0;   // out of sequence → reset
    return;
  }
  if (_gotFrags == _expFrags) {
    decode(_payload, _plen, now);
    _expFrags = 0;
  }
}

void AisParser::decode(const char* p, int plen, uint32_t now) {
  if (plen < 1) { return; }
  uint32_t type = getU(p, plen, 0, 6);

  uint32_t mmsi; int32_t lonRaw, latRaw; uint32_t sogRaw, cogRaw;
  if (type == 1 || type == 2 || type == 3) {
    if (plen < 28) { return; }           // need ~168 bits
    mmsi   = getU(p, plen, 8, 30);
    sogRaw = getU(p, plen, 50, 10);
    lonRaw = getS(p, plen, 61, 28);
    latRaw = getS(p, plen, 89, 27);
    cogRaw = getU(p, plen, 116, 12);
  } else if (type == 18) {
    if (plen < 28) { return; }
    mmsi   = getU(p, plen, 8, 30);
    sogRaw = getU(p, plen, 46, 10);
    lonRaw = getS(p, plen, 57, 28);
    latRaw = getS(p, plen, 85, 27);
    cogRaw = getU(p, plen, 112, 12);
  } else {
    return;                              // not a position report we handle
  }

  float lon = lonRaw / 600000.0f;
  float lat = latRaw / 600000.0f;
  if (lon > 180.0f || lon < -180.0f || lat > 90.0f || lat < -90.0f) { return; }
  float sog = (sogRaw == 1023) ? 0.0f : sogRaw * 0.1f;
  float cog = (cogRaw >= 3600) ? 0.0f : cogRaw * 0.1f;

  _targets->upsert(mmsi, lat, lon, cog, sog, now);
}
