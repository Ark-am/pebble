import Poco from "commodetto/Poco";
import Button from "pebble/button";
import Message from "pebble/message";
import Vibes from "pebble/vibes";
import RetryLocation from "./retry-location";

import {
  calculateMagneticDeclination,
  decimalYear
} from "./geomagnetic";
import {
  calculateQiblaBearing,
  calculateQiblaDistance,
  compassPoint,
  normalizeDegrees,
  normalizeSignedDegrees
} from "./qibla";
import { SUN_HORIZON, calculateSunPosition } from "./sun";

const render = new Poco(screen);
const largeBoldFont = new render.Font("Gothic-Bold", 24);
const mediumBoldFont = new render.Font("Gothic-Bold", 18);
const smallFont = new render.Font("Gothic-Regular", 14);
const smallBoldFont = new render.Font("Gothic-Bold", 14);

const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);
const lightGray = render.makeColor(170, 170, 170);
const gray = render.makeColor(85, 85, 85);
const gold = render.makeColor(255, 170, 0);
const darkGold = render.makeColor(170, 85, 0);
const red = render.makeColor(255, 85, 85);
const green = render.makeColor(0, 255, 85);
const darkGreen = render.makeColor(0, 170, 85);
const yellow = render.makeColor(255, 255, 0);

// The dial. Angles on screen are degrees clockwise from 12 o'clock.
const CENTER_X = 100;
const CENTER_Y = 105;
const DIAL_RADIUS = 57;
const LABEL_RADIUS = DIAL_RADIUS - 16;
// The band between north and the Qibla that shows how its bearing is measured.
const ARC_RADIUS = 26;
const ARC_WIDTH = 4;

const ORIGIN_Y = 23;
const INSTRUCTION_Y = 166;
const DETAIL_Y = 192;
const MODES_Y = 210;
const MODE_WIDTH = 72;
const MODE_HEIGHT = 18;
const MODE_GAP = 6;
// The padlock after the modes, which keeps the dial still.
const LOCK_WIDTH = 30;
const NOTICE_TIME = 2500;

const ALIGNED_DEGREES = 4;
// Near the zenith the sun's direction is too vague to aim at.
const SUN_HIGHEST = 75;
const SENSOR_TIMEOUT = 15000;
const APP_VERSION = "1.0.5";
const DEBUG = !!Natives.isDebugBuild();

// "compass" turns the dial with the watch's heading. "sun" sets the dial by
// the sun's direction, so it works without the compass.
const MODES = ["compass", "sun"];
const MODE_LABELS = { compass: "Compass", sun: "Sun" };
const MODE_KEY = "mode";
const STILL_KEY = "still";

let qiblaBearing;
let qiblaDistance;
let userLatitude;
let userLongitude;
let magneticHeading;
let smoothedHeading;
let magneticDeclination = 0;
let locationStatus = "Getting phone location";
let compassStatus = "Waiting for compass";
let previouslyAligned = false;
let location;
let locationTimer;
let compassTimer;
let compassPollTimer;
let locationFailed = false;
let compassFailed = false;
let mode = loadMode();
// With the dial still, north stays at the top and an arrow shows the way the
// watch faces, instead of the dial turning.
let dialStill = loadStill();
let notice;
let noticeTimer;

function debugLog(message) {
  if (DEBUG) {
    console.log(message);
  }
}

function loadMode() {
  try {
    const saved = localStorage.getItem(MODE_KEY);
    if (MODES.includes(saved)) {
      return saved;
    }
  }
  catch {
    // Without storage the app simply starts in compass mode.
  }
  return "compass";
}

function setMode(next) {
  if (next === mode) {
    return;
  }

  mode = next;
  previouslyAligned = false;
  try {
    localStorage.setItem(MODE_KEY, mode);
  }
  catch {
    // Remembering the mode is a convenience; carry on without it.
  }
  debugLog("Mode: " + mode);
  updateDirection();
}

function loadStill() {
  try {
    return localStorage.getItem(STILL_KEY) === "1";
  }
  catch {
    return false;
  }
}

