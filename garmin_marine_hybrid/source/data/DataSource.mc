// Abstract feed for the DataModel.
//
// A DataSource fills the shared DataModel in place on each tick. The View
// owns the timer and the model; it does not care where the numbers come
// from. To change the data origin you swap the DataSource in the App and
// touch nothing else.
//
//   SimDataSource   -> synthetic values (current V5 milestone)
//   LiveDataSource  -> real instrument data via the native sensor channel
//                      (skeleton only; wired in the link phase)
class DataSource {

    function initialize() {
    }

    // Called once when the view becomes visible.
    function onStart() {
    }

    // Called once when the view is hidden.
    function onStop() {
    }

    // Called every tick (~1 Hz) to refresh `model` in place.
    function update(model) {
    }

    // Re-establish the feed (START button): re-arm the source and reset any
    // decode state so it resyncs cleanly. No-op for sources that don't need it.
    function reconnect() {
    }
}
