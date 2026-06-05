#include "Nmea2000Reader.h"
#include "config.h"          // USE_NMEA2000, CAN pins, shallow threshold

#ifdef USE_NMEA2000
// ── Real implementation (needs the NMEA2000 libs + a CAN transceiver) ──
#include <Arduino.h>
#include <math.h>

// CAN pins from config.h (must be set before including NMEA2000_CAN.h).
#ifndef ESP32_CAN_TX_PIN
#define ESP32_CAN_TX_PIN ((gpio_num_t)CFG_CAN_TX_PIN)
#endif
#ifndef ESP32_CAN_RX_PIN
#define ESP32_CAN_RX_PIN ((gpio_num_t)CFG_CAN_RX_PIN)
#endif
#include <NMEA2000_CAN.h>     // creates the global `NMEA2000` object for ESP32
#include <N2kMessages.h>

#ifndef NMEA_SHALLOW_M
#define NMEA_SHALLOW_M CFG_SHALLOW_M
#endif
static const double MS_TO_KN = 1.94384;

static MarineData*       s_d      = nullptr;
static volatile uint32_t s_lastRx = 0;

static inline double wrap360d(double d) { d = fmod(d, 360.0); return d < 0 ? d + 360.0 : d; }

static void handleN2k(const tN2kMsg &msg) {
  if (!s_d) { return; }
  MarineData* d = s_d;

  switch (msg.PGN) {
    case 127250L: {                                   // Heading
      unsigned char sid; double h, dev, var; tN2kHeadingReference ref;
      if (ParseN2kHeading(msg, sid, h, dev, var, ref) && !N2kIsNA(h)) {
        double deg = (ref == N2khr_true)
                   ? h * RAD_TO_DEG
                   : (h + (N2kIsNA(var) ? 0.0 : var)) * RAD_TO_DEG;
        d->headingTrue = (float)wrap360d(deg); d->vHeading = true; s_lastRx = millis();
      }
      break;
    }
    case 129026L: {                                   // COG/SOG rapid
      unsigned char sid; tN2kHeadingReference ref; double cog, sog;
      if (ParseN2kCOGSOGRapid(msg, sid, ref, cog, sog)) {
        if (!N2kIsNA(cog)) { d->cog = (float)wrap360d(cog * RAD_TO_DEG); d->vCog = true; }
        if (!N2kIsNA(sog)) { d->sog = (float)(sog * MS_TO_KN);           d->vSog = true; }
        s_lastRx = millis();
      }
      break;
    }
    case 130306L: {                                   // Wind
      unsigned char sid; double ws, wa; tN2kWindReference ref;
      if (ParseN2kWindSpeed(msg, sid, ws, wa, ref)) {
        float kn  = N2kIsNA(ws) ? NAN : (float)(ws * MS_TO_KN);
        float deg = N2kIsNA(wa) ? NAN : (float)wrap360d(wa * RAD_TO_DEG);
        bool isTrue = (ref == N2kWind_True_North || ref == N2kWind_True_boat ||
                       ref == N2kWind_True_water || ref == N2kWind_Magnetic);
        if (isTrue) {
          if (isfinite(deg)) { d->twa = deg; d->vTwa = true; }
          if (isfinite(kn))  { d->tws = kn;  d->vTws = true; }
        } else {
          if (isfinite(deg)) { d->awa = deg; d->vAwa = true; }
          if (isfinite(kn))  { d->aws = kn;  d->vAws = true; }
        }
        s_lastRx = millis();
      }
      break;
    }
    case 128267L: {                                   // Water depth
      unsigned char sid; double dep, off;
      if (ParseN2kWaterDepth(msg, sid, dep, off) && !N2kIsNA(dep)) {
        float ukc = (float)(dep + (N2kIsNA(off) ? 0.0 : off));
        d->depthUnderKeel = ukc; d->vDepth = true;
        d->shallowAlarm = (ukc < NMEA_SHALLOW_M);
        s_lastRx = millis();
      }
      break;
    }
    case 130312L:
    case 130316L: {                                   // Temperature
      unsigned char sid, inst; tN2kTempSource src; double act, sett;
      bool ok = (msg.PGN == 130316L)
              ? ParseN2kTemperatureExt(msg, sid, inst, src, act, sett)
              : ParseN2kTemperature(msg, sid, inst, src, act, sett);
      if (ok && src == N2kts_SeaTemperature && !N2kIsNA(act)) {
        d->waterTemp = (float)KelvinToC(act); d->vTemp = true; s_lastRx = millis();
      }
      break;
    }
    case 129283L: {                                   // Cross-track error (m → nm)
      unsigned char sid; tN2kXTEMode mode; bool term; double xte;
      if (ParseN2kXTE(msg, sid, mode, term, xte) && !N2kIsNA(xte)) {
        d->xte = (float)(xte / 1852.0); d->vXte = true; s_lastRx = millis();
      }
      break;
    }
    case 129284L: {                                   // Navigation data (dist/bearing)
      unsigned char sid; double dtw; tN2kHeadingReference br;
      bool pc, ace; tN2kDistanceCalculationType ct;
      double eta; int16_t etad; double bod, bpd; uint32_t ow, dw;
      double dlat, dlon, wcv;
      if (ParseN2kNavigationInfo(msg, sid, dtw, br, pc, ace, ct, eta, etad,
                                 bod, bpd, ow, dw, dlat, dlon, wcv)) {
        if (!N2kIsNA(dtw)) { d->dtw = (float)(dtw / 1852.0); d->vDtw = true; }
        double b = !N2kIsNA(bpd) ? bpd : bod;
        if (!N2kIsNA(b))   { d->bearing = (float)wrap360d(b * RAD_TO_DEG); d->vBrg = true; }
        s_lastRx = millis();
      }
      break;
    }
    case 127506L: {                                   // DC status → battery %
      unsigned char sid, inst; tN2kDCType ty; unsigned char soc, soh;
      double tr, rv, cap;
      if (ParseN2kDCStatus(msg, sid, inst, ty, soc, soh, tr, rv, cap)) {
        if (soc != N2kUInt8NA) { d->battery = soc; d->vBatt = true; s_lastRx = millis(); }
      }
      break;
    }
    default: break;
  }
}

void Nmea2000Reader::begin(MarineData* data) {
  s_d = data;
  NMEA2000.SetMode(tNMEA2000::N2km_ListenOnly);
  NMEA2000.SetMsgHandler(handleN2k);
  NMEA2000.EnableForward(false);
  NMEA2000.Open();
  Serial.println("[N2K] listen-only aberto (precisa de transceiver CAN)");
}
void Nmea2000Reader::poll()            { NMEA2000.ParseMessages(); }
uint32_t Nmea2000Reader::lastRxMs() const { return s_lastRx; }

#else
// ── Stub: USE_NMEA2000 not defined → no NMEA2000 lib needed, no-ops ──
void Nmea2000Reader::begin(MarineData*) {}
void Nmea2000Reader::poll() {}
uint32_t Nmea2000Reader::lastRxMs() const { return 0; }
#endif
