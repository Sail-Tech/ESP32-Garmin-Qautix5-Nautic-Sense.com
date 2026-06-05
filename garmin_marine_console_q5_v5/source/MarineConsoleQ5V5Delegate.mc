using Toybox.WatchUi;
using Toybox.System;

// Maps watch input to view actions. Uses BehaviorDelegate so it works off
// abstract behaviours (next/previous page, menu) rather than a fixed button
// layout. onKey() stays as a fallback to discover the quatix 5 key codes if
// the default mapping ever feels wrong.
class MarineConsoleQ5V5Delegate extends WatchUi.BehaviorDelegate {

    var _view;

    function initialize(view) {
        BehaviorDelegate.initialize();
        _view = view;
    }

    function onNextPage() {
        _view.nextPage();
        return true;
    }

    function onPreviousPage() {
        _view.previousPage();
        return true;
    }

    function onMenu() {
        _view.toggleSunMode();
        return true;
    }

    // START / ENTER button → dismiss an alarm overlay if shown, else reconnect.
    function onSelect() {
        _view.onStartButton();
        return true;
    }

    function onKey(keyEvent) {
        // Fallback: trigger reconnect on START/ENTER even if onSelect() doesn't
        // map to it on this device.
        var k = keyEvent.getKey();
        if (k == WatchUi.KEY_ENTER || k == WatchUi.KEY_START) {
            _view.onStartButton();
            return true;
        }
        return false;
    }
}
