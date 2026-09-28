// World Magnetic Model 2025 coefficients. The model is valid from 2025
// through 2029 and provides east-positive magnetic declination in degrees.
const MODEL_EPOCH = 2025;
const MODEL_END = 2030;
const MAX_DEGREE = 12;
const COEFFICIENT_COUNT = 91;

const WGS84_SEMI_MAJOR_AXIS = 6378.137;
const WGS84_SEMI_MINOR_AXIS = 6356.7523142;
const MAGNETIC_REFERENCE_RADIUS = 6371.2;

// Coefficients are indexed by n * (n + 1) / 2 + m. Index zero is unused.
// Source: NOAA/NCEI WMM2025.COF, released 2024-11-13.
const G = [
  0.0, -29351.8, -1410.8, -2556.6, 2951.1, 1649.3, 1361.0,
  -2404.1, 1243.8, 453.6, 895.0, 799.5, 55.7, -281.1, 12.1, -233.2,
  368.9, 187.2, -138.7, -142.0, 20.9, 64.4, 63.8, 76.9, -115.7,
  -40.9, 14.9, -60.7, 79.5, -77.0, -8.8, 59.3, 15.8, 2.5, -11.1,
  14.2, 23.2, 10.8, -17.5, 2.0, -21.7, 16.9, 15.0, -16.8, 0.9,
  4.6, 7.8, 3.0, -0.2, -2.5, -13.1, 2.4, 8.6, -8.7, -12.9, -1.3,
  -6.4, 0.2, 2.0, -1.0, -0.6, -0.9, 1.5, 0.9, -2.7, -3.9, 2.9,
  -1.5, -2.5, 2.4, -0.6, -0.1, -0.6, -0.1, 1.1, -1.0, -0.2, 2.6,
  -2.0, -0.2, 0.3, 1.2, -1.3, 0.6, 0.6, 0.5, -0.1, -0.4, -0.2,
  -1.3, -0.7
];

const H = [
  0.0, 0.0, 4545.4, 0.0, -3133.6, -815.1, 0.0, -56.6, 237.5,
  -549.5, 0.0, 278.6, -133.9, 212.0, -375.6, 0.0, 45.4, 220.2,
  -122.9, 43.0, 106.1, 0.0, -18.4, 16.8, 48.8, -59.8, 10.9, 72.7,
  0.0, -48.9, -14.4, -1.0, 23.4, -7.4, -25.1, -2.3, 0.0, 7.1,
  -12.6, 11.4, -9.7, 12.7, 0.7, -5.2, 3.9, 0.0, -24.8, 12.2,
  8.3, -3.3, -5.2, 7.2, -0.6, 0.8, 10.0, 0.0, 3.3, 0.0, 2.4, 5.3,
  -9.1, 0.4, -4.2, -3.8, 0.9, -9.1, 0.0, 0.0, 2.9, -0.6, 0.2,
  0.5, -0.3, -1.2, -1.7, -2.9, -1.8, -2.3, 0.0, -1.3, 0.7, 1.0,
  -1.4, 0.0, 0.6, -0.1, 0.8, 0.1, -1.0, 0.1, 0.2
];

const DELTA_G = [
  0.0, 12.0, 9.7, -11.6, -5.2, -8.0, -1.3, -4.2, 0.4, -15.6,
  -1.6, -2.4, -6.0, 5.6, -7.0, 0.6, 1.4, 0.0, 0.6, 2.2, 0.9,
  -0.2, -0.4, 0.9, 1.2, -0.9, 0.3, 0.9, 0.0, -0.1, -0.1, 0.5,
  -0.1, -0.8, -0.8, 0.8, -0.1, 0.2, 0.0, 0.5, -0.1, 0.3, 0.2,
  0.0, 0.2, 0.0, -0.1, 0.1, 0.3, -0.3, 0.0, 0.3, -0.1, 0.1,
  -0.1, 0.1, 0.0, 0.1, 0.1, 0.0, -0.3, 0.0, -0.1, -0.1, 0.0,
  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.1, 0.0, 0.0, -0.1, -0.1,
  -0.1, -0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0,
  0.0, -0.1, 0.0, -0.1
];

