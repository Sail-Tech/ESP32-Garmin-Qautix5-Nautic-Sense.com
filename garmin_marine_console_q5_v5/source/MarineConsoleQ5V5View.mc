using Toybox.WatchUi;
using Toybox.Graphics as Gfx;
using Toybox.System;
using Toybox.Timer;
using Toybox.Attention;

// Marine Console Q5 V5 — view / presentation layer.
//
// One pair of values per screen, big and glanceable. UP/DOWN page through the
// pairs; MENU toggles the COLOR / SUN (mono high-contrast) theme. All values
// come from the shared DataModel; "---" means no data (no ESP32 link).
class MarineConsoleQ5V5View extends WatchUi.View {

    // display field ids
    const D_HDG = 0;
    const D_COG = 1;
    const D_SOG = 2;
    const D_DTW = 3;
    const D_XTE = 4;
    const D_BRG = 5;
    const D_AWA = 6;
    const D_AWS = 7;
    const D_TWA = 8;
    const D_TWS = 9;
    const D_GUST = 10;
    const D_DEPTH = 11;
    const D_TEMP = 12;
    const D_BATT = 13;

    const NUM_PAGES = 8;
    const WAKE_MS = 30000;   // keep the backlight on this long after interaction

    // Compass palette — vibrant variant of the NauticSense OCEAN rose
    // (neon cyan/green/red over a dark disc so the colours pop).
    const OC_DISC  = 0x081521;
    const OC_DISC2 = 0x0E2236;
    const OC_LINE  = 0x3A4F73;
    const OC_CYAN  = 0x00E0FF;
    const OC_WHITE = 0xFFFFFF;
    const OC_MUTED = 0xA8BAD0;
    const OC_GREEN = 0x37FF5A;
    const OC_RED   = 0xFF2E2E;

    var _timer;
    var _model;
    var _source;
    var _page = 0;
    var _sunMode = false;
    var _wakeUntil = 0;      // System.getTimer() value until which to hold the light
    var _lastLitMs = 0;
    var _tick = 0;
    var _reconnectUntil = 0; // show the RECONNECT confirmation until this time
    var _alarmAck = false;     // current alarm episode acknowledged (START)
    var _alarmVibrated = false; // already buzzed for this alarm (vibrate once)
    var _clearSinceMs = null;   // when the alarm condition went clear

    function initialize(model, source) {
        View.initialize();
        _model = model;
        _source = source;
    }

    function onShow() {
        _source.onStart();
        keepLit();
        _timer = new Timer.Timer();
        // 4 Hz: sample the HR channel fast enough to catch every held byte
        // (each is on-air ~2 s) and keep the link fresh. Redraw rides along.
        _timer.start(method(:onTick), 250, true);
    }

    // Hold the backlight on for WAKE_MS from now (called on show + interaction).
    function keepLit() {
        _wakeUntil = System.getTimer() + WAKE_MS;
        _lastLitMs = 0;   // force an immediate re-assert on the next tick
    }

