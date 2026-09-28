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
  normalizeDegrees,
  normalizeSignedDegrees
} from "./qibla";

const render = new Poco(screen);
const titleFont = new render.Font("Gothic-Bold", 24);
const bodyFont = new render.Font("Gothic-Regular", 18);
const smallFont = new render.Font("Gothic-Regular", 14);

const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);
const green = render.makeColor(0, 255, 85);
const yellow = render.makeColor(255, 255, 0);
const gray = render.makeColor(85, 85, 85);

const CENTER_X = 100;
const CENTER_Y = 103;
const COMPASS_RADIUS = 59;
const SENSOR_TIMEOUT = 15000;
const APP_VERSION = "1.0.3";
const DEBUG = !!Natives.isDebugBuild();

let qiblaBearing;
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

function debugLog(message) {
  if (DEBUG) {
    console.log(message);
  }
}

function retryAvailable() {
  return locationFailed || compassFailed;
}

function centeredText(text, font, color, y) {
  const width = render.getTextWidth(text, font);
  render.drawText(text, font, color, (render.width - width) / 2, y);
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

function handleAlignment(currentlyAligned) {
  if (currentlyAligned && !previouslyAligned) {
    Vibes.shortPulse();
  }

  previouslyAligned = currentlyAligned;
}

function drawArrow(turnAngle, color) {
  const angle = turnAngle * Math.PI / 180;
  const tipX = CENTER_X + Math.sin(angle) * 44;
  const tipY = CENTER_Y - Math.cos(angle) * 44;
  const tailX = CENTER_X - Math.sin(angle) * 20;
  const tailY = CENTER_Y + Math.cos(angle) * 20;
  const leftAngle = angle - 0.55;
  const rightAngle = angle + 0.55;

  render.drawLine(tailX, tailY, tipX, tipY, color, 5);
  render.drawLine(
    tipX,
    tipY,
    tipX - Math.sin(leftAngle) * 18,
    tipY + Math.cos(leftAngle) * 18,
    color,
    5
  );
  render.drawLine(
    tipX,
    tipY,
    tipX - Math.sin(rightAngle) * 18,
    tipY + Math.cos(rightAngle) * 18,
    color,
    5
  );
  render.drawCircle(color, CENTER_X, CENTER_Y, 5);
}

function drawCompass(turnAngle, isAligned) {
  render.begin();
  render.fillRectangle(black, 0, 0, render.width, render.height);

  centeredText("QIBLA " + APP_VERSION, titleFont, white, 4);

  render.drawCircle(gray, CENTER_X, CENTER_Y, COMPASS_RADIUS);
  render.drawCircle(black, CENTER_X, CENTER_Y, COMPASS_RADIUS - 2);
  centeredText("N", smallFont, white, 30);

  render.drawLine(CENTER_X, 47, CENTER_X, 53, white, 2);
  render.drawLine(150, CENTER_Y, 156, CENTER_Y, white, 2);
  render.drawLine(CENTER_X, 153, CENTER_X, 159, white, 2);
  render.drawLine(44, CENTER_Y, 50, CENTER_Y, white, 2);

  if (turnAngle === undefined) {
    const isCalibrating =
      compassStatus === "Calibrating compass" ||
      compassStatus === "Move watch to calibrate";
    centeredText(
      isCalibrating
        ? "Move watch to calibrate"
        : retryAvailable()
          ? "Tap screen to retry"
          : "Waiting for sensors",
      smallFont,
      yellow,
      94
    );
  }
  else {
    const arrowColor = isAligned ? green : white;
    drawArrow(turnAngle, arrowColor);

    let instruction;
    if (isAligned) {
      instruction = "Qibla aligned";
    }
    else {
      const direction = turnAngle < 0 ? "left" : "right";
      instruction = "Turn " + direction + " " + Math.round(Math.abs(turnAngle)) + "\u00B0";
    }
    centeredText(instruction, bodyFont, isAligned ? green : white, 169);
  }

  if (qiblaBearing === undefined) {
    centeredText(locationStatus, smallFont, yellow, 194);
  }
  else {
    centeredText("Qibla: " + qiblaBearing.toFixed(1) + "\u00B0", smallFont, white, 194);
  }

  centeredText(
    compassStatus,
    smallFont,
    magneticHeading === undefined ? yellow : white,
    211
  );

  render.end();
}

function updateDirection() {
  if (qiblaBearing === undefined || smoothedHeading === undefined) {
    drawCompass(undefined, false);
    return;
  }

  const trueHeading = normalizeDegrees(smoothedHeading + magneticDeclination);
  const turnAngle = normalizeSignedDegrees(qiblaBearing - trueHeading);
  const isAligned = Math.abs(turnAngle) <= 4;

  debugLog("Qibla: " + qiblaBearing);
  debugLog("Magnetic heading: " + smoothedHeading);
  debugLog("True heading: " + trueHeading);
  debugLog("Turn angle: " + turnAngle);
  debugLog("Aligned: " + isAligned);

  handleAlignment(isAligned);
  drawCompass(turnAngle, isAligned);
}

function requestLocation() {
  if (locationTimer !== undefined) {
    clearTimeout(locationTimer);
    locationTimer = undefined;
  }

  qiblaBearing = undefined;
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

        qiblaBearing = calculateQiblaBearing(sample.latitude, sample.longitude);
        const modelDate = decimalYear();
        magneticDeclination = calculateMagneticDeclination(
          sample.latitude,
          sample.longitude,
          modelDate
        );
        locationStatus = "Location ready";
        locationFailed = false;

        console.log("Location: " + sample.latitude + ", " + sample.longitude);
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
  compassStatus = "Hold watch flat";
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

drawCompass(undefined, false);
retrySensors();

// The center/Select button provides a retry even when the phone bridge never
// becomes available and therefore cannot deliver an error callback.
const retryButton = new Button({
  type: "select",
  onPush(pushed) {
    if (pushed) {
      retrySensors();
    }
  }
});

let retryTouchDown = false;
const retryTouch = new device.sensor.Touch({
  onSample() {
    const points = this.sample();
    if (!points) {
      return;
    }

    if (points.length) {
      retryTouchDown = true;
    }
    else if (retryTouchDown) {
      retryTouchDown = false;
      if (retryAvailable()) {
        retryFailedSensors();
      }
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
