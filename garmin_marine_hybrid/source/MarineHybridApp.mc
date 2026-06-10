using Toybox.Application;
using Toybox.WatchUi;

// Marine Console — Hybrid edition (universal + MOB). Reads telemetry from the
// ESP32's BLE advertising by SCANNING (connectionless, like the beacon), and
// only opens a brief connection when you send MOB. One app for every generic-
// BLE Garmin (2019+), with the watch->boat back-channel back.
//
//   DataModel        — holds all instrument values
//   HybridDataSource — scans for telemetry; connects on demand to send MOB
//   View             — renders the model, owns the tick timer
//   Delegate         — touch + buttons (hold = MOB)
class MarineHybridApp extends Application.AppBase {

    var _model;
    var _source;
    var _view;
    var _delegate;

    function initialize() {
        AppBase.initialize();
        _model = new DataModel();
        _source = new HybridDataSource(_model);
        _view = new MarineHybridView(_model, _source);
        _delegate = new MarineHybridDelegate(_view);
    }

    function onStart(state) {
    }

    function onStop(state) {
    }

    function getInitialView() {
        return [ _view, _delegate ];
    }
}
