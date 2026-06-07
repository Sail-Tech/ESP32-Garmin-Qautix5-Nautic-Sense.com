using Toybox.BluetoothLowEnergy as Ble;
using Toybox.System;

// Real instrument feed for the Venu 3 (and other generic-BLE Connect IQ
// watches). Unlike the quatix 5 HR hack, the Venu 3 supports generic BLE, so
// this acts as a BLE CENTRAL and connects directly to the ESP32's custom GATT
// service:
//
//   Service  4e415554-4943-5345-4e53-450000000001
//     Data   ...0002  NOTIFY  — the ESP32 pushes the whole dataset in one frame
//     Cmd    ...0003  WRITE   — the watch sends commands back (MOB)
//
// The data frame layout MUST match BleNauticLink.* on the ESP32 (see below).
// Because it is a real link it is bidirectional: sendMob() writes the command
// characteristic, so a man-overboard from the watch reaches the boat.
//
// Frame (31 bytes, little-endian):
//   0      header 0xA5
//   1..2   validity bitmask (u16)   bit per field, see VBIT_* order
//   3..4   heading  u16  deg          5..6   cog u16 deg
//   7..8   sog u16 kt*100             9..10  xte i16 nm*1000
//   11..12 bearing u16 deg            13..14 dtw u16 nm*100
//   15..16 awa i16 deg                17..18 aws u16 kt*100
//   19..20 twa i16 deg                21..22 tws u16 kt*100
//   23..24 gust u16 kt*100            25..26 depth u16 m*100
//   27..28 temp i16 C*100             29 battery u8     30 flags u8
class BleDataSource extends DataSource {

    const SVC  = "4e415554-4943-5345-4e53-450000000001";
    const DATA = "4e415554-4943-5345-4e53-450000000002";
    const CMD  = "4e415554-4943-5345-4e53-450000000003";
    const AIS  = "4e415554-4943-5345-4e53-450000000004";
    const DEVICE_NAME = "ESP32-NauticSense";

    const HEADER     = 0xA5;
    const AIS_HEADER = 0xA6;
    const STALE_MS   = 8000;
    const AIS_TARGET_MS = 20000;   // drop an AIS target not refreshed in this time
    const CMD_MOB    = 0x01;

    // validity bit positions (must match NauticVBit on the ESP32)
    const VB_HDG=0; const VB_COG=1; const VB_SOG=2; const VB_XTE=3; const VB_BRG=4;
    const VB_DTW=5; const VB_AWA=6; const VB_AWS=7; const VB_TWA=8; const VB_TWS=9;
    const VB_GUST=10; const VB_DEPTH=11; const VB_TEMP=12; const VB_BATT=13;

    var _model;
    var _delegate;
    var _svcUuid;
    var _dataUuid;
    var _cmdUuid;
    var _aisUuid;
    var _device;
    var _dataChar;
    var _cmdChar;
    var _aisChar;
    var _profileReady = false;
    var _aisArmed = false;
    var _lastRxMs = null;

    function initialize(model) {
        DataSource.initialize();
        _model = model;
    }

    function onStart() {
        _svcUuid  = Ble.stringToUuid(SVC);
        _dataUuid = Ble.stringToUuid(DATA);
        _cmdUuid  = Ble.stringToUuid(CMD);
        _aisUuid  = Ble.stringToUuid(AIS);
        _delegate = new NauticBleDelegate(self);
        Ble.setDelegate(_delegate);
        registerProfile();
        // Scanning starts once the profile is registered (onProfileRegistered).
    }

    function onStop() {
        try {
            Ble.setScanState(Ble.SCAN_STATE_OFF);
            if (_device != null) {
                Ble.unpairDevice(_device);
            }
        } catch (ex) {
        }
        _device = null;
        _dataChar = null;
        _cmdChar = null;
    }

    function registerProfile() {
        var profile = {
            :uuid => _svcUuid,
            :characteristics => [
                { :uuid => _dataUuid, :descriptors => [ Ble.cccdUuid() ] },
                { :uuid => _aisUuid,  :descriptors => [ Ble.cccdUuid() ] },
                { :uuid => _cmdUuid }
            ]
        };
        try {
            Ble.registerProfile(profile);
        } catch (ex) {
            // Already registered (e.g. after onStop/onStart) — fine.
            _profileReady = true;
            startScan();
        }
    }

