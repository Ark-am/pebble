// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

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
        messageKey: 'BATTERY',
        label: 'Battery',
        defaultValue: '0',
        options: [
          { label: 'Show when low or charging', value: '0' },
          { label: 'Always show', value: '1' }
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
