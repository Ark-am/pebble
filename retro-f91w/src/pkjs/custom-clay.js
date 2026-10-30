// Runs on the settings page (Clay copies it there with toString, so it uses
// nothing from outside itself). It shows the chime's settings only while the
// chime is on, and the colour pickers only for a custom case or LCD, adds the
// separate ones when asked, and starts a custom set from the preset that was
// showing.

module.exports = function () {
  var clayConfig = this;
  var CASE_CUSTOM = '3';
  var LCD_CUSTOM = '6';

  // Pickers by ColourPart in src/c/main.c.
  var CASE_PARTS = [0, 1, 2, 3, 4, 5, 6];
  var LCD_PARTS = [7, 8];
  var CASE_SEPARATE = [9, 10, 11, 12, 13, 15];
  var LCD_SEPARATE = [14];
  // Which shared colour each separate one starts from; -1 is black.
  var STARTS_FROM = { 9: 1, 10: 1, 11: 2, 12: 2, 13: 5, 14: 8, 15: -1 };
  // Shared pickers that cover fewer parts once the others are separate.
  var LABELS = {
    1: ['Printed text', 'Brand'],
    2: ['Band, lines and box', 'Band'],
    5: ['Boxed word and arrows', 'Boxed word'],
    8: ['Digits', 'Time']
  };

  // The presets' colours, as in apply_palette() in src/c/main.c, in the order
  // of CASE_PARTS and LCD_PARTS.
  var CASE_PRESETS = {
    0: [0x000000, 0xFFFFFF, 0x0000FF, 0xAAAAAA, 0xAAAA00, 0xAA0000, 0x555555],
    1: [0xFFAA00, 0x000000, 0x000000, 0x000000, 0x000000, 0xAA0000, 0x000000],
    2: [0xAAAAAA, 0x000000, 0x0000AA, 0x555555, 0x0000AA, 0xAA0000, 0x555555]
  };
  var LCD_PRESETS = {
    0: [0xAAFFAA, 0x000000],
    1: [0xAAFF00, 0x000000],
    2: [0xAAFFFF, 0x000055],
    3: [0xFFAA55, 0x000000],
    4: [0x000000, 0xFFFFFF],
    5: [0xAAAAAA, 0x000000]
  };

  function picker(part) {
    return clayConfig.getItemByMessageKey('COLOURS[' + part + ']');
  }

  function showIf(parts, shown) {
    parts.forEach(function (part) {
      var item = picker(part);
      if (item) {
        item[shown ? 'show' : 'hide']();
      }
    });
  }

  function setAll(parts, colours) {
    parts.forEach(function (part, i) {
      picker(part).set(colours[i]);
    });
  }

  // Separate colours start out as the shared colour they split from.
  function startSeparate(parts) {
    parts.forEach(function (part) {
      var from = STARTS_FROM[part];
      picker(part).set(from < 0 ? 0x000000 : picker(from).get());
    });
  }

  // The chime's settings show only while it is on, and the custom pattern
  // only when chosen.
  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var chime = clayConfig.getItemByMessageKey('CHIME');
    var pattern = clayConfig.getItemByMessageKey('CHIME_PATTERN');
    var custom = clayConfig.getItemByMessageKey('CHIME_CUSTOM');
    var others = ['CHIME_PATTERN', 'CHIME_EVERY', 'CHIME_FROM', 'CHIME_UNTIL'];

    function update() {
      var on = chime.get();
      others.forEach(function (key) {
        clayConfig.getItemByMessageKey(key)[on ? 'show' : 'hide']();
      });
      custom[on && pattern.get() === '2' ? 'show' : 'hide']();
    }

    chime.on('change', update);
    pattern.on('change', update);
    update();
  });

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var lcd = clayConfig.getItemByMessageKey('LCD_COLOUR');
    var watchCase = clayConfig.getItemByMessageKey('CASE_COLOUR');
    var separate = clayConfig.getItemByMessageKey('SPLIT_COLOURS');
    if (!separate) {
      // A black-and-white watch, which leaves out the colour items: no custom
      // LCD either.
      var option = lcd.$manipulatorTarget.select('option[value="' + LCD_CUSTOM + '"]')[0];
      if (option) {
        option.parentNode.removeChild(option);
      }
      return;
    }

    function update() {
      var customCase = watchCase.get() === CASE_CUSTOM;
      var customLcd = lcd.get() === LCD_CUSTOM;
      var split = separate.get();
      showIf(CASE_PARTS, customCase);
      showIf(CASE_SEPARATE, customCase && split);
      showIf(LCD_PARTS, customLcd);
      showIf(LCD_SEPARATE, customLcd && split);
      separate[customCase || customLcd ? 'show' : 'hide']();
      Object.keys(LABELS).forEach(function (part) {
        picker(part).$element.select('.label').set('innerHTML', LABELS[part][split ? 1 : 0]);
      });
    }

    var lastCase = watchCase.get();
    watchCase.on('change', function () {
      if (watchCase.get() === CASE_CUSTOM && CASE_PRESETS[lastCase]) {
        setAll(CASE_PARTS, CASE_PRESETS[lastCase]);
        startSeparate(CASE_SEPARATE);
      }
      lastCase = watchCase.get();
      update();
    });

    var lastLcd = lcd.get();
    lcd.on('change', function () {
      if (lcd.get() === LCD_CUSTOM && LCD_PRESETS[lastLcd]) {
        setAll(LCD_PARTS, LCD_PRESETS[lastLcd]);
        startSeparate(LCD_SEPARATE);
      }
      lastLcd = lcd.get();
      update();
    });

    separate.on('change', function () {
      if (separate.get()) {
        startSeparate(CASE_SEPARATE);
        startSeparate(LCD_SEPARATE);
      }
      update();
    });

    update();
  });
};
