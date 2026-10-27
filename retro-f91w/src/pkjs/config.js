// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Retro F-91W'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Colours'
      },
      {
        type: 'select',
        messageKey: 'LCD_COLOUR',
        label: 'LCD',
        description: 'On black-and-white watches, every choice but Dark shows a white LCD.',
        defaultValue: '0',
        options: [
          { label: 'Classic grey', value: '0' },
          { label: 'Backlit green', value: '1' },
          { label: 'Blue', value: '2' },
          { label: 'Amber', value: '3' },
          { label: 'Dark, with light segments', value: '4' }
        ]
      },
      {
        type: 'select',
        messageKey: 'CASE_COLOUR',
        label: 'Case',
        description: 'Colour watches only.',
        defaultValue: '0',
        options: [
          { label: 'Black, with a blue frame', value: '0' },
          { label: 'Gold', value: '1' },
          { label: 'Silver', value: '2' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Printed text'
      },
      {
        type: 'input',
        messageKey: 'BRAND',
        label: 'Brand',
        defaultValue: 'PEBBLE',
        attributes: { maxlength: 12 }
      },
      {
        type: 'input',
        messageKey: 'MODEL',
        label: 'Model, beside the brand',
        description: 'Leave empty for none.',
        defaultValue: 'F-91W',
        attributes: { maxlength: 12 }
      },
      {
        type: 'input',
        messageKey: 'TOP_LABEL',
        label: 'Label above the LCD',
        description: 'Leave empty for none.',
        defaultValue: 'ALARM CHRONOGRAPH',
        attributes: { maxlength: 30 }
      },
      {
        type: 'input',
        messageKey: 'BOTTOM_LABEL',
        label: 'Label at the bottom',
        description: 'With three words, the middle one sits in a red box, as on the ' +
          'original. Leave empty for none.',
        defaultValue: 'WATER WR RESIST',
        attributes: { maxlength: 30 }
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Display'
      },
      {
        type: 'select',
        messageKey: 'TOP_ROW',
        label: 'Top row',
        defaultValue: '0',
        options: [
          { label: 'Day and date', value: '0' },
          { label: 'Weather', value: '1' },
          { label: 'Steps', value: '2' },
          { label: 'Heart rate', value: '3' },
          { label: 'Each in turn: shake the watch to change', value: '4' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TIME_FORMAT',
        label: 'Time format',
        defaultValue: '0',
        options: [
          { label: 'Same as the watch', value: '0' },
          { label: '12-hour', value: '1' },
          { label: '24-hour', value: '2' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'SECONDS',
        label: 'Show seconds',
        description: 'Updates every second, which uses more battery.',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'CHIME',
        label: 'Hourly chime',
        description: 'A short vibration on the hour, except during Quiet Time.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'TEMPERATURE_UNIT',
        label: 'Temperature',
        defaultValue: 'C',
        options: [
          { label: 'Celsius', value: 'C' },
          { label: 'Fahrenheit', value: 'F' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
