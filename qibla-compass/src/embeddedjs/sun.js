// Where the sun is in the sky, from NOAA's solar calculator equations (based
// on Meeus, "Astronomical Algorithms"). They agree with NREL's far longer
// Solar Position Algorithm to a few hundredths of a degree, much finer than
// anyone can point a watch at the sun.
//
// This module stands alone, without imports, so Node.js can test it directly.

const RADIANS = Math.PI / 180;
const MS_PER_DAY = 86400000;

// The sun's centre is on the horizon at about -0.83°: refraction lifts it into
// view while it is still geometrically below. This is the usual sunrise and
// sunset definition.
export const SUN_HORIZON = -0.833;

function normalize(degrees) {
  return (degrees % 360 + 360) % 360;
}

// Returns the azimuth (degrees clockwise from true north) and elevation
// (degrees above the horizon) of the sun at the given time and place.
export function calculateSunPosition(date, latitude, longitude) {
  const time = date.getTime();
  const julianCentury = (time / MS_PER_DAY + 2440587.5 - 2451545) / 36525;
  const t = julianCentury;

  const meanLongitude = normalize(280.46646 + t * (36000.76983 + t * 0.0003032));
  const meanAnomaly = (357.52911 + t * (35999.05029 - 0.0001537 * t)) * RADIANS;
  const eccentricity = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  const equationOfCentre =
    Math.sin(meanAnomaly) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
    Math.sin(2 * meanAnomaly) * (0.019993 - 0.000101 * t) +
    Math.sin(3 * meanAnomaly) * 0.000289;

  const nutation = (125.04 - 1934.136 * t) * RADIANS;
  const apparentLongitude =
    (meanLongitude + equationOfCentre - 0.00569 - 0.00478 * Math.sin(nutation)) * RADIANS;
  const meanObliquity =
    23 + (26 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60) / 60;
  const obliquity = (meanObliquity + 0.00256 * Math.cos(nutation)) * RADIANS;
  const declination = Math.asin(Math.sin(obliquity) * Math.sin(apparentLongitude));

  // How far the sun runs ahead of or behind clock time, in minutes.
  const y = Math.tan(obliquity / 2) ** 2;
  const longitudeRadians = meanLongitude * RADIANS;
  const equationOfTime = 4 / RADIANS * (
    y * Math.sin(2 * longitudeRadians) -
    2 * eccentricity * Math.sin(meanAnomaly) +
    4 * eccentricity * y * Math.sin(meanAnomaly) * Math.cos(2 * longitudeRadians) -
    0.5 * y * y * Math.sin(4 * longitudeRadians) -
    1.25 * eccentricity * eccentricity * Math.sin(2 * meanAnomaly)
  );

  const minutesIntoDay = ((time % MS_PER_DAY) + MS_PER_DAY) % MS_PER_DAY / 60000;
  const solarMinutes = minutesIntoDay + equationOfTime + 4 * longitude;
  const hourAngle = (solarMinutes / 4 - 180) * RADIANS;
  const latitudeRadians = latitude * RADIANS;

  const elevation = Math.asin(
    Math.sin(latitudeRadians) * Math.sin(declination) +
    Math.cos(latitudeRadians) * Math.cos(declination) * Math.cos(hourAngle)
  );
  // atan2 measures from due south, turning west; adding 180° measures from
  // north, turning east, like a compass.
  const azimuth = Math.atan2(
    Math.sin(hourAngle),
    Math.cos(hourAngle) * Math.sin(latitudeRadians) -
      Math.tan(declination) * Math.cos(latitudeRadians)
  );

  return {
    azimuth: normalize(azimuth / RADIANS + 180),
    elevation: elevation / RADIANS
  };
}
