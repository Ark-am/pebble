// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// Values match the SlotKind enum on the watch.
var SLOT_OPTIONS = [
  { label: 'Weather', value: '1' },
  { label: 'Date', value: '2' },
  { label: 'Battery and alerts', value: '3' },
  { label: 'Health (tap the watch to cycle)', value: '4' },
  { label: 'Nothing', value: '0' }
];

// Font styles; values match the styles in src/c/font_styles.h.
var FONT_OPTIONS = [
  { label: 'Default (the face\'s own fonts)', value: '0' },
  { label: 'Serif (IBM Plex Serif)', value: '1' },
  { label: 'Rounded (Varela Round)', value: '2' },
  { label: 'Mono (DM Mono)', value: '3' }
];

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Meridian'
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
          { label: 'Dark', value: '0' },
          { label: 'Light', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'ACCENT_COLOR',
        label: 'Accent colour',
        description: 'Used for the date, the 12 o\'clock marker and the second hand. ' +
          'Colour watches only.',
        defaultValue: '16755200',
        options: [
          { label: 'Amber', value: '16755200' },
          { label: 'Orange', value: '16733440' },
          { label: 'Red', value: '16711680' },
          { label: 'Rose', value: '16711765' },
          { label: 'Violet', value: '11163135' },
          { label: 'Blue', value: '43775' },
          { label: 'Teal', value: '43690' },
          { label: 'Green', value: '43520' }
        ]
      },
      {
        type: 'select',
        messageKey: 'NUMBER_FONT',
        label: 'Number font',
        description: 'For the hour numbers.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'INFO_FONT',
        label: 'Information font',
        description: 'For the weather, date, battery and health information.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'toggle',
        messageKey: 'HOUR_NUMBERS',
        label: 'Show hour numbers',
        description: 'Numbers are left out where information is shown.',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'SECOND_HAND',
        label: 'Show second hand',
        description: 'Updates every second, which uses more battery.',
        defaultValue: false
      },
      {
        type: 'input',
        messageKey: 'DIAL_NAME',
        label: 'Name on the dial',
        description: 'Shown below the 12 o\'clock marker. Leave empty to show no name.',
        defaultValue: 'PEBBLE',
        attributes: {
          maxlength: 20
        }
      },
      {
        type: 'slider',
        messageKey: 'ROTATION',
        label: 'Rotate the dial',
        description: 'Turns the whole face clockwise by this many degrees, for wearing ' +
          'the watch at an angle. Negative values turn it anticlockwise. On round ' +
          'watches everything turns, the numbers and information included; on ' +
          'rectangular watches the text stays upright.',
        defaultValue: 0,
        min: -180,
        max: 180,
        step: 5
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Information'
      },
      {
        type: 'text',
        defaultValue: 'Choose what appears at each position on the dial.'
      },
      {
        type: 'select',
        messageKey: 'SLOT_TOP',
        label: '12 o\'clock',
        defaultValue: '1',
        options: SLOT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'SLOT_RIGHT',
        label: '3 o\'clock',
        defaultValue: '2',
        options: SLOT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'SLOT_BOTTOM',
        label: '6 o\'clock',
        defaultValue: '4',
        options: SLOT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'SLOT_LEFT',
        label: '9 o\'clock',
        defaultValue: '3',
        options: SLOT_OPTIONS
      }
    ]
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
