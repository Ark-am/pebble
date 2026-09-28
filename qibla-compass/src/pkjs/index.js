const moddableProxy = require("@moddable/pebbleproxy");

Pebble.addEventListener("ready", function (event) {
  moddableProxy.readyReceived(event);
});

Pebble.addEventListener("appmessage", function (event) {
  moddableProxy.appMessageReceived(event);
});