function toggleStill() {
  dialStill = !dialStill;
  try {
    localStorage.setItem(STILL_KEY, dialStill ? "1" : "0");
  }
  catch {
    // Remembering the choice is a convenience; carry on without it.
  }
  debugLog("Dial still: " + dialStill);
  showNotice(dialStill ? "The dial stays still, north up" : "The dial turns with you");
}

// Shows a short confirmation in place of the detail line.
function showNotice(text) {
  if (noticeTimer !== undefined) {
    clearTimeout(noticeTimer);
  }
  notice = text;
  noticeTimer = setTimeout(() => {
    noticeTimer = undefined;
    notice = undefined;
    updateDirection();
  }, NOTICE_TIME);
  updateDirection();
}

function retryAvailable() {
  return locationFailed || compassFailed;
}

function degreesText(degrees) {
  return Math.round(normalizeDegrees(degrees)) % 360 + "\u00B0";
}

function coordinateText(value, positive, negative) {
  return Math.abs(value).toFixed(2) + "\u00B0" + (value < 0 ? negative : positive);
}

function distanceText(kilometres) {
  return String(Math.round(kilometres)).replace(/\B(?=(\d{3})+(?!\d))/g, ",") + " km";
}

function turnText(turnAngle) {
  const direction = turnAngle < 0 ? "left" : "right";
  return "Turn " + direction + " " + Math.round(Math.abs(turnAngle)) + "\u00B0";
}

function centeredText(text, font, color, y) {
  const width = render.getTextWidth(text, font);
  render.drawText(text, font, color, Math.round((render.width - width) / 2), y);
}

// Draws text centred on a point.
function textAt(text, font, color, x, y) {
  const width = render.getTextWidth(text, font);
  render.drawText(
    text,
    font,
    color,
    Math.round(x - width / 2),
    Math.round(y - font.height / 2)
  );
}

function dialPoint(angle, radius) {
  const radians = angle * Math.PI / 180;
  return {
    x: Math.round(CENTER_X + Math.sin(radians) * radius),
    y: Math.round(CENTER_Y - Math.cos(radians) * radius)
  };
}

function disc(color, x, y, radius) {
  render.drawCircle(color, Math.round(x), Math.round(y), radius);
}

// Fills the wedge of the dial from `from` clockwise through `sweep` degrees.
function fillWedge(color, radius, from, sweep) {
  const start = Math.round(normalizeDegrees(from));
  const end = Math.round(normalizeDegrees(from) + sweep);
  if (end <= start) {
    return;
  }

  // The firmware draws one turn at most, so a wedge across 12 o'clock is
  // drawn as two.
  if (end > 360) {
    render.drawCircle(color, CENTER_X, CENTER_Y, radius, start, 360);
    render.drawCircle(color, CENTER_X, CENTER_Y, radius, 0, end - 360);
  }
  else {
    render.drawCircle(color, CENTER_X, CENTER_Y, radius, start, end);
  }
}

// A triangle pointing down at the dial: the way the top of the watch faces.
function drawPointer(color) {
  const bottom = CENTER_Y - DIAL_RADIUS - 1;
  const rows = 8;
  for (let row = 0; row <= rows; row++) {
    const half = Math.round(6 * (rows - row) / rows);
    render.fillRectangle(color, CENTER_X - half, bottom - rows + row, half * 2 + 1, 1);
  }
}

function drawKaaba(x, y, color) {
  render.fillRectangle(color, x - 7, y - 7, 15, 15);
  render.fillRectangle(black, x - 5, y - 5, 11, 11);
  // The gold band around the kiswah.
  render.fillRectangle(color, x - 5, y - 3, 11, 2);
}

function drawSun(x, y) {
  disc(black, x, y, 10);
  for (let ray = 0; ray < 360; ray += 45) {
    const radians = ray * Math.PI / 180;
    const sin = Math.sin(radians);
    const cos = Math.cos(radians);
    render.drawLine(
      Math.round(x + sin * 7),
      Math.round(y - cos * 7),
      Math.round(x + sin * 9),
      Math.round(y - cos * 9),
      yellow,
      2
    );
  }
  disc(yellow, x, y, 5);
}

