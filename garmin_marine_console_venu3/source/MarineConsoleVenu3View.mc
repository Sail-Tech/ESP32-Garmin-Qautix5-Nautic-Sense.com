using Toybox.WatchUi;
using Toybox.Graphics as Gfx;
using Toybox.System;
using Toybox.Timer;
using Toybox.Attention;
using Toybox.Math;

// Marine Console — Venu 3 view / presentation layer.
//
// Same instrument set and OCEAN-palette look as the quatix 5 build, but:
//   * the layout is RESPONSIVE — every coordinate is scaled from the original
//     240 px design baseline to the device's real size (454 px on the Venu 3),
//     via P(); so the one View serves the round Venu 3 / Venu 3S and degrades
//     gracefully on other round devices.
//   * input is touch + buttons (see the Delegate): swipe/tap to page, the
//     action button (or alarm tap) to confirm, touch-and-hold to send MOB.
//   * data arrives over a real BLE link (BleDataSource), not the HR hack, so
//     the link is bidirectional (MOB goes back to the ESP32).
class MarineConsoleVenu3View extends WatchUi.View {

    const NUM_PAGES = 8;
    const WAKE_MS = 30000;
    const DESIGN = 240.0;   // original layout baseline; everything scales from here

    // Compass / dial palette (vibrant OCEAN variant).
    const OC_DISC  = 0x081521;
    const OC_DISC2 = 0x0E2236;
    const OC_LINE  = 0x3A4F73;
    const OC_CYAN  = 0x00E0FF;
    const OC_WHITE = 0xFFFFFF;
    const OC_MUTED = 0xA8BAD0;
    const OC_GREEN = 0x37FF5A;
    const OC_RED   = 0xFF2E2E;

    // display field ids
    const D_HDG=0; const D_COG=1; const D_SOG=2; const D_DTW=3; const D_XTE=4;
    const D_BRG=5; const D_AWA=6; const D_AWS=7; const D_TWA=8; const D_TWS=9;
    const D_GUST=10; const D_DEPTH=11; const D_TEMP=12; const D_BATT=13;

    var _timer;
    var _model;
    var _source;
    var _page = 0;
    var _sunMode = false;
    var _wakeUntil = 0;
    var _lastLitMs = 0;
    var _tick = 0;
    var _reconnectUntil = 0;
    var _alarmAck = false;
    var _alarmVibrated = false;
    var _clearSinceMs = null;
    var _mobUntil = 0;       // show the MOB-sent confirmation until this time

    var _w = 240;
    var _h = 240;
    var _scale = 1.0;

    function initialize(model, source) {
        View.initialize();
        _model = model;
        _source = source;
    }

    function onLayout(dc) {
        _w = dc.getWidth();
        _h = dc.getHeight();
        _scale = _w / DESIGN;
    }

    // Scale a design-space length/coordinate to this device.
    function P(v) {
        return (v * _scale).toNumber();
    }
    function cx() { return _w / 2; }

    function onShow() {
        _source.onStart();
        keepLit();
        _timer = new Timer.Timer();
        _timer.start(method(:onTick), 250, true);
    }

    function keepLit() {
        _wakeUntil = System.getTimer() + WAKE_MS;
        _lastLitMs = 0;
    }

    function serviceBacklight() {
        var now = System.getTimer();
        if (now < _wakeUntil && (now - _lastLitMs) >= 2000) {
            _lastLitMs = now;
            try {
                Attention.backlight(true);
            } catch (ex) {
                // BacklightOnTooLongException can fire on AMOLED (burn-in guard).
            }
        }
    }

    function onHide() {
        if (_timer != null) {
            _timer.stop();
            _timer = null;
        }
        _source.onStop();
    }

    function onTick() as Void {
        _source.update(_model);
        serviceBacklight();

        var active = (_model.anchorAlarm || _model.shallowAlarm || _model.aisAlarm);
        if (active) {
            _clearSinceMs = null;
            if (!_alarmVibrated) {
                alarmVibrate();
                _alarmVibrated = true;
                keepLit();
            }
        } else {
            _alarmAck = false;
            if (_clearSinceMs == null) {
                _clearSinceMs = System.getTimer();
            } else if (System.getTimer() - _clearSinceMs > 10000) {
                _alarmVibrated = false;
            }
        }

        _tick += 1;
        if (_tick % 2 == 0) {
            WatchUi.requestUpdate();
        }
    }

