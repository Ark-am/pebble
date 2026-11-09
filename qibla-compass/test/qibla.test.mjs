import assert from "node:assert/strict";
import test from "node:test";

import {
  calculateQiblaBearing,
  calculateQiblaDistance,
  compassPoint,
  normalizeDegrees,
  normalizeSignedDegrees
} from "../src/embeddedjs/qibla.js";

const KAABA_LONGITUDE = 39.826206;

function assertNear(actual, expected, tolerance) {
  assert.ok(
    Math.abs(normalizeSignedDegrees(actual - expected)) <= tolerance,
    `expected ${expected}, got ${actual.toFixed(4)}`
  );
}

test("points due south or north along the Kaaba's meridian", () => {
  assertNear(calculateQiblaBearing(60, KAABA_LONGITUDE), 180, 1e-9);
  assertNear(calculateQiblaBearing(-30, KAABA_LONGITUDE), 0, 1e-9);
});

test("matches published great-circle Qibla bearings", () => {
  assertNear(calculateQiblaBearing(40.7128, -74.006), 58.48, 0.1);
  assertNear(calculateQiblaBearing(51.5074, -0.1278), 118.99, 0.1);
  assertNear(calculateQiblaBearing(-6.2088, 106.8456), 295.15, 0.1);
});

test("normalizes headings", () => {
  assert.equal(normalizeDegrees(-10), 350);
  assert.equal(normalizeDegrees(370), 10);
  assert.equal(normalizeSignedDegrees(190), -170);
  assert.equal(normalizeSignedDegrees(-190), 170);
});

test("measures the distance to the Kaaba", () => {
  assert.equal(calculateQiblaDistance(21.422487, KAABA_LONGITUDE), 0);
  // One degree of latitude along the Kaaba's meridian.
  const oneDegree = calculateQiblaDistance(22.422487, KAABA_LONGITUDE);
  assert.ok(Math.abs(oneDegree - 111.195) < 0.01, `got ${oneDegree}`);
});

test("names the nearest compass point", () => {
  assert.deepEqual(compassPoint(0), { abbreviation: "N", name: "north" });
  assert.deepEqual(compassPoint(58.48), { abbreviation: "ENE", name: "east-northeast" });
  assert.equal(compassPoint(277.6).name, "west");
  assert.equal(compassPoint(295.15).name, "west-northwest");
  assert.equal(compassPoint(354).abbreviation, "N");
  assert.equal(compassPoint(348.74).abbreviation, "NNW");
  assert.equal(compassPoint(-10).abbreviation, "N");
});