// An arrow from you the way the top of the watch faces. It stops short of the
// letters so it never hides them.
function drawNeedle(angle, color) {
  const radians = angle * Math.PI / 180;
  const sin = Math.sin(radians);
  const cos = Math.cos(radians);
  const length = LABEL_RADIUS - 10;
  const tipX = Math.round(CENTER_X + sin * length);
  const tipY = Math.round(CENTER_Y - cos * length);
  render.drawLine(
    Math.round(CENTER_X - sin * 8),
    Math.round(CENTER_Y + cos * 8),
    tipX,
    tipY,
    color,
    3
  );
  for (const side of [-0.5, 0.5]) {
    render.drawLine(
      tipX,
      tipY,
      Math.round(tipX - Math.sin(radians + side) * 9),
      Math.round(tipY + Math.cos(radians + side) * 9),
      color,
      3
    );
  }
}

// `rotation` is the true bearing shown at 12 o'clock. The other bearings are
// left out when undefined: `shadow` is where your shadow falls, and `facing`
// is where the watch faces, drawn as an arrow.
function drawDial(rotation, sun, shadow, facing, aligned) {
  disc(gray, CENTER_X, CENTER_Y, DIAL_RADIUS);
  disc(black, CENTER_X, CENTER_Y, DIAL_RADIUS - 2);

  for (let bearing = 0; bearing < 360; bearing += 10) {
    const major = bearing % 30 === 0;
    const outer = dialPoint(bearing - rotation, DIAL_RADIUS - 4);
    const inner = dialPoint(bearing - rotation, DIAL_RADIUS - (major ? 10 : 7));
    render.drawLine(
      outer.x,
      outer.y,
      inner.x,
      inner.y,
      major ? lightGray : gray,
      major ? 2 : 1
    );
  }

  for (let bearing = 0; bearing < 360; bearing += 30) {
    const at = dialPoint(bearing - rotation, LABEL_RADIUS);
    if (bearing % 90 === 0) {
      const letter = "NESW"[bearing / 90];
      textAt(letter, mediumBoldFont, bearing === 0 ? red : white, at.x, at.y);
    }
    else {
      textAt(String(bearing), smallFont, gray, at.x, at.y);
    }
  }

  const qiblaColor = aligned ? green : gold;
  if (qiblaBearing !== undefined) {
    // North to the Qibla, clockwise: the angle the bearing measures.
    fillWedge(aligned ? darkGreen : darkGold, ARC_RADIUS, -rotation, qiblaBearing);
    disc(black, CENTER_X, CENTER_Y, ARC_RADIUS - ARC_WIDTH);
  }

  const north = dialPoint(-rotation, ARC_RADIUS + 6);
  render.drawLine(CENTER_X, CENTER_Y, north.x, north.y, lightGray, 1);

  if (sun) {
    const at = dialPoint(sun.azimuth - rotation, DIAL_RADIUS);
    drawSun(at.x, at.y);
  }
  if (shadow !== undefined) {
    const end = dialPoint(shadow - rotation, DIAL_RADIUS - 12);
    render.drawLine(CENTER_X, CENTER_Y, end.x, end.y, gray, 6);
  }

  if (qiblaBearing !== undefined) {
    const end = dialPoint(qiblaBearing - rotation, DIAL_RADIUS - 8);
    render.drawLine(CENTER_X, CENTER_Y, end.x, end.y, qiblaColor, 3);
    const at = dialPoint(qiblaBearing - rotation, DIAL_RADIUS);
    drawKaaba(at.x, at.y, qiblaColor);
  }

  if (facing !== undefined) {
    drawNeedle(facing - rotation, aligned ? green : white);
  }

  // You are at the centre.
  disc(black, CENTER_X, CENTER_Y, 6);
  disc(white, CENTER_X, CENTER_Y, 4);
}

