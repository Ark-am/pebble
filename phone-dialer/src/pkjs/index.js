// Serves the Settings page (via Clay). Everything else (contacts, call
// history and placing calls) is handled by the Android companion app, which
// receives the watch's messages alongside this script.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Runs inside the Settings page. Keeps the main menu order a true ordering:
// choosing an item that is already in another place swaps the two, so every
// item always appears exactly once. Clay passes only this function's text to
// the page, so it must not use anything defined outside it.
function customClay() {
  var clayConfig = this;
  var keys = ['MENU_1', 'MENU_2', 'MENU_3', 'MENU_4'];

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var selects = keys.map(function (key) {
      return clayConfig.getItemByMessageKey(key);
    });
    var current = selects.map(function (select) {
      return select.get();
    });

    selects.forEach(function (select, index) {
      select.on('change', function () {
        var chosen = select.get();
        var other = current.indexOf(chosen);
        var replaced = current[index];
        // Record this choice first, so the swap below finds nothing to swap.
        current[index] = chosen;
        if (other !== -1 && other !== index) {
          current[other] = replaced;
          selects[other].set(replaced);
        }
      });
    });
  });
}

new Clay(clayConfig, customClay);
