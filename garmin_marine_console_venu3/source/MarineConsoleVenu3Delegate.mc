using Toybox.WatchUi;
using Toybox.System;

// Maps Venu 3 input to view actions. The Venu 3 is touch + buttons, so this
// handles both:
//   * swipe up / right  -> next page      swipe down / left -> previous page
//   * tap               -> dismiss an alarm / MOB overlay (else ignored)
//   * touch-and-hold     -> send MOB to the ESP32 (bidirectional link)
//   * action button (onSelect / START / ENTER) -> dismiss alarm, else reconnect
//   * menu (hold button) -> toggle the SUN high-contrast theme
// BehaviorDelegate gives us the abstract behaviours; InputDelegate (its parent)
// gives the raw touch gestures.
class MarineConsoleVenu3Delegate extends WatchUi.BehaviorDelegate {

    var _view;

    function initialize(view) {
        BehaviorDelegate.initialize();
        _view = view;
    }

    // ---- abstract behaviours (buttons / system gestures) ----
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

    function onSelect() {
        _view.onStartButton();
        return true;
    }

    function onKey(keyEvent) {
        var k = keyEvent.getKey();
        if (k == WatchUi.KEY_ENTER || k == WatchUi.KEY_START) {
            _view.onStartButton();
            return true;
        }
        if (k == WatchUi.KEY_MENU) {
            _view.toggleSunMode();
            return true;
        }
        return false;
    }

    // ---- raw touch gestures (touchscreen) ----
    function onSwipe(swipeEvent) {
        var d = swipeEvent.getDirection();
        if (d == WatchUi.SWIPE_UP || d == WatchUi.SWIPE_RIGHT) {
            _view.nextPage();
        } else if (d == WatchUi.SWIPE_DOWN || d == WatchUi.SWIPE_LEFT) {
            _view.previousPage();
        }
        return true;
    }

    function onTap(clickEvent) {
        // A tap acknowledges an alarm overlay; otherwise it does nothing
        // (paging is by swipe/buttons, so taps don't change pages by accident).
        if (_view.alarmShowing()) {
            _view.onStartButton();
        }
        return true;
    }

    function onHold(clickEvent) {
        // Touch-and-hold = MOB. Sent to the ESP32 over the command characteristic.
        _view.triggerMob();
        return true;
    }
}
