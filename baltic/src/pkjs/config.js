// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// Font styles; values match the styles in src/c/font_styles.h.
var FONT_OPTIONS = [
  { label: 'Default (Baltic\'s own fonts)', value: '0' },
  { label: 'Serif (IBM Plex Serif)', value: '1' },
  { label: 'Rounded (Varela Round)', value: '2' },
  { label: 'Mono (DM Mono)', value: '3' }
];

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Baltic'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Appearance'
      },
      {
        // Values match the Dial enum on the watch.
        type: 'select',
        messageKey: 'DIAL_COLOR',
        label: 'Dial',
        description: 'Black-and-white watches show the light dials as white and the dark ' +
          'dials as black.',
        defaultValue: '0',
        options: [
          { label: 'Salmon', value: '0' },
          { label: 'Navy blue', value: '1' },
          { label: 'Silver', value: '2' },
          { label: 'Black and gold', value: '3' },
          { label: 'Azure blue', value: '4' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'SECONDS',
        label: 'Running seconds',
        description: 'Shows a hand on the small seconds dial. It updates every second, ' +
          'which uses more battery.',
        defaultValue: true
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
        description: 'For the name on the dial and the small dial at half past four.',
        defaultValue: '0',
        options: FONT_OPTIONS
      },
      {
        type: 'input',
        messageKey: 'DIAL_NAME',
        label: 'Name on the dial',
        description: 'Shown below the 12. Leave empty to show no name.',
        defaultValue: 'BALTIC',
        attributes: {
          maxlength: 12
        }
      },
      {
        // Values match the Info enum on the watch.
        type: 'select',
        messageKey: 'INFO',
        label: 'Small dial at half past four',
        description: 'Health data is not available on the original Pebble, which shows the ' +
          'date instead. Tap the watch to cycle through the health data.',
        defaultValue: '0',
        options: [
          { label: 'Nothing', value: '0' },
          { label: 'Date', value: '1' },
          { label: 'Weather', value: '2' },
          { label: 'Battery', value: '3' },
          { label: 'Health (steps, distance, sleep and more)', value: '4' }
        ]
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
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
