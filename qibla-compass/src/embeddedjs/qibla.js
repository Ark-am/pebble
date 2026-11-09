const KAABA_LATITUDE = 21.422487;
const KAABA_LONGITUDE = 39.826206;

function toRadians(degrees) {
  return degrees * Math.PI / 180;
}

function toDegrees(radians) {
  return radians * 180 / Math.PI;
}

export function normalizeDegrees(degrees) {
  return (degrees % 360 + 360) % 360;
}

export function normalizeSignedDegrees(degrees) {
  return ((degrees + 540) % 360) - 180;
}

export function calculateQiblaBearing(latitude, longitude) {
  const userLatitude = toRadians(latitude);
  const kaabaLatitude = toRadians(KAABA_LATITUDE);
  const longitudeDifference = toRadians(KAABA_LONGITUDE - longitude);

  const y = Math.sin(longitudeDifference) * Math.cos(kaabaLatitude);
  const x =
    Math.cos(userLatitude) * Math.sin(kaabaLatitude) -
    Math.sin(userLatitude) *
      Math.cos(kaabaLatitude) *
      Math.cos(longitudeDifference);

  return normalizeDegrees(toDegrees(Math.atan2(y, x)));
}

// Mean Earth radius (IUGG). A sphere is within about half a percent of the
// true distance, which is plenty for a rounded kilometre figure.
const EARTH_RADIUS_KM = 6371.0088;

export function calculateQiblaDistance(latitude, longitude) {
  const userLatitude = toRadians(latitude);
  const kaabaLatitude = toRadians(KAABA_LATITUDE);
  const latitudeDifference = kaabaLatitude - userLatitude;
  const longitudeDifference = toRadians(KAABA_LONGITUDE - longitude);

  const haversine =
    Math.sin(latitudeDifference / 2) ** 2 +
    Math.cos(userLatitude) * Math.cos(kaabaLatitude) *
      Math.sin(longitudeDifference / 2) ** 2;
  return 2 * EARTH_RADIUS_KM * Math.asin(Math.min(1, Math.sqrt(haversine)));
}

const COMPASS_POINTS = [
  ["N", "north"],
  ["NNE", "north-northeast"],
  ["NE", "northeast"],
  ["ENE", "east-northeast"],
  ["E", "east"],
  ["ESE", "east-southeast"],
  ["SE", "southeast"],
  ["SSE", "south-southeast"],
  ["S", "south"],
  ["SSW", "south-southwest"],
  ["SW", "southwest"],
  ["WSW", "west-southwest"],
  ["W", "west"],
  ["WNW", "west-northwest"],
  ["NW", "northwest"],
  ["NNW", "north-northwest"]
];

// The nearest of the 16 compass points, such as "ENE" and "east-northeast".
export function compassPoint(bearing) {
  const index = Math.round(normalizeDegrees(bearing) / 22.5) % 16;
  const [abbreviation, name] = COMPASS_POINTS[index];
  return { abbreviation, name };
}
