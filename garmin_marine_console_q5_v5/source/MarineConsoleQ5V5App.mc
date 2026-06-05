using Toybox.Application;
using Toybox.WatchUi;

// Marine Console Q5 V5 — application entry point.
//
// Wires the three layers together:
//   DataModel   — holds all instrument values
//   DataSource  — fills the model (SimDataSource now; LiveDataSource later)
//   View        — renders the model, owns the tick timer
//   Delegate    — maps watch buttons to view actions
//
// The single line that changes when real data arrives is the `new
// SimDataSource()` below.
class MarineConsoleQ5V5App extends Application.AppBase {

    var _model;
    var _source;
    var _view;
    var _delegate;

    function initialize() {
        AppBase.initialize();
        _model = new DataModel();
        _source = new LiveDataSource();   // real ESP32 feed; data only flows when paired
        _view = new MarineConsoleQ5V5View(_model, _source);
        _delegate = new MarineConsoleQ5V5Delegate(_view);
    }

    function onStart(state) {
    }

    function onStop(state) {
    }

    function getInitialView() {
        return [ _view, _delegate ];
    }
}
