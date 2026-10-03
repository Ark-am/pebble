// Serves the settings page (via Clay) and tells the watch the current offset
// from UTC of each chosen time zone, with the name to show for it. The offsets
// include daylight saving (see dst.js); the watch asks again every hour so it
// follows the changes.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var ZONES = require('./zones');
var dst = require('./dst');

// Clay shows the settings page and sends the chosen values to the watch.
new Clay(clayConfig);

var DEFAULT_ZONES = ['Europe/London', 'Asia/Tokyo'];

function savedSettings() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (error) {
    return {};
  }
}

function zoneEntry(zone) {
  for (var i = 0; i < ZONES.length; i++) {
    if (ZONES[i].zone === zone) {
      return ZONES[i];
    }
  }
  return null;
}

function sendOffsets() {
  var settings = savedSettings();
  var now = new Date();
  var message = {};
  [1, 2].forEach(function (number) {
    var zone = settings['CITY_' + number + '_ZONE'] || DEFAULT_ZONES[number - 1];
    var entry = zoneEntry(zone);
    var name = String(settings['CITY_' + number + '_NAME'] || '').trim();
    message['CITY_' + number + '_LABEL'] = (name || (entry ? entry.city : zone)).substring(0, 15);
    message['CITY_' + number + '_OFFSET'] = entry ? dst.offsetMinutes(entry, now) : 0;
  });
  Pebble.sendAppMessage(message, null, function () {
    console.log('Could not send the time zones to the watch');
  });
}

Pebble.addEventListener('ready', sendOffsets);

// Runs after Clay has saved the settings, so new cities apply straight away.
Pebble.addEventListener('webviewclosed', function (event) {
  if (event && event.response) {
    sendOffsets();
  }
});

Pebble.addEventListener('appmessage', function (event) {
  if ('REQUEST_OFFSETS' in event.payload) {
    sendOffsets();
  }
});
