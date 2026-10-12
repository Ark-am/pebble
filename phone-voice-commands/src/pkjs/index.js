// Serves the Settings page (via Clay). Commands themselves are handled by the
// Android companion app, which receives the watch's messages alongside this
// script.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Runs inside the Settings page. Each "Move up" button swaps its quick command
// with the one above, so the list can be reordered without retyping. Clay
// passes only this function's text to the page, so it must not use anything
// defined outside it.
function customClay() {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    [2, 3, 4, 5, 6].forEach(function (slot) {
      clayConfig.getItemById('QUICK_UP_' + slot).on('click', function () {
        var above = clayConfig.getItemByMessageKey('QUICK_' + (slot - 1));
        var here = clayConfig.getItemByMessageKey('QUICK_' + slot);
        var text = above.get();
        above.set(here.get());
        here.set(text);
      });
    });
  });
}

new Clay(clayConfig, customClay);
