// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Epoch'
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
