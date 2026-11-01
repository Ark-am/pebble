// Runs on the settings page (Clay copies it there with toString, so it uses
// nothing from outside itself). It shows each colour picker only while its
// menu is set to Custom.

module.exports = function () {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    ['LINE', 'INFO'].forEach(function (name) {
      var choice = clayConfig.getItemByMessageKey(name + '_COLOUR');
      var picker = clayConfig.getItemByMessageKey(name + '_COLOR_CUSTOM');
      // Black-and-white watches leave these out.
      if (!choice || !picker) {
        return;
      }
      function update() {
        picker[choice.get() === '2' ? 'show' : 'hide']();
      }
      choice.on('change', update);
      update();
    });
  });
};
