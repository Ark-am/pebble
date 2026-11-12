// Turns the phone's GPS fixes into what the watch shows: speed, heading and
// the distance travelled.
//
// Most phones report their speed and direction with each fix, measured from
// the satellites' signals, and that is used when it is there. When it is not,
// both are worked out from how far the position has moved.
//
// Nothing here touches the phone or the watch, so test/tracker.test.mjs can
// run it under Node.

var EARTH_RADIUS = 6371000;   // metres

// A fix less certain than this many metres is too vague to measure speed with.
var MAX_ACCURACY = 60;
// Slower than this (metres a second, about 1.8 km/h) counts as standing
// still. GPS never reports exactly zero, and without this the trip would
// creep upwards while you wait at a light.
var STILL_SPEED = 0.5;
// Faster than this (metres a second, about 1,400 km/h) is a GPS glitch.
var MAX_SPEED = 400;
// Fixes further apart than this are a gap, such as a tunnel.
var GAP_MS = 3000;
// A gap longer than this is a new journey; nothing is added for it.
var LONG_GAP_MS = 10 * 60 * 1000;
// Without a speed from the phone, positions this far back are compared.
var WINDOW_MS = 4000;
// The heading is only trusted while moving at least this fast (m/s)...
var HEADING_SPEED = 1;
// ...and is kept for this long after stopping, so it does not flicker.
var HEADING_HOLD_MS = 8000;

function isNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

function toRadians(degrees) {
  return degrees * Math.PI / 180;
}

// Metres between two points, along the Earth's surface.
function distance(a, b) {
  var lat1 = toRadians(a.latitude);
  var lat2 = toRadians(b.latitude);
  var dLat = lat2 - lat1;
  var dLon = toRadians(b.longitude - a.longitude);
  var h = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
          Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLon / 2) * Math.sin(dLon / 2);
  return 2 * EARTH_RADIUS * Math.asin(Math.min(1, Math.sqrt(h)));
}

// The compass direction from a to b, in degrees clockwise from north.
function bearing(a, b) {
  var lat1 = toRadians(a.latitude);
  var lat2 = toRadians(b.latitude);
  var dLon = toRadians(b.longitude - a.longitude);
  var y = Math.sin(dLon) * Math.cos(lat2);
  var x = Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(dLon);
  return (Math.atan2(y, x) * 180 / Math.PI + 360) % 360;
}

function Tracker() {
  this.odometer = 0;       // metres travelled since the tracker was made
  this.last = null;        // the last fix that came with a speed
  this.history = [];       // recent positions, for fixes without a speed
  this.anchor = null;      // where distance was last measured from, likewise
  this.heading = null;
  this.headingTime = 0;
}

// Takes the `coords` of a fix and the time it arrived, in milliseconds.
// Returns null for a fix to ignore, { usable: false } for one too vague to
// measure with, and otherwise:
//   speed      metres a second, 0 when standing still
//   heading    degrees clockwise from north, or null when not known
//   accuracy   metres, or null when not known
//   odometer   metres travelled so far
Tracker.prototype.update = function (coords, now) {
  if (!coords || !isNumber(coords.latitude) || !isNumber(coords.longitude)) {
    return null;
  }
  var accuracy = isNumber(coords.accuracy) && coords.accuracy >= 0 ? coords.accuracy : null;
  if (accuracy !== null && accuracy > MAX_ACCURACY) {
    return { usable: false, accuracy: accuracy };
  }

  var point = { latitude: coords.latitude, longitude: coords.longitude, time: now };
  // iOS reports -1 for a speed or heading it does not know.
  var hasSpeed = isNumber(coords.speed) && coords.speed >= 0;
  var result = hasSpeed ? this.fromSpeed(point, coords, accuracy)
                        : this.fromPositions(point, accuracy);
  if (!result) {
    return null;
  }

  if (result.heading !== null) {
    this.heading = Math.round(result.heading) % 360;
    this.headingTime = now;
  } else if (now - this.headingTime > HEADING_HOLD_MS) {
    this.heading = null;
  }

  return {
    usable: true,
    speed: result.speed,
    heading: this.heading,
    accuracy: accuracy,
    odometer: this.odometer
  };
};

// The phone gave the speed. Distance is that speed added up over time, which
// is steadier than measuring between positions that each wobble a little.
Tracker.prototype.fromSpeed = function (point, coords, accuracy) {
  if (coords.speed > MAX_SPEED) {
    return null;
  }
  var speed = coords.speed < STILL_SPEED ? 0 : coords.speed;
  point.speed = speed;

  if (this.last) {
    var elapsed = point.time - this.last.time;
    if (elapsed > 0 && elapsed <= GAP_MS) {
      this.odometer += (this.last.speed + speed) / 2 * (elapsed / 1000);
    } else if (elapsed > GAP_MS && elapsed < LONG_GAP_MS) {
      // After a gap, the straight line across it is the best guess, as long
      // as it is clearly more than the fixes' own uncertainty.
      var across = distance(this.last, point);
      // A stale or newly acquired fix can land far from the real route.
      // Never bridge a gap faster than the maximum plausible GPS speed.
      if (across >= Math.max(10, 2 * (accuracy || 0)) &&
          across <= MAX_SPEED * elapsed / 1000) {
        this.odometer += across;
      }
    }
  }
  this.last = point;
  // Start afresh if the phone later stops giving speeds.
  this.history = [];
  this.anchor = null;

  var heading = null;
  if (speed >= HEADING_SPEED && isNumber(coords.heading) && coords.heading >= 0) {
    heading = coords.heading;
  }
  return { speed: speed, heading: heading };
};

// The phone gave only a position. Speed is how far the position has moved
// over the last few seconds.
Tracker.prototype.fromPositions = function (point, accuracy) {
  this.last = null;
  // Movement smaller than this could just be the position wobbling.
  var wobble = Math.max(3, (accuracy === null ? 10 : accuracy) / 2);

  var history = this.history;
  history.push(point);
  while (history.length > 2 && point.time - history[0].time > WINDOW_MS) {
    history.shift();
  }
  var oldest = history[0];
  var elapsed = point.time - oldest.time;
  if (history.length > 1 && elapsed > LONG_GAP_MS) {
    // Too long ago to compare with; start again from here.
    this.history = [point];
    this.anchor = point;
    return { speed: 0, heading: null };
  }

  var speed = 0;
  var heading = null;
  if (history.length > 1 && elapsed >= 500) {
    var moved = distance(oldest, point);
    if (moved / (elapsed / 1000) > MAX_SPEED) {
      // The position jumped. Leave this fix out, and measure from here on.
      this.history = [point];
      this.anchor = point;
      return null;
    }
    if (moved >= wobble) {
      speed = moved / (elapsed / 1000);
      if (speed < STILL_SPEED) {
        speed = 0;
      } else {
        heading = bearing(oldest, point);
      }
    }
  }

  // Distance is counted in steps, from one anchor point to the next, each
  // far enough apart to be real movement.
  if (!this.anchor) {
    this.anchor = point;
  } else {
    var step = distance(this.anchor, point);
    if (step >= wobble) {
      this.odometer += step;
      this.anchor = point;
    }
  }
  return { speed: speed, heading: heading };
};

module.exports = {
  Tracker: Tracker,
  distance: distance,
  bearing: bearing,
  MAX_ACCURACY: MAX_ACCURACY,
  STILL_SPEED: STILL_SPEED
};