// The Qibla's bearing and its compass direction, such as "58.5° east-northeast".
function drawHeader() {
  if (qiblaBearing === undefined) {
    centeredText("QIBLA", largeBoldFont, white, 0);
    return;
  }

  let degrees = qiblaBearing.toFixed(1);
  if (degrees === "360.0") {
    degrees = "0.0";
  }
  degrees += "\u00B0";

  const point = compassPoint(qiblaBearing);
  const degreesWidth = render.getTextWidth(degrees, largeBoldFont);
  let direction = " " + point.name;
  if (degreesWidth + render.getTextWidth(direction, mediumBoldFont) > render.width - 8) {
    direction = " " + point.abbreviation;
  }

  const width = degreesWidth + render.getTextWidth(direction, mediumBoldFont);
  const x = Math.round((render.width - width) / 2);
  render.drawText(degrees, largeBoldFont, gold, x, 0);
  // Share the larger text's baseline.
  render.drawText(
    direction,
    mediumBoldFont,
    white,
    x + degreesWidth,
    largeBoldFont.ascent - mediumBoldFont.ascent
  );
}

// Where the bearing is measured from, and how far away the Kaaba is.
function drawOrigin() {
  if (userLatitude === undefined) {
    return;
  }

  centeredText(
    "From " + coordinateText(userLatitude, "N", "S") + " " +
      coordinateText(userLongitude, "E", "W") + " \u00B7 " +
      distanceText(qiblaDistance),
    smallFont,
    lightGray,
    ORIGIN_Y
  );
}

// The row along the bottom: the two modes, then the padlock.
function controls() {
  const items = MODES.map(key => ({ key, width: MODE_WIDTH }));
  items.push({ key: STILL_KEY, width: LOCK_WIDTH });

  const total = items.reduce((sum, item) => sum + item.width, 0) +
    (items.length - 1) * MODE_GAP;
  let x = Math.round((render.width - total) / 2);
  for (const item of items) {
    item.x = x;
    x += item.width + MODE_GAP;
  }
  return items;
}

// A padlock centred on x, y, closed while the dial is held still.
function drawPadlock(x, y, color, closed) {
  const lift = closed ? 0 : 2;
  render.fillRectangle(color, x - 5, y - 1, 11, 7);
  render.fillRectangle(color, x - 4, y - 7 - lift, 9, 2);
  render.fillRectangle(color, x - 4, y - 7 - lift, 2, 6 + lift);
  render.fillRectangle(color, x + 3, y - 7 - lift, 2, closed ? 6 : 3);
}

function drawControls() {
  for (const item of controls()) {
    const active = item.key === STILL_KEY ? dialStill : item.key === mode;
    if (active) {
      render.drawRoundRect(item.x, MODES_Y, item.width, MODE_HEIGHT, white, MODE_HEIGHT / 2);
    }
    else {
      render.frameRoundRect(item.x, MODES_Y, item.width, MODE_HEIGHT, gray, MODE_HEIGHT / 2);
    }

    const color = active ? black : lightGray;
    const middle = item.x + item.width / 2;
    if (item.key === STILL_KEY) {
      drawPadlock(Math.round(middle), MODES_Y + MODE_HEIGHT / 2 + 1, color, dialStill);
    }
    else {
      textAt(MODE_LABELS[item.key], smallBoldFont, color, middle, MODES_Y + MODE_HEIGHT / 2);
    }
  }
}

function handleAlignment(currentlyAligned) {
  if (currentlyAligned && !previouslyAligned) {
    Vibes.shortPulse();
  }

  previouslyAligned = currentlyAligned;
}

function compassProblem(sunUsable) {
  if (compassStatus === "Compass unavailable") {
    return {
      instruction: "Compass unavailable",
      detail: sunUsable ? "Press Down to use the sun" : "Tap or press Select to retry"
    };
  }
  if (compassStatus === "Waiting for compass") {
    return { instruction: "Waiting for compass", detail: "" };
  }
  return {
    instruction: "Move watch to calibrate",
    detail: "Swing it in a figure-eight"
  };
}

