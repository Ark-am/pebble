import assert from "node:assert/strict";
import test from "node:test";

import { calculateSunPosition } from "../src/embeddedjs/sun.js";

function assertNear(actual, expected, tolerance, label) {
  assert.ok(
    Math.abs(actual - expected) <= tolerance,
    `${label}: expected ${expected}, got ${actual.toFixed(4)}`
  );
}

// The worked example in NREL's Solar Position Algorithm report (Reda and
// Andreas, 2004): Golden, Colorado, 17 October 2003, 12:30:30 local time
// (UTC-7). It gives an azimuth of 194.34024° and a zenith of 50.11162°. The
// zenith includes 0.016° of refraction, which this module leaves out.
test("matches NREL's Solar Position Algorithm example", () => {
  const sun = calculateSunPosition(
    new Date(Date.UTC(2003, 9, 17, 19, 30, 30)),
    39.742476,
    -105.1786
  );
  assertNear(sun.azimuth, 194.34024, 0.05, "azimuth");
  assertNear(sun.elevation, 90 - 50.11162 - 0.016, 0.05, "elevation");
});

test("is due south and high at Greenwich's solar noon in June", () => {
  // The equation of time puts solar noon at about 12:02 UTC on 21 June.
  const sun = calculateSunPosition(new Date(Date.UTC(2026, 5, 21, 12, 2)), 51.4769, 0);
  assertNear(sun.azimuth, 180, 1, "azimuth");
  // 90° - latitude + the solstice declination of 23.44°.
  assertNear(sun.elevation, 61.96, 0.2, "elevation");
});

test("is below the horizon at night and rises in the east", () => {
  const midnight = calculateSunPosition(new Date(Date.UTC(2026, 10, 9, 0, 0)), 51.4769, 0);
  assert.ok(midnight.elevation < -20, `elevation ${midnight.elevation}`);

  const morning = calculateSunPosition(new Date(Date.UTC(2026, 2, 20, 6, 30)), 0, 0);
  assert.ok(morning.elevation > 0 && morning.elevation < 15, `elevation ${morning.elevation}`);
  assertNear(morning.azimuth, 90, 2, "azimuth");
});

test("is due north at noon in the southern hemisphere's winter", () => {
  const sun = calculateSunPosition(new Date(Date.UTC(2026, 5, 21, 2, 0)), -33.8688, 151.2093);
  assert.ok(sun.azimuth < 10 || sun.azimuth > 350, `azimuth ${sun.azimuth}`);
});
