using Toybox.BluetoothLowEnergy as Ble;
using Toybox.System;

// Hybrid feed: reads telemetry from the ESP32's advertising by SCANNING (no
// connection), and only opens a SHORT connection when the user sends MOB — then
// disconnects and resumes scanning. Works on every generic-BLE Garmin (2019+).
//
// Telemetry pages (manufacturer data, company 0xFFFF) are decoded exactly like
// the beacon edition. The MOB path uses the same custom service/command
// characteristic as the Venu 3 native link.
class HybridDataSource extends DataSource {

    const COMPANY = 0xFFFF;
    const MAGIC   = 0xB5;
    const STALE_MS = 8000;
    const AIS_TARGET_MS = 20000;

    // command service/characteristic (match BleNauticLink / BleHybridLink on ESP32)
    const SVC = "4e415554-4943-5345-4e53-450000000001";
    const CMD = "4e415554-4943-5345-4e53-450000000003";
    const CMD_MOB = 0x01;

    // validity bit positions
    const VB_HDG=0; const VB_COG=1; const VB_SOG=2; const VB_XTE=3; const VB_BRG=4;
    const VB_DTW=5; const VB_AWA=6; const VB_AWS=7; const VB_TWA=8; const VB_TWS=9;
    const VB_GUST=10; const VB_DEPTH=11; const VB_TEMP=12; const VB_BATT=13;

    var _model;
    var _delegate;
    var _svcUuid;
    var _cmdUuid;
    var _device;
    var _cmdChar;
    var _mobPending = false;
    var _lastRxMs = null;

    function initialize(model) {
        DataSource.initialize();
        _model = model;
    }

    function onStart() {
        _svcUuid = Ble.stringToUuid(SVC);
        _cmdUuid = Ble.stringToUuid(CMD);
        _delegate = new HybridBleDelegate(self);
        Ble.setDelegate(_delegate);
        // Register the command service so we can connect+write MOB later.
        try {
            Ble.registerProfile({
                :uuid => _svcUuid,
                :characteristics => [ { :uuid => _cmdUuid } ]
            });
        } catch (ex) {
        }
        startScan();   // telemetry by scanning (does not need the profile)
    }

    function onStop() {
        try {
            Ble.setScanState(Ble.SCAN_STATE_OFF);
            if (_device != null) { Ble.unpairDevice(_device); }
        } catch (ex) {
        }
        _device = null; _cmdChar = null;
    }

    function startScan() {
        try { Ble.setScanState(Ble.SCAN_STATE_SCANNING); } catch (ex) {}
    }

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

    // MOB: arm a pending request; the next matching advert triggers a brief
    // connect + write + disconnect.
    function sendMob() {
        _mobPending = true;
    }

    // ---- scan callback ----
    function onScan(scanResults) {
        for (var r = scanResults.next(); r != null; r = scanResults.next()) {
            var d = r.getManufacturerSpecificData(COMPANY);
            if (d != null && d.size() >= 6 && d[0] == MAGIC) {
                decodePage(d);
                if (_mobPending) {
                    _mobPending = false;
                    try {
                        Ble.setScanState(Ble.SCAN_STATE_OFF);
                        _device = Ble.pairDevice(r);   // connect to send MOB
                    } catch (ex) {
                        startScan();
                    }
                    return;
                }
            }
        }
    }

    function onConnected(device, state) {
        if (state == Ble.CONNECTION_STATE_CONNECTED) {
            _device = device;
            try {
                var svc = device.getService(_svcUuid);
                if (svc != null) {
                    _cmdChar = svc.getCharacteristic(_cmdUuid);
                    if (_cmdChar != null) {
                        _cmdChar.requestWrite([CMD_MOB]b,
                            { :writeType => Ble.WRITE_TYPE_WITH_RESPONSE });
                    }
                }
            } catch (ex) {
                disconnectAndScan();
            }
        } else {
            _device = null; _cmdChar = null;
            startScan();          // back to telemetry
        }
    }

    // After the MOB write completes, drop the connection (resumes scanning via
    // onConnected's disconnect branch).
    function onCharWrite(characteristic, status) {
        disconnectAndScan();
    }

    function disconnectAndScan() {
        try {
            if (_device != null) { Ble.unpairDevice(_device); }
        } catch (ex) {
        }
        _device = null; _cmdChar = null;
        startScan();
    }

    // ---- page decode (matches BeaconFrame.h) ----
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
            upsertAis(mmsi, u16(d, 10), u16(d, 12) / 100.0, u16(d, 14));
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

    function u16(b, o) { return b[o] | (b[o + 1] << 8); }
    function i16(b, o) { var v = u16(b, o); if (v >= 32768) { v -= 65536; } return v; }
    function bit(mask, n) { return (mask & (1 << n)) != 0; }
}

// BLE delegate forwarding scan + connection callbacks into the HybridDataSource.
class HybridBleDelegate extends Ble.BleDelegate {
    var _src;

    function initialize(src) {
        BleDelegate.initialize();
        _src = src;
    }

    function onScanResults(scanResults) {
        _src.onScan(scanResults);
    }

    function onConnectedStateChanged(device, state) {
        _src.onConnected(device, state);
    }

    function onCharacteristicWrite(characteristic, status) {
        _src.onCharWrite(characteristic, status);
    }

    function onProfileRegister(uuid, status) {
    }
}
