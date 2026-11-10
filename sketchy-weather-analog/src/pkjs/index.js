// Serves the settings page (via Clay) and fetches the current weather for the
// phone's location from Open-Meteo, which needs no API key. The watch gets the
// temperature in the chosen unit, a short condition name, and whether it is
// daytime there, so the icons show a sun or a moon, or stars at night.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Clay shows the settings page and sends the chosen values to the watch.
new Clay(clayConfig, require('./custom-clay'));

var LOCATION_OPTIONS = { timeout: 15000, maximumAge: 10 * 60 * 1000 };

// The condition for each WMO weather interpretation code, as used by
// Open-Meteo. The names match the watch's (weather_from_condition in
// src/c/main.c).
var CONDITIONS = {
  0: 'CLEAR',
  1: 'FAIR',             // mainly clear
  2: 'PARTLY_CLOUDY',
  3: 'CLOUDY',
  45: 'FOG', 48: 'FOG',  // fog and depositing rime fog
  51: 'DRIZZLE', 53: 'DRIZZLE', 55: 'DRIZZLE',
  56: 'ICY_DRIZZLE', 57: 'ICY_DRIZZLE',
  61: 'LIGHT_RAIN', 63: 'RAIN', 65: 'HEAVY_RAIN',
  66: 'ICY_RAIN', 67: 'ICY_RAIN',
  71: 'LIGHT_SNOW', 73: 'SNOW', 75: 'HEAVY_SNOW', 77: 'LIGHT_SNOW',  // 77: snow grains
  80: 'SHOWERS', 81: 'SHOWERS', 82: 'SHOWERS',
  85: 'SNOW_SHOWERS', 86: 'SNOW_SHOWERS',
  95: 'STORM', 96: 'HAIL_STORM', 99: 'HAIL_STORM'
};

// A dry sky shows heat, cold or wind instead when they stand out: at or above
// HOT_CELSIUS, at or below COLD_CELSIUS, or in winds of WINDY_KMH or more.
var DRY_SKIES = ['CLEAR', 'FAIR', 'PARTLY_CLOUDY', 'CLOUDY'];
var HOT_CELSIUS = 32;
var COLD_CELSIUS = -10;
var WINDY_KMH = 40;

function conditionName(code, celsius, windKmh) {
  var name = CONDITIONS[code] || 'WEATHER';
  if (DRY_SKIES.indexOf(name) >= 0) {
    if (celsius >= HOT_CELSIUS) return 'HOT';
    if (celsius <= COLD_CELSIUS) return 'COLD';
    if (windKmh >= WINDY_KMH) return 'WINDY';
  }
  return name;
}

function savedSettings() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (error) {
    return {};
  }
}

function fahrenheit() {
  return savedSettings().TEMPERATURE_UNIT === 'F';
}

function sendWeather(position) {
  var url = 'https://api.open-meteo.com/v1/forecast' +
    '?latitude=' + position.coords.latitude.toFixed(3) +
    '&longitude=' + position.coords.longitude.toFixed(3) +
    '&current=temperature_2m,weather_code,is_day,wind_speed_10m' +
    (fahrenheit() ? '&temperature_unit=fahrenheit' : '');

  var request = new XMLHttpRequest();
  request.onload = function () {
    if (request.status !== 200) {
      console.log('Weather request failed: HTTP ' + request.status);
      return;
    }
    var current = JSON.parse(request.responseText).current;
    var temperature = current.temperature_2m;
    var celsius = fahrenheit() ? (temperature - 32) * 5 / 9 : temperature;
    Pebble.sendAppMessage({
      TEMPERATURE: Math.round(temperature),
      CONDITION: conditionName(current.weather_code, celsius, current.wind_speed_10m),
      DAYTIME: current.is_day
    }, null, function () {
      console.log('Could not send weather to the watch');
    });
  };
  request.onerror = function () {
    console.log('Weather request failed');
  };
  request.open('GET', url);
  request.send();
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(sendWeather, function (error) {
    console.log('Location unavailable: ' + error.message);
  }, LOCATION_OPTIONS);
}

Pebble.addEventListener('ready', fetchWeather);

// Runs after Clay has saved the settings, so a new temperature unit applies
// straight away.
Pebble.addEventListener('webviewclosed', function (event) {
  if (event && event.response) {
    fetchWeather();
  }
});

Pebble.addEventListener('appmessage', function (event) {
  if ('REQUEST_WEATHER' in event.payload) {
    fetchWeather();
  }
});
