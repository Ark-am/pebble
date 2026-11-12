// Run with: node --test test/

import assert from "node:assert/strict";
import { createRequire } from "node:module";
import test from "node:test";

const require = createRequire(import.meta.url);
const { Tracker, distance, bearing } = require("../src/pkjs/tracker.js");

function assertNear(actual, expected, tolerance, label) {
  assert.ok(
    Math.abs(actual - expected) <= tolerance,
    `${label}: expected ${expected}, got ${actual}`
  );
}

// Metres to degrees, near enough for short test journeys.
const METRES_PER_DEGREE = 111195;
const START = { latitude: 23.78, longitude: 90.4 };

function north(metres) {
  return { latitude: START.latitude + metres / METRES_PER_DEGREE, longitude: START.longitude };
}

function east(metres) {
  const scale = METRES_PER_DEGREE * Math.cos((START.latitude * Math.PI) / 180);
  return { latitude: START.latitude, longitude: START.longitude + metres / scale };
}

test("distance and bearing agree with known values", () => {
  // One degree of latitude is about 111.2 km.
  assertNear(distance({ latitude: 0, longitude: 0 }, { latitude: 1, longitude: 0 }), 111195, 10, "1° north");
  assertNear(bearing(START, north(100)), 0, 0.01, "north");
  assertNear(bearing(START, east(100)), 90, 0.01, "east");
  assertNear(bearing(north(100), START), 180, 0.01, "south");
  assertNear(bearing(east(100), START), 270, 0.01, "west");
});

test("uses the phone's own speed and heading, and adds up the distance", () => {
  const tracker = new Tracker();
  let reading;
  // 20 m/s (72 km/h) heading east, one fix a second for a minute.
  for (let second = 0; second <= 60; second++) {
    reading = tracker.update(
      { ...east(20 * second), accuracy: 5, speed: 20, heading: 90 },
      second * 1000
    );
  }
  assert.equal(reading.usable, true);
  assert.equal(reading.speed, 20);
  assert.equal(reading.heading, 90);
  assert.equal(reading.accuracy, 5);
  assertNear(reading.odometer, 1200, 0.001, "odometer");
});

test("standing still adds nothing, however long", () => {
  const tracker = new Tracker();
  let reading;
  for (let second = 0; second <= 600; second++) {
    // A parked phone still reports a little speed and a wandering position.
    const wobble = north(Math.sin(second) * 4);
    reading = tracker.update({ ...wobble, accuracy: 8, speed: 0.3, heading: 200 }, second * 1000);
  }
  assert.equal(reading.speed, 0);
  assert.equal(reading.odometer, 0);
  assert.equal(reading.heading, null);
});

test("works the speed out from positions when the phone gives none", () => {
  const tracker = new Tracker();
  let reading;
  // 10 m/s north, a fix every two seconds, with no speed or heading.
  for (let step = 0; step <= 30; step++) {
    reading = tracker.update({ ...north(20 * step), accuracy: 10 }, step * 2000);
  }
  assertNear(reading.speed, 10, 0.05, "speed");
  assert.equal(reading.heading, 0);
  assertNear(reading.odometer, 600, 1, "odometer");
});

test("positions that only wobble are not movement", () => {
  const tracker = new Tracker();
  let reading;
  for (let step = 0; step <= 100; step++) {
    const wobble = east(Math.cos(step * 1.7) * 2);
    reading = tracker.update({ ...wobble, accuracy: 10 }, step * 1000);
  }
  assert.equal(reading.speed, 0);
  assert.equal(reading.odometer, 0);
});

test("a fix that is too vague is not used", () => {
  const tracker = new Tracker();
  const reading = tracker.update({ ...START, accuracy: 500, speed: 3 }, 0);
  assert.equal(reading.usable, false);
});

test("iOS's -1 for an unknown speed and heading is not taken as a value", () => {
  const tracker = new Tracker();
  tracker.update({ ...START, accuracy: 5, speed: -1, heading: -1 }, 0);
  const reading = tracker.update({ ...north(30), accuracy: 5, speed: -1, heading: -1 }, 2000);
  assertNear(reading.speed, 15, 0.05, "speed");
  assert.equal(reading.heading, 0);
});

test("a gap such as a tunnel is bridged with a straight line", () => {
  const tracker = new Tracker();
  tracker.update({ ...START, accuracy: 5, speed: 25, heading: 0 }, 0);
  tracker.update({ ...north(25), accuracy: 5, speed: 25, heading: 0 }, 1000);
  // No fixes for 40 seconds, then out the other end a kilometre on.
  const reading = tracker.update({ ...north(1025), accuracy: 5, speed: 25, heading: 0 }, 41000);
  assertNear(reading.odometer, 1025, 2, "odometer");
});

test("a GPS jump after a gap does not inflate the trip", () => {
  const tracker = new Tracker();
  tracker.update({ ...START, accuracy: 5, speed: 25, heading: 0 }, 0);
  const reading = tracker.update(
    { ...north(100000), accuracy: 5, speed: 25, heading: 0 },
    40000
  );
  assert.equal(reading.odometer, 0);
});

test("the heading is held briefly after stopping, then dropped", () => {
  const tracker = new Tracker();
  tracker.update({ ...START, accuracy: 5, speed: 12, heading: 315 }, 0);
  let reading = tracker.update({ ...START, accuracy: 5, speed: 0, heading: 0 }, 3000);
  assert.equal(reading.heading, 315);
  reading = tracker.update({ ...START, accuracy: 5, speed: 0, heading: 0 }, 9000);
  assert.equal(reading.heading, null);
});

test("an impossible speed or a jumping position is left out", () => {
  const tracker = new Tracker();
  assert.equal(tracker.update({ ...START, accuracy: 5, speed: 5000 }, 0), null);

  const byPosition = new Tracker();
  byPosition.update({ ...START, accuracy: 10 }, 0);
  assert.equal(byPosition.update({ ...north(50000), accuracy: 10 }, 1000), null);
  // It carries on from the new place afterwards.
  const reading = byPosition.update({ ...north(50020), accuracy: 10 }, 3000);
  assertNear(reading.speed, 10, 0.05, "speed after the jump");
  assertNear(reading.odometer, 20, 0.5, "odometer after the jump");
});

test("missing or broken coordinates are ignored", () => {
  const tracker = new Tracker();
  assert.equal(tracker.update(null, 0), null);
  assert.equal(tracker.update({ latitude: NaN, longitude: 90 }, 0), null);
});
