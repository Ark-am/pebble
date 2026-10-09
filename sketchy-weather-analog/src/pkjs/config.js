// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// Font styles; values match the styles in src/c/font_styles.h.
var FONT_OPTIONS = [
  { label: 'Default (the face\'s own fonts)', value: '0' },
  { label: 'Serif (IBM Plex Serif)', value: '1' },
  { label: 'Rounded (Varela Round)', value: '2' },
  { label: 'Mono (DM Mono)', value: '3' }
];

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
        type: 'select',
        messageKey: 'DIAL_STYLE',
        label: 'Dial style',
        description: 'The hands, hour markers, hour numbers and centre ring.',
        defaultValue: '0',
        options: [
          { label: 'Elegant', value: '0' },
          { label: 'Sketchy', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'ELEGANT_HANDS',
        label: 'Elegant hands',
        description: 'Used with the Elegant dial style.',
        defaultValue: '0',
        options: [
          { label: 'Dauphine: slim and faceted', value: '0' },
          { label: 'Sword: a narrow shaded blade', value: '1' },
          { label: 'Breguet: a fine needle through an open ring', value: '2' }
        ]
      },
      {
        type: 'select',
        messageKey: 'SKETCHY_HANDS',
        label: 'Sketchy hands',
        description: 'Used with the Sketchy dial style.',
        defaultValue: '0',
        options: [
          { label: 'Pencil stroke', value: '0' },
          { label: 'Sketched spear', value: '1' },
          { label: 'Sketched arrow', value: '2' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'HOUR_NUMBERS',
        label: 'Show hour numbers',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'NUMBER_FONT',
        label: 'Number font',
        description: 'For the hour numbers. Pebble uses each dial style\'s own numbers.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'INFO_FONT',
        label: 'Information font',
        description: 'For the date, weather, battery and health values in the centre.',
        defaultValue: '0',
        options: FONT_OPTIONS
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
