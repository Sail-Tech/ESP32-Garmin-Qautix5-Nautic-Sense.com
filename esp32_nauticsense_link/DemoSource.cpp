#include "DemoSource.h"

void DemoSource::update(uint32_t nowMs) {
  if (_lastMs == 0) { _lastMs = nowMs; }
  if (nowMs - _lastMs < 1000) { return; }   // advance ~1 Hz
  _lastMs += 1000;
  _t++;

  uint32_t t = _t;
  int hdg = (236 + (int)(t * 2)) % 360;

  // NAV
  _data.headingTrue = hdg;
  _data.cog         = (hdg + 4) % 360;
  _data.sog         = 6.6f + (t % 8) * 0.2f;
  _data.xte         = (t % 7) * 0.01f;
  _data.bearing     = (hdg + 8) % 360;
  _data.dtw         = 2.4f + (t % 6) * 0.1f;

  // WIND
  _data.aws  = 13.0f + (t % 6) * 0.5f;
  _data.awa  = 20 + (int)((t * 4) % 50);
  _data.tws  = 15.0f + (t % 5) * 0.7f;
  _data.twa  = 30 + (int)((t * 3) % 60);
  _data.gust = _data.tws + 3.5f;

  // DEPTH / ENV
  _data.depthUnderKeel = 17.0f + (t % 6) * 0.5f;
  _data.waterTemp      = 21.0f + (t % 4) * 0.1f;
  _data.battery        = 87 - (int)(t % 10);
  _data.anchorAlarm    = ((t % 20) >= 16);
  _data.shallowAlarm   = (_data.depthUnderKeel < 18.0f);

  // demo provides every field
  _data.vHeading = _data.vCog = _data.vSog = _data.vXte = _data.vBrg =
  _data.vDtw = _data.vAwa = _data.vAws = _data.vTwa = _data.vTws =
  _data.vGust = _data.vDepth = _data.vTemp = _data.vBatt = true;
}