    // Re-assert the backlight while inside the wake window. The display always
    // respects the device backlight timeout, so we refresh it every ~2 s.
    function serviceBacklight() {
        var now = System.getTimer();
        if (now < _wakeUntil && (now - _lastLitMs) >= 2000) {
            _lastLitMs = now;
            try {
                Attention.backlight(true);
            } catch (ex) {
                // BacklightOnTooLongException only on burn-in (AMOLED) devices.
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

        // Vibrate ONCE per alarm + light up. Re-arm only after the condition
        // has been clear for a while, so flickering data doesn't buzz again.
        var active = (_model.anchorAlarm || _model.shallowAlarm || _model.aisAlarm);
        if (active) {
            _clearSinceMs = null;
            if (!_alarmVibrated) {
                alarmVibrate();
                _alarmVibrated = true;
                keepLit();
            }
        } else {
            _alarmAck = false;   // overlay re-shows if it occurs again
            if (_clearSinceMs == null) {
                _clearSinceMs = System.getTimer();
            } else if (System.getTimer() - _clearSinceMs > 10000) {
                _alarmVibrated = false;   // genuinely clear for 10 s → re-arm
            }
        }

        _tick += 1;
        // Sample at 4 Hz, but redraw at ~2 Hz (the compass redraw is heavier).
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

    // START button: re-establish the link. Re-arms the sensor + resets decode,
    // and clears the model so the screen shows "---" until the stream resyncs.
    function reconnect() {
        _source.reconnect();
        _model.reset();
        _reconnectUntil = System.getTimer() + 1500;
        keepLit();
        WatchUi.requestUpdate();
    }

    // An unacknowledged alarm is currently active → show the overlay.
    function alarmShowing() {
        return (_model.anchorAlarm || _model.shallowAlarm || _model.aisAlarm) && !_alarmAck;
    }

    // START button dispatch: dismiss the alarm overlay if one is up, else
    // re-establish the link.
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

    // Full-screen red alarm overlay (drawn on top of the current page).
    function drawAlarmOverlay(dc) {
        dc.setColor(Gfx.COLOR_RED, Gfx.COLOR_RED);
        dc.fillRectangle(0, 0, dc.getWidth(), dc.getHeight());
        var l1; var l2; var detail = null;
        if (_model.anchorAlarm) {
            l1 = "ANCHOR"; l2 = "DRAG";
        } else if (_model.shallowAlarm) {
            l1 = "SHALLOW"; l2 = "WATER";
            detail = "DEPTH " + fmt1(_model.depthUnderKeel) + " m";
        } else {
            l1 = "AIS"; l2 = "TARGET";
            detail = fmt1(_model.aisNearDist) + " nm  " + fmtInt3(_model.aisNearBrg) + "°";
        }
        txt(dc, 120, 38,  Gfx.FONT_SMALL,  "! ALARM !", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, 120, 70,  Gfx.FONT_LARGE,  l1, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        txt(dc, 120, 110, Gfx.FONT_LARGE,  l2, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        if (detail != null) {
            txt(dc, 120, 152, Gfx.FONT_SMALL, detail, Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
        }
        txt(dc, 120, 196, Gfx.FONT_XTINY, "START = OK", Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
    }

    // ---- theme colours ----

    // Vibrant palette (neon on black); SUN mode stays mono for sunlight.
    function bgCol()     { return _sunMode ? Gfx.COLOR_WHITE : Gfx.COLOR_BLACK; }
    function fgCol()     { return _sunMode ? Gfx.COLOR_BLACK : 0xFFFFFF; }
    function labelCol()  { return _sunMode ? Gfx.COLOR_DK_GRAY : 0xB8C8D6; }
    function accentCol() { return _sunMode ? Gfx.COLOR_BLACK : 0x00E0FF; }   // bright cyan
    function goodCol()   { return _sunMode ? Gfx.COLOR_BLACK : 0x37FF5A; }   // neon green
    function infoCol()   { return _sunMode ? Gfx.COLOR_BLACK : 0x33B5FF; }   // vivid azure
    function warnCol()   { return 0xFF2E2E; }                                // vivid red
    function barCol()    { return _sunMode ? Gfx.COLOR_BLACK : 0x0A57E6; }   // electric blue

    function isOnline() { return _model.linkState.equals("ON"); }

    // ---- formatting helpers (null-safe, numeric-only output) ----

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

    function two(v) {
        if (v < 10) { return "0" + v.toString(); }
        return v.toString();
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
        dc.fillRectangle(0, 0, 240, 26);
        // Title only, lowered into the wider part of the round screen so the
        // longer names (TRUE WIND) aren't clipped. Page number → footer.
        txt(dc, 120, 9, Gfx.FONT_XTINY, pageTitle(), Gfx.TEXT_JUSTIFY_CENTER, Gfx.COLOR_WHITE);
    }

    // Footer = link status only (alarms are shown contextually on their pages,
    // e.g. the DEPTH value turns red when shallow).
    function drawFooter(dc) {
        var page = (_page + 1).toString() + "/" + NUM_PAGES.toString() + "  ";
        if (System.getTimer() < _reconnectUntil) {
            txt(dc, 120, 208, Gfx.FONT_XTINY, page + "RECONNECT...", Gfx.TEXT_JUSTIFY_CENTER, accentCol());
            return;
        }
        var online = isOnline();
        txt(dc, 120, 208, Gfx.FONT_XTINY, page + (online ? "LINK OK" : "NO LINK"),
            Gfx.TEXT_JUSTIFY_CENTER, online ? goodCol() : warnCol());
    }

    // One value block: big value centred, label/unit right-aligned in a column.
    function drawBlock(dc, yTop, yBot, label, value, unit, color) {
        var rx = 206;   // right column for the indications (inside the round edge)
        txt(dc, rx, yTop, Gfx.FONT_XTINY, label, Gfx.TEXT_JUSTIFY_RIGHT, labelCol());
        txt(dc, 120, yTop + 18, Gfx.FONT_NUMBER_MEDIUM, value, Gfx.TEXT_JUSTIFY_CENTER, color);
        txt(dc, rx, yBot - 16, Gfx.FONT_XTINY, unit, Gfx.TEXT_JUSTIFY_RIGHT, labelCol());
    }

    // Render the display field `id` into the block [yTop, yBot].
    function drawField(dc, id, yTop, yBot) {
        if (id == D_HDG) {
            drawBlock(dc, yTop, yBot, "HDG", fmtInt3(_model.headingTrue), "° TRUE", accentCol());
        } else if (id == D_COG) {
            drawBlock(dc, yTop, yBot, "COG", fmtInt3(_model.cog), "° course", fgCol());
        } else if (id == D_SOG) {
            drawBlock(dc, yTop, yBot, "SOG", fmt1(_model.sog), "knots", goodCol());
        } else if (id == D_DTW) {
            drawBlock(dc, yTop, yBot, "DTW", fmt1(_model.dtw), "nm to wpt", fgCol());
        } else if (id == D_XTE) {
            drawBlock(dc, yTop, yBot, "XTE", fmt2(_model.xte), "nm off", fgCol());
        } else if (id == D_BRG) {
            drawBlock(dc, yTop, yBot, "BRG", fmtInt3(_model.bearing), "° to wpt", infoCol());
        } else if (id == D_AWA) {
            drawBlock(dc, yTop, yBot, "AWA", intStr(_model.awa), "° apparent", fgCol());
        } else if (id == D_AWS) {
            drawBlock(dc, yTop, yBot, "AWS", fmt1(_model.aws), "knots", goodCol());
        } else if (id == D_TWA) {
            drawBlock(dc, yTop, yBot, "TWA", intStr(_model.twa), "° true", fgCol());
        } else if (id == D_TWS) {
            drawBlock(dc, yTop, yBot, "TWS", fmt1(_model.tws), "knots", goodCol());
        } else if (id == D_GUST) {
            drawBlock(dc, yTop, yBot, "GUST", fmt1(_model.gust), "knots", warnCol());
        } else if (id == D_DEPTH) {
            var dCol = (_model.depthUnderKeel != null && _model.depthUnderKeel < 18.0) ? warnCol() : accentCol();
            drawBlock(dc, yTop, yBot, "DEPTH", fmt1(_model.depthUnderKeel), "m under keel", dCol);
        } else if (id == D_TEMP) {
            drawBlock(dc, yTop, yBot, "WATER", fmt1(_model.waterTemp), "°C", fgCol());
        } else if (id == D_BATT) {
            drawBlock(dc, yTop, yBot, "BATTERY", intStr(_model.battery), "%", goodCol());
        }
    }

    // page -> the two fields it shows
    function pageFieldA() {
        if (_page == 3) { return D_SOG; }
        if (_page == 4) { return D_XTE; }
        if (_page == 5) { return D_DEPTH; }
        return D_GUST;   // page 6
    }

    function pageFieldB() {
        if (_page == 3) { return D_DTW; }
        if (_page == 4) { return D_BRG; }
        if (_page == 5) { return D_TEMP; }
        return D_BATT;   // page 6
    }

    // A cardinal/scale label placed on the rotating rose at the given bearing.
    function drawRoseLabel(dc, cx, cy, rr, bearing, hdg, label, color) {
        var a = (bearing - hdg - 90) * Math.PI / 180.0;
        var x = cx + (Math.cos(a) * rr);
        var y = cy + (Math.sin(a) * rr);
        txt(dc, x, y - 8, Gfx.FONT_XTINY, label, Gfx.TEXT_JUSTIFY_CENTER, color);
    }

    // Marine rose, OCEAN palette, ported from the NauticSense web compass.
    // Heading-up: the card rotates so the current heading sits under the fixed
    // red lubber line at the top.
    function drawOceanCompass(dc, cx, cy, r) {
        var heading = _model.headingTrue;
        var cog = _model.cog;
        var hdg = (heading == null) ? 0 : heading;

        // disc + outer ring
        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(cx, cy, r);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(2);
        dc.drawCircle(cx, cy, r);

        // port (red) / starboard (green) arcs at the top, ±70°
        dc.setPenWidth(5);
        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(cx, cy, r + 1, Gfx.ARC_COUNTER_CLOCKWISE, 90, 160);
        dc.setColor(OC_GREEN, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(cx, cy, r + 1, Gfx.ARC_CLOCKWISE, 90, 20);

        // graduated scale (every 5°), rotating with heading
        var i = 0;
        while (i < 360) {
            var a = (i - hdg - 90) * Math.PI / 180.0;
            var cs = Math.cos(a);
            var sn = Math.sin(a);
            var len; var w; var col;
            if (i % 30 == 0)      { len = 14; w = 3; col = OC_WHITE; }
            else if (i % 10 == 0) { len = 9;  w = 2; col = OC_MUTED; }
            else                  { len = 5;  w = 1; col = OC_LINE; }
            var r1 = r - 3;
            var r2 = r - 3 - len;
            dc.setColor(col, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(w);
            dc.drawLine(cx + (cs * r1), cy + (sn * r1), cx + (cs * r2), cy + (sn * r2));
            i += 5;
        }
        dc.setPenWidth(1);

        // cardinals (N red, rest white)
        drawRoseLabel(dc, cx, cy, r - 28, 0,   hdg, "N", OC_RED);
        drawRoseLabel(dc, cx, cy, r - 28, 90,  hdg, "E", OC_WHITE);
        drawRoseLabel(dc, cx, cy, r - 28, 180, hdg, "S", OC_WHITE);
        drawRoseLabel(dc, cx, cy, r - 28, 270, hdg, "W", OC_WHITE);

        // COG marker line (cyan) when known
        if (cog != null && heading != null) {
            var ca = (cog - hdg - 90) * Math.PI / 180.0;
            dc.setColor(OC_CYAN, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(2);
            dc.drawLine(cx, cy, cx + (Math.cos(ca) * (r * 0.82)), cy + (Math.sin(ca) * (r * 0.82)));
            dc.setPenWidth(1);
        }

        // inner lens for the centre readout
        var ir = (r * 0.5).toNumber();
        dc.setColor(OC_DISC2, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(cx, cy, ir);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawCircle(cx, cy, ir);

        // fixed red lubber line (triangle at top)
        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([[cx, cy - r + 7], [cx - 7, cy - r - 7], [cx + 7, cy - r - 7]]);

        // centre heading readout
        txt(dc, cx, cy - 26, Gfx.FONT_NUMBER_MEDIUM, fmtInt3(heading), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        txt(dc, cx, cy + 14, Gfx.FONT_XTINY, "° TRUE", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
        if (cog != null) {
            txt(dc, cx, cy + 28, Gfx.FONT_XTINY, "COG " + fmtInt3(cog), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        }
    }

    // A fixed (bow-up) label on the wind dial at the given relative angle.
    function windLabel(dc, cx, cy, rr, deg, label, color) {
        var a = (deg - 90) * Math.PI / 180.0;
        txt(dc, cx + (Math.cos(a) * rr), cy + (Math.sin(a) * rr) - 8,
            Gfx.FONT_XTINY, label, Gfx.TEXT_JUSTIFY_CENTER, color);
    }

    // Wind needle: a line from mid-radius out to the rim at `angleDeg` (0 = bow,
    // clockwise = starboard) with an arrowhead at the rim — points to where the
    // wind comes FROM. null angle draws nothing.
    function drawWindNeedle(dc, cx, cy, r, angleDeg, color, w) {
        if (angleDeg == null) { return; }
        var a = (angleDeg - 90) * Math.PI / 180.0;
        var cs = Math.cos(a);
        var sn = Math.sin(a);
        var rimX = cx + (cs * (r - 6));
        var rimY = cy + (sn * (r - 6));
        dc.setColor(color, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(w);
        dc.drawLine(cx + (cs * (r * 0.5)), cy + (sn * (r * 0.5)), rimX, rimY);
        dc.setPenWidth(1);
        var bx = cx + (cs * (r - 16));
        var by = cy + (sn * (r - 16));
        var px = -sn;
        var py = cs;
        dc.fillPolygon([[rimX, rimY], [bx + (px * 6), by + (py * 6)], [bx - (px * 6), by - (py * 6)]]);
    }

    // Wind dial — apparent (cyan) + true (green) on a bow-up rose with
    // port (red) / starboard (green) halves. Centre shows AWS / TWS.
    function drawWindDial(dc, cx, cy, r, apparent) {
        var awa = _model.awa;
        var twa = _model.twa;

        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(cx, cy, r);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.setPenWidth(2);
        dc.drawCircle(cx, cy, r);

        // port (left) red / starboard (right) green rim halves
        dc.setPenWidth(5);
        dc.setColor(OC_RED, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(cx, cy, r + 1, Gfx.ARC_COUNTER_CLOCKWISE, 90, 270);
        dc.setColor(OC_GREEN, Gfx.COLOR_TRANSPARENT);
        dc.drawArc(cx, cy, r + 1, Gfx.ARC_CLOCKWISE, 90, 270);

        // ticks every 15°
        var i = 0;
        while (i < 360) {
            var a = (i - 90) * Math.PI / 180.0;
            var cs = Math.cos(a);
            var sn = Math.sin(a);
            var len; var w;
            if (i % 90 == 0)      { len = 13; w = 3; }
            else if (i % 30 == 0) { len = 9;  w = 2; }
            else                  { len = 5;  w = 1; }
            dc.setColor(OC_MUTED, Gfx.COLOR_TRANSPARENT);
            dc.setPenWidth(w);
            dc.drawLine(cx + (cs * (r - 3)), cy + (sn * (r - 3)), cx + (cs * (r - 3 - len)), cy + (sn * (r - 3 - len)));
            i += 15;
        }
        dc.setPenWidth(1);

        windLabel(dc, cx, cy, r - 22, 0,   "0",   OC_WHITE);
        windLabel(dc, cx, cy, r - 22, 90,  "90",  OC_GREEN);   // starboard beam
        windLabel(dc, cx, cy, r - 22, 180, "180", OC_WHITE);
        windLabel(dc, cx, cy, r - 22, 270, "90",  OC_RED);     // port beam

        // bow marker (fixed white triangle at top)
        dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([[cx, cy - r + 6], [cx - 6, cy - r - 6], [cx + 6, cy - r - 6]]);

        // both needles for context; the page's own wind is the thick one.
        if (apparent) {
            drawWindNeedle(dc, cx, cy, r, twa, OC_GREEN, 2);
            drawWindNeedle(dc, cx, cy, r, awa, OC_CYAN, 4);
        } else {
            drawWindNeedle(dc, cx, cy, r, awa, OC_CYAN, 2);
            drawWindNeedle(dc, cx, cy, r, twa, OC_GREEN, 4);
        }

        // inner lens + centre readout
        var ir = (r * 0.46).toNumber();
        dc.setColor(OC_DISC2, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(cx, cy, ir);
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawCircle(cx, cy, ir);

        if (apparent) {
            txt(dc, cx, cy - 30, Gfx.FONT_NUMBER_MEDIUM, fmt1(_model.aws), Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
            txt(dc, cx, cy + 16, Gfx.FONT_XTINY, "kt  ·  AWA " + intStr(_model.awa) + "°", Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
        } else {
            txt(dc, cx, cy - 30, Gfx.FONT_NUMBER_MEDIUM, fmt1(_model.tws), Gfx.TEXT_JUSTIFY_CENTER, OC_GREEN);
            txt(dc, cx, cy + 16, Gfx.FONT_XTINY, "kt  ·  TWA " + intStr(_model.twa) + "°", Gfx.TEXT_JUSTIFY_CENTER, OC_GREEN);
        }
    }

    // Filled triangle centred at (x,y), apex toward dirDeg (0 = up/bow).
    function drawAisTriangle(dc, x, y, dirDeg, color) {
        var th = dirDeg * Math.PI / 180.0;
        var dx = Math.sin(th);    var dy = -Math.cos(th);   // forward
        var px = Math.cos(th);    var py = Math.sin(th);    // right
        var L = 7.0;
        var B = 5.0;
        dc.setColor(color, Gfx.COLOR_TRANSPARENT);
        dc.fillPolygon([
            [x + (dx * L), y + (dy * L)],
            [x - (dx * L * 0.7) + (px * B), y - (dy * L * 0.7) + (py * B)],
            [x - (dx * L * 0.7) - (px * B), y - (dy * L * 0.7) - (py * B)]
        ]);
    }

    // AIS plot — own ship centred, head-up, range rings 2/5/10 NM. Triangles
    // (course-oriented) for targets within 5 NM, dots beyond. No labels.
    //
    // NOTE: demo targets — the HR link can't carry an AIS target list, so these
    // are synthesised here just to show the standard layout. Real AIS needs a
    // richer transport (NMEA/AIS feed).
    function drawAisPlot(dc, cx, cy, R) {
        var hdg = (_model.headingTrue == null) ? 0 : _model.headingTrue;

        dc.setColor(OC_DISC, Gfx.COLOR_TRANSPARENT);
        dc.fillCircle(cx, cy, R);

        // range rings + labels (2 / 5 / 10 NM)
        dc.setPenWidth(1);
        var ringNm = [2, 5, 10];
        var k = 0;
        while (k < 3) {
            var rr = R * (ringNm[k] / 10.0);
            dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
            dc.drawCircle(cx, cy, rr);
            txt(dc, cx - (rr * 0.707), cy - (rr * 0.707) - 7, Gfx.FONT_XTINY,
                ringNm[k].toString(), Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
            k += 1;
        }
        txt(dc, cx, cy + R - 14, Gfx.FONT_XTINY, "NM", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);

        // heading line (bow up)
        dc.setColor(OC_LINE, Gfx.COLOR_TRANSPARENT);
        dc.drawLine(cx, cy, cx, cy - R);

        var scale = R / 10.0;
        if (isOnline()) {
            // REAL: the HR link carries only the nearest target.
            if (_model.aisNearDist != null && _model.aisNearBrg != null && _model.aisNearDist <= 10) {
                var a = (_model.aisNearBrg - hdg - 90) * Math.PI / 180.0;
                var rad = _model.aisNearDist * scale;
                var tx = cx + (Math.cos(a) * rad);
                var ty = cy + (Math.sin(a) * rad);
                if (_model.aisNearDist <= 5.0) {
                    drawAisTriangle(dc, tx, ty, 0, OC_CYAN);
                } else {
                    dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT);
                    dc.fillCircle(tx, ty, 3);
                }
                txt(dc, cx, cy + 38, Gfx.FONT_XTINY,
                    "NEAR " + fmt1(_model.aisNearDist) + "nm", Gfx.TEXT_JUSTIFY_CENTER, OC_CYAN);
            } else {
                txt(dc, cx, cy + 38, Gfx.FONT_XTINY, "NO AIS", Gfx.TEXT_JUSTIFY_CENTER, OC_MUTED);
            }
        } else {
            // OFFLINE: demo targets just to show the standard layout.
            var tg = [[30,1.5,210],[75,3.0,290],[140,4.5,10],[200,7.0,90],[290,9.0,180],[330,6.0,45]];
            var j = 0;
            while (j < 6) {
                var brg = tg[j][0]; var dist = tg[j][1]; var crs = tg[j][2];
                var a = (brg - hdg - 90) * Math.PI / 180.0;
                var rad = dist * scale;
                var tx = cx + (Math.cos(a) * rad);
                var ty = cy + (Math.sin(a) * rad);
                if (dist <= 5.0) {
                    drawAisTriangle(dc, tx, ty, crs - hdg, OC_CYAN);
                } else {
                    dc.setColor(OC_WHITE, Gfx.COLOR_TRANSPARENT);
                    dc.fillCircle(tx, ty, 3);
                }
                j += 1;
            }
        }

        // own ship (green triangle, bow up) at centre
        drawAisTriangle(dc, cx, cy, 0, OC_GREEN);
    }

    function onUpdate(dc) {
        dc.setColor(fgCol(), bgCol());
        dc.clear();
        drawHeader(dc);
        if (_page == 0) {
            drawOceanCompass(dc, 120, 124, 90);
        } else if (_page == 1) {
            drawWindDial(dc, 120, 124, 88, true);
        } else if (_page == 2) {
            drawWindDial(dc, 120, 124, 88, false);
        } else if (_page == 7) {
            drawAisPlot(dc, 120, 124, 90);
        } else {
            drawField(dc, pageFieldA(), 32, 116);
            dc.setColor(labelCol(), Gfx.COLOR_TRANSPARENT);
            dc.drawLine(34, 120, 206, 120);
            drawField(dc, pageFieldB(), 124, 200);
        }
        drawFooter(dc);
        if (alarmShowing()) {
            drawAlarmOverlay(dc);
        }
    }
}
