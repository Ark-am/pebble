// Serves the settings page (via Clay) and, when the small dial shows the
// weather, fetches it for the phone's location from Open-Meteo, which needs no
// API key. The watch gets the temperature in the chosen unit and a short
// condition name.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Clay shows the settings page and sends the chosen values to the watch.
new Clay(clayConfig);

var LOCATION_OPTIONS = { timeout: 15000, maximumAge: 10 * 60 * 1000 };
var INFO_WEATHER = '2';

// WMO weather interpretation codes, as used by Open-Meteo.
function conditionName(code) {
  if (code === 0) return 'CLEAR';
  if (code <= 2) return 'FAIR';
  if (code === 3) return 'CLOUDY';
  if (code === 45 || code === 48) return 'FOG';
  if (code >= 51 && code <= 57) return 'DRIZZLE';
  if (code >= 61 && code <= 67) return 'RAIN';
  if (code >= 71 && code <= 77) return 'SNOW';
  if (code >= 80 && code <= 82) return 'SHOWERS';
  if (code === 85 || code === 86) return 'SNOW';
  if (code >= 95) return 'STORM';
  return 'WEATHER';
}

function savedSettings() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (error) {
    return {};
  }
}

function sendWeather(position) {
  var url = 'https://api.open-meteo.com/v1/forecast' +
    '?latitude=' + position.coords.latitude.toFixed(3) +
    '&longitude=' + position.coords.longitude.toFixed(3) +
    '&current=temperature_2m,weather_code' +
    (savedSettings().TEMPERATURE_UNIT === 'F' ? '&temperature_unit=fahrenheit' : '');

  var request = new XMLHttpRequest();
  request.onload = function () {
    if (request.status !== 200) {
      console.log('Weather request failed: HTTP ' + request.status);
      return;
    }
    var current = JSON.parse(request.responseText).current;
    Pebble.sendAppMessage({
      TEMPERATURE: Math.round(current.temperature_2m),
      CONDITION: conditionName(current.weather_code)
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

// Only ask for the location when the weather is actually shown.
function fetchWeatherIfShown() {
  if (String(savedSettings().INFO) === INFO_WEATHER) {
    fetchWeather();
  }
}

Pebble.addEventListener('ready', fetchWeatherIfShown);

// Runs after Clay has saved the settings, so choosing the weather or a new
// temperature unit applies straight away.
Pebble.addEventListener('webviewclosed', function (event) {
  if (event && event.response) {
    fetchWeatherIfShown();
  }
});

Pebble.addEventListener('appmessage', function (event) {
  if ('REQUEST_WEATHER' in event.payload) {
    fetchWeather();
  }
});
