// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// Values match the Item enum on the watch.
var ITEM_OPTIONS = [
  { label: 'Weather', value: '1' },
  { label: 'Date', value: '2' },
  { label: 'Battery', value: '3' },
  { label: 'Steps', value: '4' },
  { label: 'Heart rate', value: '5' },
  { label: 'Nothing', value: '0' }
];

// The four widget screens and what each shows by default.
var SCREEN_DEFAULTS = [['1', '2'], ['2', '3'], ['4', '5'], ['0', '0']];

function screenItems(number) {
  var defaults = SCREEN_DEFAULTS[number - 1];
  return [
    {
      type: 'select',
      messageKey: 'SCREEN_' + number + '_MAIN',
      label: 'Screen ' + number + ': picture and first value',
      defaultValue: defaults[0],
      options: ITEM_OPTIONS
    },
    {
      type: 'select',
      messageKey: 'SCREEN_' + number + '_SECOND',
      label: 'Screen ' + number + ': second value',
      defaultValue: defaults[1],
      options: ITEM_OPTIONS
    }
  ];
}

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Sketchy Weather Analog'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Appearance'
      },
      {
        type: 'select',
        messageKey: 'THEME',
        label: 'Background',
        defaultValue: '0',
        options: [
          { label: 'Paper (light)', value: '0' },
          { label: 'Chalkboard (dark)', value: '1' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'HOUR_NUMBERS',
        label: 'Show hour numbers',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Centre circle'
      },
      {
        type: 'text',
        defaultValue: 'Choose up to four screens for the centre of the dial. The first ' +
          'item on each screen also sets the picture. Set both items to Nothing to ' +
          'skip a screen. Tap the watch to move to the next screen. Steps and heart ' +
          'rate show -- on watches without them.'
      },
      {
        type: 'toggle',
        messageKey: 'ROTATE_SCREENS',
        label: 'Change screen every minute',
        defaultValue: false
      }
    ].concat(screenItems(1), screenItems(2), screenItems(3), screenItems(4))
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Weather and alerts'
      },
      {
        type: 'select',
        messageKey: 'TEMPERATURE_UNIT',
        label: 'Temperature',
        defaultValue: 'C',
        options: [
          { label: 'Celsius (°C)', value: 'C' },
          { label: 'Fahrenheit (°F)', value: 'F' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'DISCONNECT_VIBE',
        label: 'Vibrate when the phone disconnects',
        description: 'Never vibrates during Quiet Time.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