    function nextPage() {
        _page = (_page + 1) % NUM_PAGES;
        keepLit();
        WatchUi.requestUpdate();
    }

    function previousPage() {
        _page = (_page + NUM_PAGES - 1) % NUM_PAGES;
        keepLit();
        WatchUi.requestUpdate();
    }

    function toggleSunMode() {
        _sunMode = !_sunMode;
        keepLit();
        WatchUi.requestUpdate();
    }

    function reconnect() {
        _source.reconnect();
        _model.reset();
        _reconnectUntil = System.getTimer() + 1500;
        keepLit();
        WatchUi.requestUpdate();
    }

    // Touch-and-hold → send MOB to the ESP32 (bidirectional link). Shows a
    // brief confirmation. Ignored while an alarm overlay is up.
    function triggerMob() {
        if (alarmShowing()) { return; }
        if (_source has :sendMob) {
            _source.sendMob();
        }
        _mobUntil = System.getTimer() + 2500;
        alarmVibrate();
        keepLit();
        WatchUi.requestUpdate();
    }

    function alarmShowing() {
        return (_model.anchorAlarm || _model.shallowAlarm || _model.aisAlarm) && !_alarmAck;
    }

    function onStartButton() {
        if (alarmShowing()) {
            _alarmAck = true;
            keepLit();
            WatchUi.requestUpdate();
        } else {
            reconnect();
        }
    }

    function alarmVibrate() {
        if (Attention has :vibrate) {
            try {
                Attention.vibrate([new Attention.VibeProfile(100, 700)]);
            } catch (ex) {
            }
        }
    }

