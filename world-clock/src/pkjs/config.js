// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads the appearance in read_settings() in
// src/c/main.c, and src/pkjs/index.js turns the chosen time zones into
// offsets for the watch.

var ZONES = require('./zones');

var ZONE_OPTIONS = ZONES.map(function (entry) {
  return { label: entry.city + ' (' + entry.zone + ')', value: entry.zone };
});

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
        messageKey: 'ACCENT_COLOR',
        label: 'Accent colour',
        description: 'Used for the city names and the divider. Colour watches only.',
        defaultValue: '43690',
        options: [
          { label: 'Teal', value: '43690' },
          { label: 'Blue', value: '43775' },
          { label: 'Violet', value: '11163135' },
          { label: 'Rose', value: '16711765' },
          { label: 'Red', value: '16711680' },
          { label: 'Orange', value: '16733440' },
          { label: 'Amber', value: '16755200' },
          { label: 'Green', value: '43520' }
        ]
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
