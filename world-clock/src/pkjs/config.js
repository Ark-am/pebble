// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads the appearance in read_settings() in
// src/c/main.c, and src/pkjs/index.js turns the chosen time zones into
// offsets for the watch.

var ZONES = require('./zones');

var ZONE_OPTIONS = ZONES.map(function (entry) {
  return { label: entry.city + ' (' + entry.zone + ')', value: entry.zone };
});

// Colors for text, as decimal RGB values the watch rounds to its palette.
var TEXT_COLORS = [
  { label: 'Teal', value: '43690' },
  { label: 'Blue', value: '43775' },
  { label: 'Violet', value: '11163135' },
  { label: 'Rose', value: '16711765' },
  { label: 'Red', value: '16711680' },
  { label: 'Orange', value: '16733440' },
  { label: 'Amber', value: '16755200' },
  { label: 'Yellow', value: '16776960' },
  { label: 'Green', value: '43520' },
  { label: 'Black', value: '0' },
  { label: 'White', value: '16777215' }
];

var BACKGROUND_OPTIONS = [
  { label: 'White', value: '16777215' },
  { label: 'Cream', value: '16777130' },
  { label: 'Light grey', value: '11184810' },
  { label: 'Dark grey', value: '5592405' },
  { label: 'Black', value: '0' },
  { label: 'Navy', value: '85' },
  { label: 'Dark green', value: '21760' },
  { label: 'Burgundy', value: '5570560' },
  { label: 'Purple', value: '5570645' }
];

function cityItems(number, defaultZone) {
  return [
    {
      type: 'select',
      messageKey: 'CITY_' + number + '_ZONE',
      label: 'City ' + number + ' time zone',
      defaultValue: defaultZone,
      options: ZONE_OPTIONS
    },
    {
      type: 'input',
      messageKey: 'CITY_' + number + '_NAME',
      label: 'City ' + number + ' name',
      description: 'Leave empty to use the city shown with the time zone.',
      defaultValue: '',
      attributes: { maxlength: 15, placeholder: 'e.g. Home office' }
    }
  ];
}

module.exports = [
  {
    type: 'heading',
    defaultValue: 'World Clock'
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
        type: 'select',
        messageKey: 'BACKGROUND_COLOR',
        label: 'Background colour',
        description: 'Other text is black or white, whichever suits the background.',
        defaultValue: '16777215',
        capabilities: ['COLOR'],
        options: BACKGROUND_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'THEME',
        label: 'Background',
        defaultValue: '0',
        capabilities: ['BW'],
        options: [
          { label: 'Light', value: '0' },
          { label: 'Dark', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TIME_COLOR',
        label: 'Time colour',
        description: 'For your time and the cities\' times.',
        defaultValue: '-1',
        capabilities: ['COLOR'],
        options: [{ label: 'Automatic (black or white)', value: '-1' }].concat(TEXT_COLORS)
      },
      {
        type: 'select',
        messageKey: 'ACCENT_COLOR',
        label: 'Accent colour',
        description: 'For the city names, the day and the divider.',
        defaultValue: '43690',
        capabilities: ['COLOR'],
        options: TEXT_COLORS
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Other times'
      }
    ].concat(cityItems(1, 'Europe/London'), cityItems(2, 'Asia/Tokyo'))
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