function smoothHeading(previous, next, factor = 0.2) {
  if (previous === undefined) {
    return next;
  }

  // Snap once close enough so polling can stop redrawing a settled heading.
  const difference = ((next - previous + 540) % 360) - 180;
  if (Math.abs(difference) < 0.5) {
    return next;
  }
  return normalizeDegrees(previous + difference * factor);
}

function updateDirection() {
  const sun = userLatitude === undefined
    ? undefined
    : calculateSunPosition(new Date(), userLatitude, userLongitude);
  const sunUp = sun !== undefined && sun.elevation > SUN_HORIZON;
  const sunUsable = sunUp && sun.elevation <= SUN_HIGHEST;
  const trueHeading = smoothedHeading === undefined
    ? undefined
    : normalizeDegrees(smoothedHeading + magneticDeclination);

  // Without a reference the dial shows north at the top, like a map.
  let rotation = 0;
  let pointer = false;
  let facing;
  let shadow;
  let sunReference;
  let aligned = false;
  let instruction;
  let instructionColor = white;
  let detail = "";

  if (mode === "compass" && trueHeading !== undefined) {
    if (dialStill) {
      facing = trueHeading;
    }
    else {
      rotation = trueHeading;
      pointer = true;
    }
  }
  else if (mode === "sun" && sunUsable) {
    // The way to face first. When the Qibla is behind you as you face the
    // sun, turning from your shadow is shorter, and spares you looking at it.
    sunReference = sun.azimuth;
    if (qiblaBearing !== undefined &&
        Math.abs(normalizeSignedDegrees(qiblaBearing - sun.azimuth)) > 90) {
      sunReference = normalizeDegrees(sun.azimuth + 180);
      shadow = sunReference;
    }
    if (!dialStill) {
      rotation = sunReference;
    }
  }

  if (qiblaBearing === undefined) {
    instruction = locationStatus;
    instructionColor = yellow;
    detail = locationFailed
      ? "Tap or press Select to retry"
      : "Your phone shares where you are";
  }
  else if (mode === "compass") {
    if (trueHeading !== undefined) {
      const turnAngle = normalizeSignedDegrees(qiblaBearing - trueHeading);
      aligned = Math.abs(turnAngle) <= ALIGNED_DEGREES;
      instruction = aligned ? "Qibla aligned" : turnText(turnAngle);
      instructionColor = aligned ? green : white;
      detail = "You face " + degreesText(trueHeading) + " " +
        compassPoint(trueHeading).name;

      debugLog("True heading: " + trueHeading + ", turn: " + turnAngle);
    }
    else {
      const problem = compassProblem(sunUsable);
      instruction = problem.instruction;
      instructionColor = yellow;
      detail = problem.detail;
    }
  }
  else if (!sunUp) {
    instruction = "The sun has set";
    instructionColor = yellow;
    detail = "Press Up to use the compass";
  }
  else if (!sunUsable) {
    instruction = "Sun too high to aim";
    instructionColor = yellow;
    detail = "Press Up to use the compass";
  }
  else {
    const fromShadow = shadow !== undefined;
    const turnAngle = normalizeSignedDegrees(qiblaBearing - sunReference);
    if (Math.abs(turnAngle) <= ALIGNED_DEGREES) {
      instruction = fromShadow ? "Face your shadow" : "Face the sun";
      instructionColor = green;
      detail = fromShadow
        ? "The Qibla lies straight along it"
        : "The Qibla lies straight toward it";
    }
    else {
      instruction = turnText(turnAngle);
      detail = "after facing " + (fromShadow ? "your shadow" : "the sun") +
        " at " + degreesText(sunReference) + " " +
        compassPoint(sunReference).abbreviation;
    }
  }

  handleAlignment(aligned);

  render.begin();
  render.fillRectangle(black, 0, 0, render.width, render.height);
  drawHeader();
  drawOrigin();
  drawDial(rotation, sunUp ? sun : undefined, shadow, facing, aligned);
  if (pointer) {
    drawPointer(aligned ? green : white);
  }
  centeredText(instruction, largeBoldFont, instructionColor, INSTRUCTION_Y);
  if (notice) {
    centeredText(notice, smallFont, white, DETAIL_Y);
  }
  else if (detail) {
    centeredText(detail, smallFont, lightGray, DETAIL_Y);
  }
  drawControls();
  render.end();
}

