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
