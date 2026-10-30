// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// Font styles; values match the styles in src/c/font_styles.h.
var FONT_OPTIONS = [
  { label: 'Sketchy (Patrick Hand)', value: '4' },
  { label: 'Pebble (the watch\'s own Gothic)', value: '0' },
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

// What the line under the values can show: the name, or one of the items.
// The name's value matches BOTTOM_NAME on the watch.
var BOTTOM_OPTIONS = [{ label: 'The name (PEBBLE)', value: '6' }].concat(ITEM_OPTIONS);

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
    },
    {
      type: 'select',
      messageKey: 'SCREEN_' + number + '_BOTTOM',
      label: 'Screen ' + number + ': bottom line',
      defaultValue: '6',
      options: BOTTOM_OPTIONS
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
        type: 'input',
        messageKey: 'DIAL_NAME',
        label: 'Name under the information',
        description: 'Shown on the bottom line of each screen set to The name. Leave ' +
          'empty to show no name.',
        defaultValue: 'PEBBLE',
        attributes: {
          maxlength: 20
        }
      },
      {
        type: 'select',
        messageKey: 'MINUTE_STYLE',
        label: 'Minutes',
        description: 'All but the hand use the current hour\'s colour.',
        defaultValue: '0',
        options: [
          { label: 'Hand', value: '0' },
          { label: 'Highlighted tick', value: '1' },
          { label: 'Minute number', value: '2' },
          { label: 'Progress arc', value: '3' },
          { label: 'Sketched bead', value: '4' },
          { label: 'Edge pointer', value: '5' },
          { label: 'Progress arc and bead', value: '6' },
          { label: 'Minute number and tick', value: '7' }
        ]
      },
      {
        type: 'select',
        messageKey: 'SKETCHY_HANDS',
        label: 'Minute hand',
        description: 'Used when the minutes show as a hand.',
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
        description: 'The current hour\'s number always shows, larger, in place of an hour hand.',
        defaultValue: false
      },
      {
        type: 'color',
        messageKey: 'HOUR_COLOR',
        label: 'Current hour colour',
        defaultValue: 'FF0000',
        sunlight: false,
        capabilities: ['COLOR']
      },
      {
        type: 'select',
        messageKey: 'NUMBER_FONT',
        label: 'Number font',
        description: 'For the hour numbers.',
        defaultValue: '4',
        options: FONT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'INFO_FONT',
        label: 'Information font',
        description: 'For the date, weather, battery and health values in the centre.',
        defaultValue: '4',
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
          'item on each screen also sets the picture, and the bottom line, under the ' +
          'values, shows the name or a third value. Set the first two to Nothing to ' +
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
