using Toybox.Application;
using Toybox.WatchUi;

// Marine Console — Venu 3 application entry point.
//
// Wires the layers together:
//   DataModel      — holds all instrument values
//   BleDataSource  — fills the model from the ESP32 over a custom BLE GATT
//                    service (generic BLE central; bidirectional)
//   View           — renders the model, owns the tick timer
//   Delegate       — maps touch + buttons to view actions
class MarineConsoleVenu3App extends Application.AppBase {

    var _model;
    var _source;
    var _view;
    var _delegate;

    function initialize() {
        AppBase.initialize();
        _model = new DataModel();
        _source = new BleDataSource(_model);   // real BLE link to the ESP32
        _view = new MarineConsoleVenu3View(_model, _source);
        _delegate = new MarineConsoleVenu3Delegate(_view);
    }

    function onStart(state) {
    }

    function onStop(state) {
    }

    function getInitialView() {
        return [ _view, _delegate ];
    }
}
