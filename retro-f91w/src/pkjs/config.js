// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

// What can show in place of the model name or a word of the bottom label;
// values match the
// Widget enum in src/c/main.c.
var WIDGETS = [
          { label: 'The printed text', value: '0' },
          { label: 'Weather', value: '1' },
          { label: 'Steps', value: '2' },
          { label: 'Heart rate', value: '3' },
          { label: 'Battery', value: '4' }
        ];

// A colour picker for one part of a custom case or LCD; the number matches
// ColourPart in src/c/main.c. src/pkjs/custom-clay.js shows each only when it
// applies.
function colour(part, label, defaultValue) {
  return {
    type: 'color',
    messageKey: 'COLOURS[' + part + ']',
    label: label,
    defaultValue: defaultValue,
    capabilities: ['COLOR']
  };
}

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
          { label: 'Classic pale grey-green', value: '0' },
          { label: 'Classic grey', value: '5' },
          { label: 'Backlit green', value: '1' },
          { label: 'Blue', value: '2' },
          { label: 'Amber', value: '3' },
          { label: 'Dark, with light segments', value: '4' },
          { label: 'Custom', value: '6' }
        ]
      },
      colour(7, 'LCD', 0xAAFFAA),
      colour(8, 'Digits', 0x000000),
      colour(14, 'Top row, PM and chime sign', 0x000000),
      {
        type: 'select',
        messageKey: 'CASE_COLOUR',
        label: 'Case',
        description: 'Colour watches only.',
        defaultValue: '0',
        options: [
          { label: 'Black, with a blue frame', value: '0' },
          { label: 'Gold', value: '1' },
          { label: 'Silver', value: '2' },
          { label: 'Custom', value: '3' }
        ]
      },
      colour(0, 'Case', 0x000000),
      colour(15, 'Around the case', 0x000000),
      colour(1, 'Printed text', 0xFFFFFF),
      colour(9, 'Button labels', 0xFFFFFF),
      colour(10, 'Bottom label', 0xFFFFFF),
      colour(4, 'Model name', 0xAAAA00),
      colour(2, 'Band, lines and box', 0x0000FF),
      colour(11, 'Lines', 0x0000FF),
      colour(12, 'Box round the middle word', 0x0000FF),
      colour(3, 'Thin line inside the band', 0xAAAAAA),
      colour(5, 'Boxed word and arrows', 0xAA0000),
      colour(13, 'Arrows', 0xAA0000),
      colour(6, 'LCD border', 0x555555),
      {
        type: 'toggle',
        messageKey: 'SPLIT_COLOURS',
        label: 'Separate colour for every part',
        description: 'Gives the brand, labels, lines, box, arrows and the LCD\'s top row ' +
          'colours of their own.',
        defaultValue: false,
        capabilities: ['COLOR']
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
        messageKey: 'LIGHT_LABEL',
        label: 'By the Back button (top left)',
        description: 'Leave empty for none.',
        defaultValue: 'LIGHT',
        attributes: { maxlength: 14 }
      },
      {
        type: 'input',
        messageKey: 'UP_LABEL',
        label: 'By the Up button (top right)',
        description: 'Leave empty for none.',
        defaultValue: 'MODE',
        attributes: { maxlength: 14 }
      },
      {
        type: 'input',
        messageKey: 'DOWN_LABEL',
        label: 'By the Down button (bottom right)',
        description: 'Leave empty for none.',
        defaultValue: 'ALARM ON/OFF',
        attributes: { maxlength: 22 }
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
        messageKey: 'MODEL_WIDGET',
        label: 'Beside the brand (F-91W)',
        description: 'Show information in place of the model name.',
        defaultValue: '0',
        options: WIDGETS
      },
      {
        type: 'select',
        messageKey: 'BOTTOM_LEFT',
        label: 'Bottom left (WATER)',
        description: 'Show information in place of the first word of the bottom label.',
        defaultValue: '0',
        options: WIDGETS
      },
      {
        type: 'select',
        messageKey: 'BOTTOM_RIGHT',
        label: 'Bottom right (RESIST)',
        description: 'Show information in place of the last word of the bottom label.',
        defaultValue: '0',
        options: WIDGETS
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