    function startScan() {
        try {
            Ble.setScanState(Ble.SCAN_STATE_SCANNING);
        } catch (ex) {
        }
    }

    // START button: drop the link and rescan from scratch.
    function reconnect() {
        try {
            if (_device != null) {
                Ble.unpairDevice(_device);
            }
        } catch (ex) {
        }
        _device = null;
        _dataChar = null;
        _cmdChar = null;
        _lastRxMs = null;
        _model.reset();
        startScan();
    }

    // Called every tick by the View — data itself arrives via callbacks, so
    // here we only enforce freshness (clear the model if the feed went quiet).
    function update(model) {
        _model = model;
        if (_lastRxMs == null || (System.getTimer() - _lastRxMs) > STALE_MS) {
            model.reset();
        } else {
            pruneAis();   // drop AIS targets we haven't heard about recently
        }
    }

    // Remove AIS targets older than AIS_TARGET_MS (entry = [mmsi,brg,dist,cog,ms]).
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

    // Send MOB to the ESP32 over the command characteristic (bidirectional).
    function sendMob() {
        if (_cmdChar == null) { return; }
        try {
            _cmdChar.requestWrite([CMD_MOB]b,
                { :writeType => Ble.WRITE_TYPE_WITH_RESPONSE });
        } catch (ex) {
        }
    }

    // ---- callbacks from the delegate ----

    function onProfileRegistered() {
        _profileReady = true;
        startScan();
    }

    function onScan(scanResults) {
        for (var r = scanResults.next(); r != null; r = scanResults.next()) {
            if (matches(r)) {
                try {
                    Ble.setScanState(Ble.SCAN_STATE_OFF);
                    _device = Ble.pairDevice(r);
                } catch (ex) {
                }
                return;
            }
        }
    }

    function matches(scanResult) {
        var name = scanResult.getDeviceName();
        if (name != null && name.equals(DEVICE_NAME)) {
            return true;
        }
        var uuids = scanResult.getServiceUuids();
        if (uuids != null) {
            for (var u = uuids.next(); u != null; u = uuids.next()) {
                if (u.equals(_svcUuid)) {
                    return true;
                }
            }
        }
        return false;
    }

    function onConnected(device, connectionState) {
        if (connectionState == Ble.CONNECTION_STATE_CONNECTED) {
            _device = device;
            subscribe(device);
        } else {
            _device = null;
            _dataChar = null;
            _cmdChar = null;
            _model.reset();
            startScan();   // try to get it back
        }
    }

    function subscribe(device) {
        try {
            var svc = device.getService(_svcUuid);
            if (svc == null) { return; }
            _dataChar = svc.getCharacteristic(_dataUuid);
            _cmdChar  = svc.getCharacteristic(_cmdUuid);
            _aisChar  = svc.getCharacteristic(_aisUuid);
            _aisArmed = false;
            if (_dataChar != null) {
                var cccd = _dataChar.getDescriptor(Ble.cccdUuid());
                if (cccd != null) { cccd.requestWrite([0x01, 0x00]b); }   // notifications on
            }
            // The AIS CCCD is enabled after the data one (onDescriptorWrite chains it).
        } catch (ex) {
        }
    }

    // Enable AIS notifications once the data CCCD write has completed (one
    // outstanding GATT operation at a time on Connect IQ).
    function enableAisNotify() {
        if (_aisArmed) { return; }   // one-shot: this fires again for its own write
        _aisArmed = true;
        try {
            if (_aisChar != null) {
                var cccd = _aisChar.getDescriptor(Ble.cccdUuid());
                if (cccd != null) { cccd.requestWrite([0x01, 0x00]b); }
            }
        } catch (ex) {
        }
    }