function requestLocation() {
  if (locationTimer !== undefined) {
    clearTimeout(locationTimer);
    locationTimer = undefined;
  }

  qiblaBearing = undefined;
  userLatitude = undefined;
  userLongitude = undefined;
  locationFailed = false;
  locationStatus = watch.connected.pebblekit
    ? "Getting phone location"
    : "Waiting for phone";
  updateDirection();

  if (!location) {
    location = new RetryLocation({
      onSample() {
        const sample = this.sample();
        if (!sample) {
          return;
        }

        if (locationTimer !== undefined) {
          clearTimeout(locationTimer);
          locationTimer = undefined;
        }

        userLatitude = sample.latitude;
        userLongitude = sample.longitude;
        qiblaBearing = calculateQiblaBearing(userLatitude, userLongitude);
        qiblaDistance = calculateQiblaDistance(userLatitude, userLongitude);
        const modelDate = decimalYear();
        magneticDeclination = calculateMagneticDeclination(
          userLatitude,
          userLongitude,
          modelDate
        );
        locationStatus = "Location ready";
        locationFailed = false;

        console.log("Location: " + userLatitude + ", " + userLongitude);
        console.log(
          "Magnetic declination: " + magneticDeclination.toFixed(2) +
          " at " + modelDate.toFixed(2)
        );

        this.stop();
        updateDirection();
      },
      onError(error) {
        console.log("Location error: " + error);
        if (locationTimer !== undefined) {
          clearTimeout(locationTimer);
          locationTimer = undefined;
        }
        this.stop();
        locationStatus = "Location unavailable";
        locationFailed = true;
        updateDirection();
      }
    });
  }

  location.request({
    enableHighAccuracy: false,
    timeout: 10000,
    maximumAge: 3600000
  });

  // The phone normally reports its own timeout. This second timeout also
  // handles a disconnected or non-running PebbleKit JS bridge.
  locationTimer = setTimeout(() => {
    locationTimer = undefined;
    locationStatus = "Phone/location timed out";
    locationFailed = true;
    updateDirection();
  }, SENSOR_TIMEOUT);
}

function updateHeading(heading) {
  if (typeof heading !== "number" || heading !== heading) {
    return;
  }

  if (compassTimer !== undefined) {
    clearTimeout(compassTimer);
    compassTimer = undefined;
  }
  magneticHeading = normalizeDegrees(heading);
  smoothedHeading = smoothHeading(smoothedHeading, magneticHeading);
  compassStatus = "Compass ready";
  compassFailed = false;
  updateDirection();
}

function pollCompass() {
  const heading = Natives.compassReadHeading();

  if (heading >= 0) {
    if (heading !== magneticHeading) {
      debugLog("Compass heading: " + heading);
      updateHeading(heading);
    }
    else if (smoothedHeading !== magneticHeading) {
      // The sensor only reports changes, so keep easing toward the latest
      // reading; otherwise the arrow stops short once the watch is held still.
      updateHeading(heading);
    }
    else if (compassTimer !== undefined) {
      clearTimeout(compassTimer);
      compassTimer = undefined;
    }
    return;
  }

  if (heading === -2) {
    if (compassPollTimer !== undefined) {
      clearInterval(compassPollTimer);
      compassPollTimer = undefined;
    }
    if (compassTimer !== undefined) {
      clearTimeout(compassTimer);
      compassTimer = undefined;
    }
    compassStatus = "Compass unavailable";
    compassFailed = true;
    updateDirection();
  }
  else if (compassStatus !== "Calibrating compass") {
    compassStatus = "Calibrating compass";
    updateDirection();
  }
}