const DELTA_H = [
  0.0, 0.0, -21.5, 0.0, -27.7, -12.1, 0.0, 4.0, -0.3, -4.1,
  0.0, -1.1, 4.1, 1.6, -4.4, 0.0, -0.5, 2.2, 0.4, 1.7, 1.9,
  0.0, 0.3, -1.6, -0.4, 0.9, 0.7, 0.9, 0.0, 0.6, 0.5, -0.8,
  0.0, -1.0, 0.6, -0.2, 0.0, -0.2, 0.5, -0.4, 0.4, -0.5,
  -0.6, 0.3, 0.2, 0.0, -0.3, 0.3, -0.3, 0.3, 0.2, -0.1, -0.2,
  0.4, 0.1, 0.0, 0.0, 0.0, -0.2, 0.1, -0.1, 0.1, 0.0, -0.1,
  0.2, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.0, 0.1, 0.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 0.0, -0.1, 0.1, 0.0, 0.0, 0.0, 0.0,
  0.0, 0.0, 0.0, -0.1
];

function coefficientIndex(degree, order) {
  return degree * (degree + 1) / 2 + order;
}

export function decimalYear(date = new Date()) {
  const year = date.getUTCFullYear();
  const isLeapYear = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const monthLengths = [31, isLeapYear ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  let elapsedDays = date.getUTCDate() - 1;

  for (let month = 0; month < date.getUTCMonth(); month++) {
    elapsedDays += monthLengths[month];
  }

  elapsedDays += (
    date.getUTCHours() +
    date.getUTCMinutes() / 60 +
    date.getUTCSeconds() / 3600
  ) / 24;

  return year + elapsedDays / (isLeapYear ? 366 : 365);
}

export function calculateMagneticDeclination(latitude, longitude, year = decimalYear()) {
  // Do not extrapolate secular variation beyond the published model window.
  const modelYear = Math.max(MODEL_EPOCH, Math.min(year, MODEL_END));
  const yearsSinceEpoch = modelYear - MODEL_EPOCH;
  const g = new Array(COEFFICIENT_COUNT);
  const h = new Array(COEFFICIENT_COUNT);

  for (let index = 0; index < COEFFICIENT_COUNT; index++) {
    g[index] = G[index] + DELTA_G[index] * yearsSinceEpoch;
    h[index] = H[index] + DELTA_H[index] * yearsSinceEpoch;
  }

  const safeLatitude = Math.max(-89.9999, Math.min(latitude, 89.9999));
  const latitudeRadians = safeLatitude * Math.PI / 180;
  const longitudeRadians = longitude * Math.PI / 180;
  const sinLatitude = Math.sin(latitudeRadians);
  const cosLatitude = Math.cos(latitudeRadians);
  const eccentricitySquared = 1 -
    WGS84_SEMI_MINOR_AXIS * WGS84_SEMI_MINOR_AXIS /
    (WGS84_SEMI_MAJOR_AXIS * WGS84_SEMI_MAJOR_AXIS);

  // Convert sea-level geodetic coordinates to geocentric coordinates.
  const curvatureRadius = WGS84_SEMI_MAJOR_AXIS /
    Math.sqrt(1 - eccentricitySquared * sinLatitude * sinLatitude);
  const xPosition = curvatureRadius * cosLatitude;
  const zPosition = curvatureRadius * (1 - eccentricitySquared) * sinLatitude;
  const radius = Math.sqrt(xPosition * xPosition + zPosition * zPosition);
  const geocentricLatitude = Math.asin(zPosition / radius);
  const sinGeocentricLatitude = Math.sin(geocentricLatitude);
  const cosGeocentricLatitude = Math.cos(geocentricLatitude);

  const radiusPowers = new Array(MAX_DEGREE + 1);
  const radiusRatio = MAGNETIC_REFERENCE_RADIUS / radius;
  radiusPowers[0] = radiusRatio * radiusRatio;
  for (let degree = 1; degree <= MAX_DEGREE; degree++) {
    radiusPowers[degree] = radiusPowers[degree - 1] * radiusRatio;
  }

  const longitudeCosines = new Array(MAX_DEGREE + 1);
  const longitudeSines = new Array(MAX_DEGREE + 1);
  longitudeCosines[0] = 1;
  longitudeSines[0] = 0;
  longitudeCosines[1] = Math.cos(longitudeRadians);
  longitudeSines[1] = Math.sin(longitudeRadians);
  for (let order = 2; order <= MAX_DEGREE; order++) {
    longitudeCosines[order] =
      longitudeCosines[order - 1] * longitudeCosines[1] -
      longitudeSines[order - 1] * longitudeSines[1];
    longitudeSines[order] =
      longitudeSines[order - 1] * longitudeCosines[1] +
      longitudeCosines[order - 1] * longitudeSines[1];
  }

  const legendre = new Array(COEFFICIENT_COUNT).fill(0);
  const legendreDerivative = new Array(COEFFICIENT_COUNT).fill(0);
  legendre[0] = 1;

  for (let degree = 1; degree <= MAX_DEGREE; degree++) {
    for (let order = 0; order <= degree; order++) {
      const index = coefficientIndex(degree, order);

      if (degree === order) {
        const previous = coefficientIndex(degree - 1, order - 1);
        legendre[index] = cosGeocentricLatitude * legendre[previous];
        legendreDerivative[index] =
          cosGeocentricLatitude * legendreDerivative[previous] +
          sinGeocentricLatitude * legendre[previous];
      }
      else {
        const previous = coefficientIndex(degree - 1, order);

        if (order > degree - 2) {
          legendre[index] = sinGeocentricLatitude * legendre[previous];
          legendreDerivative[index] =
            sinGeocentricLatitude * legendreDerivative[previous] -
            cosGeocentricLatitude * legendre[previous];
        }
        else {
          const previousTwo = coefficientIndex(degree - 2, order);
          const recursionFactor =
            ((degree - 1) * (degree - 1) - order * order) /
            ((2 * degree - 1) * (2 * degree - 3));
          legendre[index] =
            sinGeocentricLatitude * legendre[previous] -
            recursionFactor * legendre[previousTwo];
          legendreDerivative[index] =
            sinGeocentricLatitude * legendreDerivative[previous] -
            cosGeocentricLatitude * legendre[previous] -
            recursionFactor * legendreDerivative[previousTwo];
        }
      }
    }
  }

  const schmidtFactors = new Array(COEFFICIENT_COUNT).fill(0);
  schmidtFactors[0] = 1;
  for (let degree = 1; degree <= MAX_DEGREE; degree++) {
    schmidtFactors[coefficientIndex(degree, 0)] =
      schmidtFactors[coefficientIndex(degree - 1, 0)] *
      (2 * degree - 1) / degree;

    for (let order = 1; order <= degree; order++) {
      schmidtFactors[coefficientIndex(degree, order)] =
        schmidtFactors[coefficientIndex(degree, order - 1)] *
        Math.sqrt(
          (degree - order + 1) * (order === 1 ? 2 : 1) /
          (degree + order)
        );
    }
  }

  let north = 0;
  let east = 0;
  let down = 0;

  for (let degree = 1; degree <= MAX_DEGREE; degree++) {
    for (let order = 0; order <= degree; order++) {
      const index = coefficientIndex(degree, order);
      const normalizedLegendre = legendre[index] * schmidtFactors[index];
      const normalizedDerivative = -legendreDerivative[index] * schmidtFactors[index];
      const longitudeField =
        g[index] * longitudeCosines[order] + h[index] * longitudeSines[order];
      const transverseField =
        g[index] * longitudeSines[order] - h[index] * longitudeCosines[order];

      down -= radiusPowers[degree] * (degree + 1) * longitudeField * normalizedLegendre;
      east += radiusPowers[degree] * order * transverseField * normalizedLegendre;
      north -= radiusPowers[degree] * longitudeField * normalizedDerivative;
    }
  }

  if (Math.abs(cosGeocentricLatitude) > 1e-10) {
    east /= cosGeocentricLatitude;
  }

  // Rotate the north component back into the geodetic reference frame.
  const latitudeDifference = geocentricLatitude - latitudeRadians;
  const geodeticNorth =
    north * Math.cos(latitudeDifference) - down * Math.sin(latitudeDifference);

  return Math.atan2(east, geodeticNorth) * 180 / Math.PI;
}
