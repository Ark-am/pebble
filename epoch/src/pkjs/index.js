// Serves the settings page. Clay shows it on the phone and sends the chosen
// values to the watch.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

new Clay(clayConfig);
