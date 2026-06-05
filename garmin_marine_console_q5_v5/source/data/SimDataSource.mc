// Synthetic data feed.
//
// Reproduces the V4 demo motion (heading sweeps, wind wanders, depth
// oscillates) but writes into the shared DataModel instead of the View.
// This is the active source for the V5 milestone. When real heading is
// available, the App swaps this for LiveDataSource and nothing else changes.
class SimDataSource extends DataSource {

    var _tick = 0;

    function initialize() {
        DataSource.initialize();
    }

    function onStart() {
        _tick = 0;
    }

    function update(model) {
        _tick += 1;
        var t = _tick;

        // NAV
        var hdg = (236 + (t * 2)) % 360;
        model.headingTrue = hdg;
        model.cog = (hdg + 4) % 360;
        model.sog = 6.6 + ((t % 8) * 0.2);
        model.xte = (t % 7) * 0.01;
        model.bearing = (hdg + 8) % 360;
        model.dtw = 2.4 + ((t % 6) * 0.1);
        model.waypoint = "WP-ALFA";

        // WIND
        model.aws = 13.0 + ((t % 6) * 0.5);
        model.awa = 20 + ((t * 4) % 50);
        model.tws = 15.0 + ((t % 5) * 0.7);
        model.twa = 30 + ((t * 3) % 60);
        model.gust = model.tws + 3.5;

        // DEPTH / ENV
        model.depthUnderKeel = 17.0 + ((t % 6) * 0.5);
        model.waterTemp = 21.0 + ((t % 4) * 0.1);
        model.battery = 87 - (t % 10);
        model.anchorAlarm = ((t % 20) >= 16);
        model.shallowAlarm = (model.depthUnderKeel < 18.0);

        model.linkState = "SIM";
        model.touch();
    }
}
