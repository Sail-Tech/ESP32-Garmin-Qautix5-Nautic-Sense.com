using Toybox.Sensor;
using Toybox.System;

// Real instrument feed for the quatix 5.
//
// The watch cannot do generic BLE, so the ESP32 pairs as a native HR sensor
// and the only datum we can read is Sensor.getInfo().heartRate — one integer.
// The firmware multiplexes the whole dataset over it with a tag/value scheme:
//   TAG  byte = 210 + fieldId   (announces the next field)
//   DATA byte = 0..199          (encoded value for that field)
// We gate each DATA on the last seen TAG, so a dropped byte just skips one
// field for one cycle. Keep the field ids / encodings in sync with the .ino.
//
// No data for STALE_MS -> the model is cleared and the UI shows "---".
class LiveDataSource extends DataSource {

    const TAG_BASE = 210;
    // Must exceed the gap between successful decodes (a data byte ~every few
    // seconds) AND the heading interval, or a brief gap wipes accumulated
    // fields. Generous so only a real disconnect clears the screen.
    const STALE_MS = 15000;

    // field ids (must match the firmware)
    const F_HDG   = 0;
    const F_COG   = 1;
    const F_SOG   = 2;
    const F_XTE   = 3;
    const F_AWA   = 4;
    const F_AWS   = 5;
    const F_TWA   = 6;
    const F_TWS   = 7;
    const F_GUST  = 8;
    const F_DEPTH = 9;
    const F_TEMP  = 10;
    const F_BATT  = 11;
    const F_FLAGS = 12;

    var _pendingTag = -1;
    var _lastRxMs = null;
    var _prevRaw = -1;

    function initialize() {
        DataSource.initialize();
    }

    function onStart() {
        Sensor.setEnabledSensors([Sensor.SENSOR_HEARTRATE]);
    }

    function onStop() {
        Sensor.setEnabledSensors([]);
    }

    // START button: re-arm the HR sensor and wipe decode state so the
    // multiplex stream resyncs from the next TAG.
    function reconnect() {
        // Toggle the sensor off then on — the strongest app-side nudge to
        // re-acquire the paired HR sensor — and wipe decode state to resync.
        Sensor.setEnabledSensors([]);
        Sensor.setEnabledSensors([Sensor.SENSOR_HEARTRATE]);
        _pendingTag = -1;
        _prevRaw = -1;
        _lastRxMs = null;
    }

    function update(model) {
        var info = Sensor.getInfo();
        var hr = null;
        if (info != null && (info has :heartRate)) {
            hr = info.heartRate;
        }
        if (hr != null) {
            // Debounce: only act on a value seen on two consecutive polls, to
            // ride out any HR smoothing the watch may apply before exposing it.
            if (hr == _prevRaw) {
                handleByte(model, hr);
            }
            _prevRaw = hr;
        }
        // Freshness: no valid byte for a while -> drop offline, clear instruments.
        if (_lastRxMs == null || (System.getTimer() - _lastRxMs) > STALE_MS) {
            model.reset();
        }
    }

    function handleByte(model, b) {
        if (b >= TAG_BASE && b <= TAG_BASE + F_FLAGS) {
            _pendingTag = b - TAG_BASE;
            return;
        }
        if (b >= 0 && b <= 199 && _pendingTag >= 0) {
            applyField(model, _pendingTag, b);
            _pendingTag = -1;
            _lastRxMs = System.getTimer();
            model.linkState = "ON";
            model.touch();
        }
    }

    function applyField(model, id, v) {
        if (id == F_HDG) {
            model.headingTrue = v * 2;
        } else if (id == F_COG) {
            model.cog = v * 2;
        } else if (id == F_SOG) {
            model.sog = v / 10.0;
        } else if (id == F_XTE) {
            model.xte = v / 100.0;
        } else if (id == F_AWA) {
            model.awa = v * 2;
        } else if (id == F_AWS) {
            model.aws = v / 10.0;
        } else if (id == F_TWA) {
            model.twa = v * 2;
        } else if (id == F_TWS) {
            model.tws = v / 10.0;
        } else if (id == F_GUST) {
            model.gust = v / 10.0;
        } else if (id == F_DEPTH) {
            model.depthUnderKeel = v / 4.0;
        } else if (id == F_TEMP) {
            model.waterTemp = v / 4.0;
        } else if (id == F_BATT) {
            model.battery = v;
        } else if (id == F_FLAGS) {
            model.anchorAlarm = (v & 1) != 0;
            model.shallowAlarm = (v & 2) != 0;
        }
    }
}
