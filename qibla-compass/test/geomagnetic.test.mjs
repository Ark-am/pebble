import assert from "node:assert/strict";
import test from "node:test";

import {
  calculateMagneticDeclination,
  decimalYear
} from "../src/embeddedjs/geomagnetic.js";

// Sea-level rows from NOAA's official WMM2025_TEST_VALUES.txt:
// https://www.ncei.noaa.gov/sites/default/files/2025-02/WMM2025_TEST_VALUES.txt
const NOAA_TEST_VALUES = [
  { year: 2025.0, latitude: 80, longitude: 0, declination: 1.28 },
  { year: 2025.0, latitude: 0, longitude: 120, declination: -0.16 },
  { year: 2025.0, latitude: -80, longitude: 240, declination: 68.78 },
  { year: 2027.5, latitude: 80, longitude: 0, declination: 2.59 },
  { year: 2027.5, latitude: 0, longitude: 120, declination: -0.24 },
  { year: 2027.5, latitude: -80, longitude: 240, declination: 68.49 }
];

for (const row of NOAA_TEST_VALUES) {
  test(`declination matches NOAA at ${row.latitude}, ${row.longitude} in ${row.year}`, () => {
    const declination = calculateMagneticDeclination(
      row.latitude,
      row.longitude,
      row.year
    );
    assert.ok(
      Math.abs(declination - row.declination) <= 0.01,
      `expected ${row.declination}, got ${declination.toFixed(4)}`
    );
  });
}

test("declination is not extrapolated past the model window", () => {
  assert.equal(
    calculateMagneticDeclination(23.8103, 90.4125, 2035),
    calculateMagneticDeclination(23.8103, 90.4125, 2030)
  );
});

test("decimalYear converts UTC dates to fractional years", () => {
  assert.equal(decimalYear(new Date(Date.UTC(2025, 0, 1))), 2025);
  assert.equal(decimalYear(new Date(Date.UTC(2027, 6, 2, 12))), 2027.5);
  assert.equal(decimalYear(new Date(Date.UTC(2028, 6, 2))), 2028.5);
});