function startCompass() {
  if (compassTimer !== undefined) {
    clearTimeout(compassTimer);
    compassTimer = undefined;
  }

  if (compassPollTimer !== undefined) {
    clearInterval(compassPollTimer);
    compassPollTimer = undefined;
  }

  magneticHeading = undefined;
  smoothedHeading = undefined;
  compassStatus = "Waiting for compass";
  compassFailed = false;

  if (!Natives.compassStart()) {
    compassStatus = "Compass unavailable";
    compassFailed = true;
    updateDirection();
    return;
  }

  pollCompass();
  compassPollTimer = setInterval(pollCompass, 250);
  compassTimer = setTimeout(() => {
    compassTimer = undefined;
    compassStatus = "Move watch to calibrate";
    compassFailed = true;
    updateDirection();
  }, SENSOR_TIMEOUT);
}

function retrySensors() {
  debugLog("Retrying location and compass");
  previouslyAligned = false;
  startCompass();
  requestLocation();
}

// Restart only what failed, so an accidental tap cannot discard a working
// location or interrupt a compass that has since recovered.
function retryFailedSensors() {
  debugLog("Retrying failed sensors");
  if (compassFailed) {
    previouslyAligned = false;
    startCompass();
  }
  if (locationFailed) {
    requestLocation();
  }
}

let virtualCompass;
if (DEBUG) {
  // The current Pebble emulator does not always forward emulated compass
  // events to Alloy. Debug builds expose this deterministic test input.
  virtualCompass = new Message({
    keys: new Map([["DEBUG_HEADING", 16000]]),
    onReadable() {
      const message = this.read();
      const heading = Number(message.get("DEBUG_HEADING"));
      // Injected headings are exact test input, so apply them unsmoothed.
      if (heading === heading) {
        smoothedHeading = undefined;
      }
      updateHeading(heading);
    }
  });
}

console.log("Qibla Compass " + APP_VERSION);
updateDirection();
retrySensors();

// Pressing Select retries, even when the phone bridge never becomes
// available and therefore cannot deliver an error callback. Holding it keeps
// the dial still or lets it turn again, like the padlock.
const selectButton = new Button({
  type: "select",
  single: true,
  long: true,
  onPush(pushed, button, recognizer) {
    if (!pushed) {
      return;
    }
    if (recognizer === "long") {
      toggleStill();
    }
    else {
      retrySensors();
    }
  }
});

// Up and Down pick the mode above them on screen: compass, then sun.
const modeButtons = new Button({
  types: ["up", "down"],
  onPush(pushed, button) {
    if (pushed) {
      setMode(button === "up" ? "compass" : "sun");
    }
  }
});

let touchPoint;
const retryTouch = new device.sensor.Touch({
  onSample() {
    const points = this.sample();
    if (!points) {
      return;
    }

    if (points.length) {
      touchPoint = points[0];
      return;
    }
    if (!touchPoint) {
      return;
    }

    const { x, y } = touchPoint;
    touchPoint = undefined;
    if (y >= MODES_Y - 6) {
      // Tapping a mode selects it; tapping the padlock toggles it.
      for (const item of controls()) {
        const left = item.x - MODE_GAP / 2;
        if (x >= left && x < left + item.width + MODE_GAP) {
          if (item.key === STILL_KEY) {
            toggleStill();
          }
          else {
            setMode(item.key);
          }
        }
      }
    }
    else if (retryAvailable()) {
      retryFailedSensors();
    }
  }
});

// If the phone-side PebbleKit JS reconnects after launch, retry location
// automatically instead of requiring the user to restart the watch app.
watch.addEventListener("connected", function () {
  if (watch.connected.pebblekit && qiblaBearing === undefined) {
    requestLocation();
  }
});

// The sun moves about a quarter of a degree a minute.
watch.addEventListener("minutechange", updateDirection);