    function onDataFrame(value) {
        if (value == null || value.size() < 31) { return; }
        if (value[0] != HEADER) { return; }

        var valid = u16(value, 1);
        _model.headingTrue   = bit(valid, VB_HDG)   ? u16(value, 3)            : null;
        _model.cog           = bit(valid, VB_COG)   ? u16(value, 5)            : null;
        _model.sog           = bit(valid, VB_SOG)   ? u16(value, 7) / 100.0    : null;
        _model.xte           = bit(valid, VB_XTE)   ? i16(value, 9) / 1000.0   : null;
        _model.bearing       = bit(valid, VB_BRG)   ? u16(value, 11)           : null;
        _model.dtw           = bit(valid, VB_DTW)   ? u16(value, 13) / 100.0   : null;
        _model.awa           = bit(valid, VB_AWA)   ? i16(value, 15)           : null;
        _model.aws           = bit(valid, VB_AWS)   ? u16(value, 17) / 100.0   : null;
        _model.twa           = bit(valid, VB_TWA)   ? i16(value, 19)           : null;
        _model.tws           = bit(valid, VB_TWS)   ? u16(value, 21) / 100.0   : null;
        _model.gust          = bit(valid, VB_GUST)  ? u16(value, 23) / 100.0   : null;
        _model.depthUnderKeel= bit(valid, VB_DEPTH) ? u16(value, 25) / 100.0   : null;
        _model.waterTemp     = bit(valid, VB_TEMP)  ? i16(value, 27) / 100.0   : null;
        _model.battery       = bit(valid, VB_BATT)  ? value[29]                : null;

        var flags = value[30];
        _model.anchorAlarm  = (flags & 0x01) != 0;
        _model.shallowAlarm = (flags & 0x02) != 0;
        _model.aisAlarm     = (flags & 0x04) != 0;

        _model.linkState = "ON";
        _model.touch();
        _lastRxMs = System.getTimer();
    }

    // Route an incoming notification to the right decoder by characteristic.
    function onNotify(characteristic, value) {
        if (characteristic == null || value == null) { return; }
        if (characteristic.getUuid().equals(_aisUuid)) {
            onAisFrame(value);
        } else {
            onDataFrame(value);
        }
    }

    // One AIS target (15-byte frame); upsert into the model list by MMSI.
    //   0 hdr 0xA6  1 count  2 idx  3..6 mmsi  7..8 brg  9..10 dist*100  11..12 cog  13..14 sog*10
    function onAisFrame(value) {
        if (value.size() < 15 || value[0] != AIS_HEADER) { return; }
        var mmsi = value[3] | (value[4] << 8) | (value[5] << 16) | (value[6] << 24);
        var brg  = u16(value, 7);
        var dist = u16(value, 9) / 100.0;
        var cog  = u16(value, 11);
        var now  = System.getTimer();
        if (_model.aisTargets == null) { _model.aisTargets = []; }
        // replace existing entry for this MMSI, else append
        for (var i = 0; i < _model.aisTargets.size(); i++) {
            if (_model.aisTargets[i][0] == mmsi) {
                _model.aisTargets[i] = [mmsi, brg, dist, cog, now];
                return;
            }
        }
        _model.aisTargets.add([mmsi, brg, dist, cog, now]);
    }

    // ---- little-endian readers ----
    function u16(b, o) {
        return b[o] | (b[o + 1] << 8);
    }
    function i16(b, o) {
        var v = u16(b, o);
        if (v >= 32768) { v -= 65536; }
        return v;
    }
    function bit(mask, n) {
        return (mask & (1 << n)) != 0;
    }
}

// Thin BLE delegate that forwards every callback into the BleDataSource. Kept
// separate because BleDelegate and DataSource are different base classes.
class NauticBleDelegate extends Ble.BleDelegate {
    var _src;

    function initialize(src) {
        BleDelegate.initialize();
        _src = src;
    }

    function onProfileRegister(uuid, status) {
        _src.onProfileRegistered();
    }

    function onScanResults(scanResults) {
        _src.onScan(scanResults);
    }

    function onConnectedStateChanged(device, state) {
        _src.onConnected(device, state);
    }

    function onCharacteristicChanged(characteristic, value) {
        _src.onNotify(characteristic, value);
    }

    // After the data CCCD write completes, enable the AIS CCCD (one GATT
    // operation may be outstanding at a time on Connect IQ).
    function onDescriptorWrite(descriptor, status) {
        _src.enableAisNotify();
    }
}
