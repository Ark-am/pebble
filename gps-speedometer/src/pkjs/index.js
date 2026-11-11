// The phone's side of GPS Speedometer. It follows the phone's GPS and sends
// the watch its speed, heading, accuracy and distance travelled, about once
// a second, for as long as the watch app is open.

var Tracker = require('./tracker').Tracker;

// What the STATUS key tells the watch (LinkStatus in src/c/main.c).
var STATUS_FIX = 0;         // this message carries a fix
var STATUS_SEARCHING = 1;   // waiting for a usable fix
var STATUS_DENIED = 2;      // the Pebble app may not use the phone's location
var STATUS_ERROR = 3;       // the phone could not give a location

// `frequency` is the Pebble app's own option: milliseconds between fixes.
var WATCH_OPTIONS = { enableHighAccuracy: true, maximumAge: 0, timeout: 15000, frequency: 1000 };
var POLL_OPTIONS = { enableHighAccuracy: true, maximumAge: 0, timeout: 5000 };

var SEND_EVERY_MS = 900;      // the watch is sent a reading no more often than this
var QUIET_MS = 4000;          // no fixes for this long: ask for one directly
var POLL_EVERY_MS = 2000;     // and keep asking this often until they return
var LOST_MS = 6000;           // no fixes for this long: tell the watch
var RETRY_MS = 10000;         // after an error, start following again
var DENIED_RETRY_MS = 30000;  // but ask less often once location is refused
var SEND_TIMEOUT_MS = 3000;   // a message not answered by now is given up on

var tracker = new Tracker();
var watchId = null;
var startedAt = 0;        // when following began
var lastFixAt = 0;        // when any fix last arrived
var lastWatchFixAt = 0;   // when a fix last arrived by following
var lastPollAt = 0;
var polling = false;
var lastPolled = null;    // the last fix got by asking, to spot repeats
var errorAt = 0;          // when following last failed, or 0
var denied = false;       // the last failure was a refusal
var lastStatus = -1;

var pending = null;       // the newest message not yet sent
var sendingSince = 0;     // when the message in flight was sent, or 0
var lastSentAt = 0;

function flush() {
  var now = Date.now();
  if (!pending) {
    return;
  }
  if (sendingSince && now - sendingSince < SEND_TIMEOUT_MS) {
    return;   // wait for the one in flight; the newest will follow it
  }
  var message = pending;
  pending = null;
  sendingSince = now;
  lastSentAt = now;
  var done = function () {
    sendingSince = 0;
    flush();
  };
  Pebble.sendAppMessage(message, done, done);
}

function send(message) {
  lastStatus = message.STATUS;
  pending = message;
  flush();
}

function sendStatus(status) {
  if (status !== lastStatus) {
    send({ STATUS: status });
  }
}

// Whole numbers for the watch: centimetres a second, degrees, metres, and
// decimetres travelled.
function readingMessage(reading) {
  return {
    STATUS: STATUS_FIX,
    SPEED: Math.round(reading.speed * 100),
    HEADING: reading.heading === null ? -1 : reading.heading,
    ACCURACY: reading.accuracy === null ? -1 : Math.min(9999, Math.round(reading.accuracy)),
    ODOMETER: Math.round(reading.odometer * 10) % 2147483647
  };
}

function handleFix(position) {
  if (!position || !position.coords) {
    return;
  }
  var now = Date.now();
  // The Pebble app's fixes carry no time of their own, so use the time now.
  var time = typeof position.timestamp === 'number' ? position.timestamp : now;
  var reading = tracker.update(position.coords, time);
  if (!reading) {
    return;
  }
  errorAt = 0;
  denied = false;
  if (!reading.usable) {
    sendStatus(STATUS_SEARCHING);
    return;
  }
  lastFixAt = now;
  var message = readingMessage(reading);
  if (now - lastSentAt >= SEND_EVERY_MS) {
    send(message);
  } else {
    // Too soon after the last one; tick() sends it.
    lastStatus = STATUS_FIX;
    pending = message;
  }
}

function onWatchFix(position) {
  lastWatchFixAt = Date.now();
  handleFix(position);
}

// A fix that was asked for. Some phones answer with the last fix they had
// when there is no signal, so an exact repeat is not taken as a new one.
function onPolledFix(position) {
  polling = false;
  var coords = position && position.coords;
  if (!coords) {
    return;
  }
  var same = lastPolled &&
      lastPolled.latitude === coords.latitude &&
      lastPolled.longitude === coords.longitude &&
      lastPolled.accuracy === coords.accuracy &&
      lastPolled.timestamp === position.timestamp;
  lastPolled = {
    latitude: coords.latitude,
    longitude: coords.longitude,
    accuracy: coords.accuracy,
    timestamp: position.timestamp
  };
  if (!same) {
    handleFix(position);
  }
}

function onError(error) {
  polling = false;
  var text = error && error.message ? String(error.message) : '';
  // The Pebble app gives every error code 1, so the wording is checked too.
  denied = /permission|denied|not granted/i.test(text) ||
           Boolean(error && error.code === 1 && !text);
  console.log('Location error: ' + (text || (error && error.code) || 'unknown'));
  if (!errorAt) {
    errorAt = Date.now();
  }
  sendStatus(denied ? STATUS_DENIED : STATUS_ERROR);
}

function follow() {
  if (watchId !== null) {
    navigator.geolocation.clearWatch(watchId);
  }
  startedAt = Date.now();
  errorAt = 0;
  watchId = navigator.geolocation.watchPosition(onWatchFix, onError, WATCH_OPTIONS);
}

// Runs every second: sends a reading that was held back, and keeps the fixes
// coming when following has gone quiet or failed.
function tick() {
  var now = Date.now();
  if (pending && now - lastSentAt >= SEND_EVERY_MS) {
    flush();
  }
  if (errorAt && now - errorAt >= (denied ? DENIED_RETRY_MS : RETRY_MS)) {
    follow();
  }
  if (lastStatus === STATUS_FIX && now - lastFixAt >= LOST_MS) {
    sendStatus(STATUS_SEARCHING);
  }
  // Asking again while location is refused would only nag the phone's owner.
  var quietSince = Math.max(lastWatchFixAt, startedAt);
  if (!denied && now - quietSince >= QUIET_MS && !polling &&
      now - lastPollAt >= POLL_EVERY_MS) {
    polling = true;
    lastPollAt = now;
    navigator.geolocation.getCurrentPosition(onPolledFix, onError, POLL_OPTIONS);
  }
  // An answer that never comes must not stop the asking.
  if (polling && now - lastPollAt >= 3 * POLL_EVERY_MS) {
    polling = false;
  }
}

Pebble.addEventListener('ready', function () {
  sendStatus(STATUS_SEARCHING);
  follow();
  setInterval(tick, 1000);
});
