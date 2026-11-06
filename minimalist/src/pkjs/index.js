// Serves the settings page with Clay, which sends the chosen values to the
// watch when they are saved.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

new Clay(clayConfig);
