using Toybox.System;

// Central marine data model for Marine Console Q5 V5.
//
// All instrument values live here, in one place, decoupled from rendering
// (the View) and from whatever feeds them (a DataSource). Fields are grouped
// NMEA-style so a real source can map sentences straight in:
//
//   HDT / HDG   -> headingTrue
//   VTG / RMC   -> cog, sog
//   MWV (R)     -> awa, aws        MWV (T) -> twa, tws
//   DPT / DBT   -> depthUnderKeel
//   MTW         -> waterTemp
//   XTE         -> xte
//   BWC / BWR   -> bearing, dtw, waypoint
//
// Convention: a field value of `null` means "no valid data". The View
// renders null as "---" / "--.-" so a missing feed is obvious on screen.
class DataModel {

    // ---- NAV ----
    var headingTrue;    // deg [0..359]
    var cog;            // deg [0..359] course over ground
    var sog;            // knots, speed over ground
    var xte;            // nautical miles, cross-track error
    var bearing;        // deg [0..359] bearing to active waypoint
    var dtw;            // nautical miles, distance to waypoint
    var waypoint;       // string, active waypoint id

    // ---- WIND ----
    var awa;            // deg, apparent wind angle
    var aws;            // knots, apparent wind speed
    var twa;            // deg, true wind angle
    var tws;            // knots, true wind speed
    var gust;           // knots

    // ---- DEPTH / ENV ----
    var depthUnderKeel; // metres
    var waterTemp;      // deg C
    var battery;        // percent [0..100]
    var anchorAlarm;    // bool
    var shallowAlarm;   // bool

    // ---- AIS (nearest target only — the HR link can't carry a full list) ----
    var aisNearBrg;     // deg true, bearing to nearest target (null = none)
    var aisNearDist;    // nautical miles to nearest target
    var aisAlarm;       // bool, a target is inside the guard ring

    // ---- STATUS ----
    var linkState;      // "ON" when the ESP32 feed is fresh, else "OFF"
    var lastUpdateMs;   // System.getTimer() at last touch(), or null

    function initialize() {
        reset();
    }

    // Clear everything to the "no data" state.
    function reset() {
        headingTrue = null;
        cog = null;
        sog = null;
        xte = null;
        bearing = null;
        dtw = null;
        waypoint = "---";

        awa = null;
        aws = null;
        twa = null;
        tws = null;
        gust = null;

        depthUnderKeel = null;
        waterTemp = null;
        battery = null;
        anchorAlarm = false;
        shallowAlarm = false;

        aisNearBrg = null;
        aisNearDist = null;
        aisAlarm = false;

        linkState = "OFF";
        lastUpdateMs = null;
    }

    // A source calls this after writing a batch of fields, so the UI can
    // reason about freshness (stale-link detection arrives in the link phase).
    function touch() {
        lastUpdateMs = System.getTimer();
    }

    // Age of the model in ms since the last touch(), or null if never fed.
    function ageMs() {
        if (lastUpdateMs == null) {
            return null;
        }
        return System.getTimer() - lastUpdateMs;
    }
}
