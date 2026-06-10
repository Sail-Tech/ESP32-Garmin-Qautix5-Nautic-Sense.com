using Toybox.Application;
using Toybox.WatchUi;

// Marine Console — Beacon edition (universal). Reads the ESP32's data from the
// BLE advertising payload by SCANNING — no pairing, no connection — so one app
// runs on every generic-BLE Garmin (2019+). One-way (telemetry only).
//
//   DataModel        — holds all instrument values
//   BeaconDataSource — fills the model from scanned advertising packets
//   View             — renders the model, owns the tick timer
//   Delegate         — maps touch + buttons to view actions
class MarineBeaconApp extends Application.AppBase {

    var _model;
    var _source;
    var _view;
    var _delegate;

    function initialize() {
        AppBase.initialize();
        _model = new DataModel();
        _source = new BeaconDataSource(_model);   // connectionless scan of the ESP32 beacon
        _view = new MarineBeaconView(_model, _source);
        _delegate = new MarineBeaconDelegate(_view);
    }

    function onStart(state) {
    }

    function onStop(state) {
    }

    function getInitialView() {
        return [ _view, _delegate ];
    }
}
