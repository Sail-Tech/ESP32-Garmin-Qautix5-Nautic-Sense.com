using Toybox.BluetoothLowEnergy as Ble;
using Toybox.System;

// Connectionless feed: SCANS for the ESP32's BLE advertisements and reads the
// dataset straight from the manufacturer-specific data — no pairing, no
// connection. Works on every generic-BLE Garmin (2019+), so a single app binary
// covers fēnix/epix/Venu/Forerunner/Edge/etc.
//
// The ESP32 rotates the data through several "pages" in successive adverts; we
// accumulate them into the model. Layout MUST match BleBeaconLink.* on the ESP32.
//
// Manufacturer payload (company id 0xFFFF stripped), little-endian. Common
// header: 0 magic 0xB5 | 1 page | 2..3 validity u16 | 4 flags | 5 aisCount
//   page 0 NAV : 6 hdg | 8 cog | 10 sog*100 | 12 brg | 14 dtw*100 | 16 xte i16*1000
//   page 1 WIND: 6 awa i16 | 8 aws*100 | 10 twa i16 | 12 tws*100 | 14 gust*100
//   page 2 ENV : 6 depth*100 | 8 temp i16*100 | 10 batt
//   page 3 AIS : 6 mmsi u32 | 10 brg | 12 dist*100 | 14 cog   (one target, rotating)
class BeaconDataSource extends DataSource {

    const COMPANY = 0xFFFF;        // BLE company id used by the ESP32 beacon
    const MAGIC   = 0xB5;
    const STALE_MS = 8000;
    const AIS_TARGET_MS = 20000;

    // validity bit positions (match NauticVBit / BleBeaconLink)
    const VB_HDG=0; const VB_COG=1; const VB_SOG=2; const VB_XTE=3; const VB_BRG=4;
    const VB_DTW=5; const VB_AWA=6; const VB_AWS=7; const VB_TWA=8; const VB_TWS=9;
    const VB_GUST=10; const VB_DEPTH=11; const VB_TEMP=12; const VB_BATT=13;

    var _model;
    var _delegate;
    var _lastRxMs = null;

    function initialize(model) {
        DataSource.initialize();
        _model = model;
    }

    function onStart() {
        _delegate = new BeaconBleDelegate(self);
        Ble.setDelegate(_delegate);
        startScan();
    }

    function onStop() {
        try { Ble.setScanState(Ble.SCAN_STATE_OFF); } catch (ex) {}
    }

    function startScan() {
        try { Ble.setScanState(Ble.SCAN_STATE_SCANNING); } catch (ex) {}
    }

    // START button: re-arm scanning and clear the model.
    function reconnect() {
        try { Ble.setScanState(Ble.SCAN_STATE_OFF); } catch (ex) {}
        _lastRxMs = null;
        _model.reset();
        startScan();
    }

    function update(model) {
        _model = model;
        if (_lastRxMs == null || (System.getTimer() - _lastRxMs) > STALE_MS) {
            model.reset();
        } else {
            pruneAis();
        }
    }

    function pruneAis() {
        if (_model.aisTargets == null || _model.aisTargets.size() == 0) { return; }
        var now = System.getTimer();
        var kept = [];
        for (var i = 0; i < _model.aisTargets.size(); i++) {
            var e = _model.aisTargets[i];
            if (now - e[4] <= AIS_TARGET_MS) { kept.add(e); }
        }
        _model.aisTargets = kept;
    }

    // ---- scan callback ----
    function onScan(scanResults) {
        for (var r = scanResults.next(); r != null; r = scanResults.next()) {
            var d = r.getManufacturerSpecificData(COMPANY);
            if (d != null && d.size() >= 6 && d[0] == MAGIC) {
                decodePage(d);
            }
        }
    }

    function decodePage(d) {
        var page = d[1];
        var valid = u16(d, 2);
        var flags = d[4];
        _model.anchorAlarm  = (flags & 0x01) != 0;
        _model.shallowAlarm = (flags & 0x02) != 0;
        _model.aisAlarm     = (flags & 0x04) != 0;

        if (page == 0 && d.size() >= 18) {            // NAV
            _model.headingTrue = bit(valid, VB_HDG) ? u16(d, 6)            : null;
            _model.cog         = bit(valid, VB_COG) ? u16(d, 8)            : null;
            _model.sog         = bit(valid, VB_SOG) ? u16(d, 10) / 100.0   : null;
            _model.bearing     = bit(valid, VB_BRG) ? u16(d, 12)           : null;
            _model.dtw         = bit(valid, VB_DTW) ? u16(d, 14) / 100.0   : null;
            _model.xte         = bit(valid, VB_XTE) ? i16(d, 16) / 1000.0  : null;
        } else if (page == 1 && d.size() >= 16) {     // WIND
            _model.awa  = bit(valid, VB_AWA)  ? i16(d, 6)            : null;
            _model.aws  = bit(valid, VB_AWS)  ? u16(d, 8) / 100.0    : null;
            _model.twa  = bit(valid, VB_TWA)  ? i16(d, 10)          : null;
            _model.tws  = bit(valid, VB_TWS)  ? u16(d, 12) / 100.0   : null;
            _model.gust = bit(valid, VB_GUST) ? u16(d, 14) / 100.0   : null;
        } else if (page == 2 && d.size() >= 11) {     // ENV
            _model.depthUnderKeel = bit(valid, VB_DEPTH) ? u16(d, 6) / 100.0  : null;
            _model.waterTemp      = bit(valid, VB_TEMP)  ? i16(d, 8) / 100.0  : null;
            _model.battery        = bit(valid, VB_BATT)  ? d[10]              : null;
        } else if (page == 3 && d.size() >= 16) {     // AIS target
            var mmsi = d[6] | (d[7] << 8) | (d[8] << 16) | (d[9] << 24);
            var brg  = u16(d, 10);
            var dist = u16(d, 12) / 100.0;
            var cog  = u16(d, 14);
            upsertAis(mmsi, brg, dist, cog);
        }

        _model.linkState = "ON";
        _model.touch();
        _lastRxMs = System.getTimer();
    }

    function upsertAis(mmsi, brg, dist, cog) {
        var now = System.getTimer();
        if (_model.aisTargets == null) { _model.aisTargets = []; }
        for (var i = 0; i < _model.aisTargets.size(); i++) {
            if (_model.aisTargets[i][0] == mmsi) {
                _model.aisTargets[i] = [mmsi, brg, dist, cog, now];
                return;
            }
        }
        _model.aisTargets.add([mmsi, brg, dist, cog, now]);
    }

    // ---- little-endian readers ----
    function u16(b, o) { return b[o] | (b[o + 1] << 8); }
    function i16(b, o) { var v = u16(b, o); if (v >= 32768) { v -= 65536; } return v; }
    function bit(mask, n) { return (mask & (1 << n)) != 0; }
}

// Scan-only BLE delegate forwarding results into the BeaconDataSource.
class BeaconBleDelegate extends Ble.BleDelegate {
    var _src;

    function initialize(src) {
        BleDelegate.initialize();
        _src = src;
    }

    function onScanResults(scanResults) {
        _src.onScan(scanResults);
    }
}
