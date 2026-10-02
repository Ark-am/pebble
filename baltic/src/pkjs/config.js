// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in read_settings() in src/c/main.c.

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
        defaultValue: false
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
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
