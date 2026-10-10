// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in apply_settings() in src/c/main.c.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Phone Voice Commands'
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
        label: 'Theme',
        defaultValue: '0',
        options: [
          { label: 'Dark', value: '0' },
          { label: 'Light', value: '1' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Listening'
      },
      {
        type: 'toggle',
        messageKey: 'AUTO_LISTEN',
        label: 'Start listening when opened',
        description: 'When off, press Select to speak.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'CONFIRM',
        label: 'Confirm before sending',
        description: 'Shows what you said so you can accept or retry it. ' +
          'When off, calls and texts go out as soon as you stop speaking.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'After a command'
      },
      {
        type: 'select',
        messageKey: 'AFTER_SUCCESS',
        label: 'When it works',
        description: 'If a command fails, the app always stays open so you can try again.',
        defaultValue: '0',
        options: [
          { label: 'Stay open', value: '0' },
          { label: 'Close after 2 seconds', value: '1' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'VIBRATE',
        label: 'Vibrate',
        description: 'One pulse when a command works, two when it does not.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
