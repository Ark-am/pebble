// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

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
    defaultValue: 'Gnomon 2'
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
          { label: 'Light', value: '0' },
          { label: 'Dark', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'NUMERALS',
        label: 'Hour numbers',
        defaultValue: '0',
        options: [
          { label: 'Classic: 12, 2, 4, 6, 8, 10, turned to follow the dial', value: '0' },
          { label: 'Modern: 12, 3, 6, 9, upright', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'NUMBER_FONT',
        label: 'Number font',
        description: 'For the modern hour numbers; the classic ones keep their own style.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'INFO_FONT',
        label: 'Information font',
        description: 'For the date, weather, health information and the name on the dial.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'toggle',
        messageKey: 'SECOND_HAND',
        label: 'Show second hand',
        description: 'Updates every second, which uses more battery.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'SECOND_HAND_COLOR',
        label: 'Second hand colour',
        description: 'Colour watches only.',
        defaultValue: '16711680',
        options: [
          { label: 'Red', value: '16711680' },
          { label: 'Orange', value: '16733440' },
          { label: 'Amber', value: '16755200' },
          { label: 'Green', value: '43520' },
          { label: 'Teal', value: '43690' },
          { label: 'Blue', value: '22015' },
          { label: 'Violet', value: '11163135' },
          { label: 'Rose', value: '16711765' },
          { label: 'Same as the text (black or white)', value: '-1' }
        ]
      },
      {
        type: 'input',
        messageKey: 'DIAL_NAME',
        label: 'Name on the dial',
        description: 'Shown below the 12. Leave empty to show no name.',
        defaultValue: 'Pebble',
        attributes: {
          maxlength: 20
        }
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
        defaultValue: 'Choose what appears around the centre of the dial.'
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_WEATHER',
        label: 'Weather',
        description: 'Temperature and an icon for the conditions, upper left.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_HEALTH',
        label: 'Health',
        description: 'Steps and other health data, upper right. Tap the watch to cycle ' +
          'through them. Not available on the original Pebble.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_DATE',
        label: 'Date',
        description: 'Day of the week and day of the month, lower left.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_BATTERY',
        label: 'Battery and connection',
        description: 'Battery ring with the phone connection and Quiet Time, lower right.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'AVOID_HANDS',
        label: 'Move information clear of the hands',
        description: 'Each item slides around the centre when a hand would cover it. ' +
          'When there is no room, it stays where it is.',
        defaultValue: false
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
