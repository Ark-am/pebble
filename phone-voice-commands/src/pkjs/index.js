// Serves the Settings page (via Clay). Commands themselves are handled by the
// Android companion app, which receives the watch's messages alongside this
// script.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

new Clay(clayConfig);