    function drawAlarmOverlay(dc) {
        dc.setColor(Gfx.COLOR_RED, Gfx.COLOR_RED);
        dc.fillRectangle(0, 0, _w, _h);
        var l1; var l2; var detail = null;
        if (_model.anchorAlarm) {
            l1 = "ANCHOR"; l2 = "DRAG";
        } else if (_model.shallowAlarm) {
            l1 = "SHALLOW"; l2 = "WATER";
            detail = "DEPTH " + fmt1(_model.depthUnderKeel) + " m";
        } else {
            l1 = "AIS"; l2 = "TARGET";
            detail = aisNearestStr();
        }
        txt(dc, cx(), P(60),  Gfx.FONT_MEDIUM, "! ALARM !", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, cx(), P(96),  Gfx.FONT_LARGE,  l1, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, cx(), P(132), Gfx.FONT_LARGE,  l2, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        if (detail != null) {
            txt(dc, cx(), P(170), Gfx.FONT_SMALL, detail, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        }
        txt(dc, cx(), P(202), Gfx.FONT_TINY, "tap = OK", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
    }

    function drawMobOverlay(dc) {
        dc.setColor(Gfx.COLOR_RED, Gfx.COLOR_RED);
        dc.fillRectangle(0, 0, _w, _h);
        txt(dc, cx(), P(80),  Gfx.FONT_LARGE,  "MOB", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, cx(), P(120), Gfx.FONT_MEDIUM, "SENT", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, cx(), P(160), Gfx.FONT_TINY,   "to NauticSense", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
    }

    // ---- theme colours ----
    function bgCol()     { return _sunMode ? Gfx.COLOR_WHITE : Gfx.COLOR_BLACK; }
    function fgCol()     { return _sunMode ? Gfx.COLOR_BLACK : 0xFFFFFF; }
    function labelCol()  { return _sunMode ? Gfx.COLOR_DK_GRAY : 0xB8C8D6; }
    function accentCol() { return _sunMode ? Gfx.COLOR_BLACK : 0x00E0FF; }
    function goodCol()   { return _sunMode ? Gfx.COLOR_BLACK : 0x37FF5A; }
    function infoCol()   { return _sunMode ? Gfx.COLOR_BLACK : 0x33B5FF; }
    function warnCol()   { return 0xFF2E2E; }
    function barCol()    { return _sunMode ? Gfx.COLOR_BLACK : 0x0A57E6; }

    function isOnline() { return _model.linkState.equals("ON"); }

    // ---- formatting helpers ----
    function fmtInt3(v) {
        if (v == null) { return "---"; }
        var n = v.toNumber();
        var s = n.toString();
        if (n < 10)  { return "00" + s; }
        if (n < 100) { return "0" + s; }
        return s;
    }
    function fmt1(v) {
        if (v == null) { return "--.-"; }
        var scaled = (v * 10).toNumber();
        var intPart = scaled / 10;
        var dec = scaled % 10;
        if (dec < 0) { dec = -dec; }
        return intPart.toString() + "." + dec.toString();
    }
    function fmt2(v) {
        if (v == null) { return "--.--"; }
        var scaled = (v * 100).toNumber();
        var intPart = scaled / 100;
        var dec = scaled % 100;
        if (dec < 0) { dec = -dec; }
        if (dec < 10) { return intPart.toString() + ".0" + dec.toString(); }
        return intPart.toString() + "." + dec.toString();
    }
    function intStr(v) {
        if (v == null) { return "--"; }
        return v.toNumber().toString();
    }

    function txt(dc, x, y, font, s, just, color) {
        dc.setColor(color, Gfx.COLOR_TRANSPARENT);
        dc.drawText(x, y, font, s, just);
    }

    // ---- chrome ----
    function pageTitle() {
        if (_page == 0) { return "HEADING"; }
        if (_page == 1) { return "APP WIND"; }
        if (_page == 2) { return "TRUE WIND"; }
        if (_page == 3) { return "SPEED"; }
        if (_page == 4) { return "TRACK"; }
        if (_page == 5) { return "DEPTH"; }
        if (_page == 6) { return "STATUS"; }
        return "AIS";
    }

    function drawHeader(dc) {
        dc.setColor(barCol(), Gfx.COLOR_TRANSPARENT);
        dc.fillRectangle(0, 0, _w, P(26));
        txt(dc, cx(), P(7), Gfx.FONT_TINY, pageTitle(), Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
    }

    function drawFooter(dc) {
        var page = (_page + 1).toString() + "/" + NUM_PAGES.toString() + "  ";
        if (System.getTimer() < _reconnectUntil) {
            txt(dc, cx(), P(206), Gfx.FONT_TINY, page + "RECONNECT...", Gfx.TEXT_JUSTIFY_CENTER, accentCol());
            return;
        }
        var online = isOnline();
        txt(dc, cx(), P(206), Gfx.FONT_TINY, page + (online ? "LINK OK" : "NO LINK"),
            Gfx.TEXT_JUSTIFY_CENTER, online ? goodCol() : warnCol());
    }

    function drawBlock(dc, yTop, yBot, label, value, unit, color) {
        var rx = P(206);
        txt(dc, rx, P(yTop), Gfx.FONT_TINY, label, Gfx.TEXT_JUSTIFY_RIGHT, labelCol());
        txt(dc, cx(), P(yTop + 18), Gfx.FONT_NUMBER_MEDIUM, value, Gfx.TEXT_JUSTIFY_CENTER, color);
        txt(dc, rx, P(yBot - 16), Gfx.FONT_TINY, unit, Gfx.TEXT_JUSTIFY_RIGHT, labelCol());
    }

    function drawField(dc, id, yTop, yBot) {
        if (id == D_SOG) {
            drawBlock(dc, yTop, yBot, "SOG", fmt1(_model.sog), "knots", goodCol());
        } else if (id == D_DTW) {
            drawBlock(dc, yTop, yBot, "DTW", fmt1(_model.dtw), "nm to wpt", fgCol());
        } else if (id == D_XTE) {
            drawBlock(dc, yTop, yBot, "XTE", fmt2(_model.xte), "nm off", fgCol());
        } else if (id == D_BRG) {
            drawBlock(dc, yTop, yBot, "BRG", fmtInt3(_model.bearing), "° to wpt", infoCol());
        } else if (id == D_GUST) {
            drawBlock(dc, yTop, yBot, "GUST", fmt1(_model.gust), "knots", warnCol());
        } else if (id == D_DEPTH) {
            var dCol = (_model.depthUnderKeel != null && _model.depthUnderKeel < 2.0) ? warnCol() : accentCol();
            drawBlock(dc, yTop, yBot, "DEPTH", fmt1(_model.depthUnderKeel), "m under keel", dCol);
        } else if (id == D_TEMP) {
            drawBlock(dc, yTop, yBot, "WATER", fmt1(_model.waterTemp), "°C", fgCol());
        } else if (id == D_BATT) {
            drawBlock(dc, yTop, yBot, "BATTERY", intStr(_model.battery), "%", goodCol());
        }
    }

    function pageFieldA() {
        if (_page == 3) { return D_SOG; }
        if (_page == 4) { return D_XTE; }
        if (_page == 5) { return D_DEPTH; }
        return D_GUST;
    }
    function pageFieldB() {
        if (_page == 3) { return D_DTW; }
        if (_page == 4) { return D_BRG; }
        if (_page == 5) { return D_TEMP; }
        return D_BATT;
    }

    // ---- compass ----
    function drawRoseLabel(dc, ox, oy, rr, bearing, hdg, label, color) {
        var a = (bearing - hdg - 90) * Math.PI / 180.0;
        var x = ox + (Math.cos(a) * rr);
        var y = oy + (Math.sin(a) * rr);
        txt(dc, x, y - P(8), Gfx.FONT_TINY, label, Gfx.TEXT_JUSTIFY_CENTER, color);
    }

    function drawOceanCompass(dc, ox, oy, r) {
        var heading = _model.headingTrue;
        var cog = _model.cog;
        var hdg = (heading == null) ? 0 : heading;

        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(ox, oy, r);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(P(2));
        dc.drawCircle(ox, oy, r);

        dc.setPenWidth(P(5));
        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(ox, oy, r + P(1), Gfx.ARC_COUNTER_CLOCKWISE, 90, 160);
        dc.setColor(OC_GREEN, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(ox, oy, r + P(1), Gfx.ARC_CLOCKWISE, 90, 20);

        var i = 0;
        while (i < 360) {
            var a = (i - hdg - 90) * Math.PI / 180.0;
            var cs = Math.cos(a);
            var sn = Math.sin(a);
            var len; var w; var col;
            if (i % 30 == 0)      { len = P(14); w = P(3); col = OC_WHITE; }
            else if (i % 10 == 0) { len = P(9);  w = P(2); col = OC_MUTED; }
            else                  { len = P(5);  w = 1;    col = OC_LINE; }
            var r1 = r - P(3);
            var r2 = r - P(3) - len;
            dc.setColor(col, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(w);
            dc.drawLine(ox + (cs * r1), oy + (sn * r1), ox + (cs * r2), oy + (sn * r2));
            i += 5;
        }
        dc.setPenWidth(1);

        drawRoseLabel(dc, ox, oy, r - P(28), 0,   hdg, "N", OC_RED);
        drawRoseLabel(dc, ox, oy, r - P(28), 90,  hdg, "E", OC_WHITE);
        drawRoseLabel(dc, ox, oy, r - P(28), 180, hdg, "S", OC_WHITE);
        drawRoseLabel(dc, ox, oy, r - P(28), 270, hdg, "W", OC_WHITE);

        if (cog != null && heading != null) {
            var ca = (cog - hdg - 90) * Math.PI / 180.0;
            dc.setColor(OC_CYAN, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(P(2));
            dc.drawLine(ox, oy, ox + (Math.cos(ca) * (r * 0.82)), oy + (Math.sin(ca) * (r * 0.82)));
            dc.setPenWidth(1);
        }

        var ir = (r * 0.5).toNumber();
        dc.setColor(OC_DISC2, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(ox, oy, ir);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawCircle(ox, oy, ir);

        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([[ox, oy - r + P(7)], [ox - P(7), oy - r - P(7)], [ox + P(7), oy - r - P(7)]]);

        txt(dc, ox, oy - P(30), Gfx.FONT_NUMBER_MEDIUM, fmtInt3(heading), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        txt(dc, ox, oy + P(16), Gfx.FONT_TINY, "° TRUE", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
        if (cog != null) {
            txt(dc, ox, oy + P(32), Gfx.FONT_TINY, "COG " + fmtInt3(cog), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        }
    }

    // ---- wind dial ----
    function windLabel(dc, ox, oy, rr, deg, label, color) {
        var a = (deg - 90) * Math.PI / 180.0;
        txt(dc, ox + (Math.cos(a) * rr), oy + (Math.sin(a) * rr) - P(8),
            Gfx.FONT_TINY, label, Gfx.TEXT_JUSTIFY_CENTER, color);
    }

    function drawWindNeedle(dc, ox, oy, r, angleDeg, color, w) {
        if (angleDeg == null) { return; }
        var a = (angleDeg - 90) * Math.PI / 180.0;
        var cs = Math.cos(a);
        var sn = Math.sin(a);
        var rimX = ox + (cs * (r - P(6)));
        var rimY = oy + (sn * (r - P(6)));
        dc.setColor(color, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(w);
        dc.drawLine(ox + (cs * (r * 0.5)), oy + (sn * (r * 0.5)), rimX, rimY);
        dc.setPenWidth(1);
        var bx = ox + (cs * (r - P(16)));
        var by = oy + (sn * (r - P(16)));
        var px = -sn;
        var py = cs;
        dc.fillPolygon([[rimX, rimY], [bx + (px * P(6)), by + (py * P(6))], [bx - (px * P(6)), by - (py * P(6))]]);
    }

    function drawWindDial(dc, ox, oy, r, apparent) {
        var awa = _model.awa;
        var twa = _model.twa;

        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(ox, oy, r);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(P(2));
        dc.drawCircle(ox, oy, r);

        dc.setPenWidth(P(5));
        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(ox, oy, r + P(1), Gfx.ARC_COUNTER_CLOCKWISE, 90, 270);
        dc.setColor(OC_GREEN, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(ox, oy, r + P(1), Gfx.ARC_CLOCKWISE, 90, 270);

        var i = 0;
        while (i < 360) {
            var a = (i - 90) * Math.PI / 180.0;
            var cs = Math.cos(a);
            var sn = Math.sin(a);
            var len; var w;
            if (i % 90 == 0)      { len = P(13); w = P(3); }
            else if (i % 30 == 0) { len = P(9);  w = P(2); }
            else                  { len = P(5);  w = 1; }
            dc.setColor(OC_MUTED, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(w);
            dc.drawLine(ox + (cs * (r - P(3))), oy + (sn * (r - P(3))), ox + (cs * (r - P(3) - len)), oy + (sn * (r - P(3) - len)));
            i += 15;
        }
        dc.setPenWidth(1);

        windLabel(dc, ox, oy, r - P(22), 0,   "0",   OC_WHITE);
        windLabel(dc, ox, oy, r - P(22), 90,  "90",  OC_GREEN);
        windLabel(dc, ox, oy, r - P(22), 180, "180", OC_WHITE);
        windLabel(dc, ox, oy, r - P(22), 270, "90",  OC_RED);

        dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([[ox, oy - r + P(6)], [ox - P(6), oy - r - P(6)], [ox + P(6), oy - r - P(6)]]);

        if (apparent) {
            drawWindNeedle(dc, ox, oy, r, twa, OC_GREEN, P(2));
            drawWindNeedle(dc, ox, oy, r, awa, OC_CYAN, P(4));
        } else {
            drawWindNeedle(dc, ox, oy, r, awa, OC_CYAN, P(2));
            drawWindNeedle(dc, ox, oy, r, twa, OC_GREEN, P(4));
        }

        var ir = (r * 0.46).toNumber();
        dc.setColor(OC_DISC2, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(ox, oy, ir);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawCircle(ox, oy, ir);

        if (apparent) {
            txt(dc, ox, oy - P(30), Gfx.FONT_NUMBER_MEDIUM, fmt1(_model.aws), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
            txt(dc, ox, oy + P(16), Gfx.FONT_TINY, "kt  ·  AWA " + intStr(_model.awa) + "°", Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        } else {
            txt(dc, ox, oy - P(30), Gfx.FONT_NUMBER_MEDIUM, fmt1(_model.tws), Gfx.TEXT_JUSTIFY_CENTER, OC_GREEN);
            txt(dc, ox, oy + P(16), Gfx.FONT_TINY, "kt  ·  TWA " + intStr(_model.twa) + "°", Gfx.TEXT_JUSTIFY_CENTER, OC_GREEN);
        }
    }

    // ---- AIS ----
    // "X.X nm  BBB°" for the closest tracked target (for the alarm overlay).
    function aisNearestStr() {
        var t = _model.aisTargets;
        if (t == null || t.size() == 0) { return "—"; }
        var bi = 0; var bd = t[0][2];
        for (var i = 1; i < t.size(); i++) {
            if (t[i][2] < bd) { bd = t[i][2]; bi = i; }
        }
        return fmt1(t[bi][2]) + " nm  " + fmtInt3(t[bi][1]) + "°";
    }

    function drawAisTriangle(dc, x, y, dirDeg, color) {
        var th = dirDeg * Math.PI / 180.0;
        var dx = Math.sin(th);    var dy = -Math.cos(th);
        var px = Math.cos(th);    var py = Math.sin(th);
        var L = P(7);
        var B = P(5);
        dc.setColor(color, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([
            [x + (dx * L), y + (dy * L)],
            [x - (dx * L * 0.7) + (px * B), y - (dy * L * 0.7) + (py * B)],
            [x - (dx * L * 0.7) - (px * B), y - (dy * L * 0.7) - (py * B)]
        ]);
    }

    // AIS plot — demo targets (a real AIS feed needs a richer transport).
    function drawAisPlot(dc, ox, oy, R) {
        var hdg = (_model.headingTrue == null) ? 0 : _model.headingTrue;

        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(ox, oy, R);

        dc.setPenWidth(1);
        var ringNm = [2, 5, 10];
        var k = 0;
        while (k < 3) {
            var rr = R * (ringNm[k] / 10.0);
            dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
            dc.drawCircle(ox, oy, rr);
            txt(dc, ox - (rr * 0.707), oy - (rr * 0.707) - P(7), Gfx.FONT_TINY,
                ringNm[k].toString(), Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
            k += 1;
        }
        txt(dc, ox, oy + R - P(14), Gfx.FONT_TINY, "NM", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);

        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawLine(ox, oy, ox, oy - R);

        var scale = R / 10.0;
        var tgs = _model.aisTargets;
        if (isOnline() && tgs != null && tgs.size() > 0) {
            // REAL: every target the ESP32 streams (entry = [mmsi,brg,dist,cog,ms]).
            for (var i = 0; i < tgs.size(); i++) {
                var brg = tgs[i][1]; var dist = tgs[i][2]; var crs = tgs[i][3];
                if (dist > 10) { continue; }
                var a = (brg - hdg - 90) * Math.PI / 180.0;
                var rad = dist * scale;
                var tx = ox + (Math.cos(a) * rad);
                var ty = oy + (Math.sin(a) * rad);
                if (dist <= 5.0) {
                    drawAisTriangle(dc, tx, ty, crs - hdg, OC_CYAN);
                } else {
                    dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT);
                    dc.fillCircle(tx, ty, P(3));
                }
            }
            txt(dc, ox, oy + P(40), Gfx.FONT_TINY, tgs.size().toString() + " TGT",
                Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        } else if (!isOnline()) {
            // OFFLINE: demo targets just to show the standard layout.
            var tg = [[30,1.5,210],[75,3.0,290],[140,4.5,10],[200,7.0,90],[290,9.0,180],[330,6.0,45]];
            var j = 0;
            while (j < 6) {
                var brg = tg[j][0]; var dist = tg[j][1]; var crs = tg[j][2];
                var a = (brg - hdg - 90) * Math.PI / 180.0;
                var rad = dist * scale;
                var tx = ox + (Math.cos(a) * rad);
                var ty = oy + (Math.sin(a) * rad);
                if (dist <= 5.0) { drawAisTriangle(dc, tx, ty, crs - hdg, OC_CYAN); }
                else { dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT); dc.fillCircle(tx, ty, P(3)); }
                j += 1;
            }
        } else {
            txt(dc, ox, oy + P(40), Gfx.FONT_TINY, "NO AIS", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
        }

        drawAisTriangle(dc, ox, oy, 0, OC_GREEN);
    }

    function onUpdate(dc) {
        dc.setColor(fgCol(), bgCol());
        dc.clear();
        drawHeader(dc);

        var cyc = P(124);   // content centre (matches the design baseline)
        if (_page == 0) {
            drawOceanCompass(dc, cx(), cyc, P(90));
        } else if (_page == 1) {
            drawWindDial(dc, cx(), cyc, P(88), true);
        } else if (_page == 2) {
            drawWindDial(dc, cx(), cyc, P(88), false);
        } else if (_page == 7) {
            drawAisPlot(dc, cx(), cyc, P(90));
        } else {
            drawField(dc, pageFieldA(), 32, 116);
            dc.setColor(labelCol(), Gfx.COLOR_TRANSPARENT);
            dc.drawLine(P(34), P(120), P(206), P(120));
            drawField(dc, pageFieldB(), 124, 200);
        }
        drawFooter(dc);

        if (alarmShowing()) {
            drawAlarmOverlay(dc);
        } else if (System.getTimer() < _mobUntil) {
            drawMobOverlay(dc);
        }
    }
}
