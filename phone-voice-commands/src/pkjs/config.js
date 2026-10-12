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
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Quick commands'
      },
      {
        type: 'text',
        defaultValue: 'Press Up or Down on the watch to pick one of these instead of speaking. ' +
          'Write each one the way you would say it, such as "Call Mom" or "Navigate home". ' +
          'It runs as soon as you choose it, without the confirmation screen. ' +
          'Use Move up to change the order the watch lists them in.'
      },
      {
        type: 'input',
        messageKey: 'QUICK_1',
        label: 'Command 1',
        defaultValue: 'Flashlight on',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'input',
        messageKey: 'QUICK_2',
        label: 'Command 2',
        defaultValue: 'Flashlight off',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'button',
        id: 'QUICK_UP_2',
        defaultValue: '\u25B2 Move up'
      },
      {
        type: 'input',
        messageKey: 'QUICK_3',
        label: 'Command 3',
        defaultValue: 'Set a timer for 5 minutes',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'button',
        id: 'QUICK_UP_3',
        defaultValue: '\u25B2 Move up'
      },
      {
        type: 'input',
        messageKey: 'QUICK_4',
        label: 'Command 4',
        defaultValue: 'Pause',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'button',
        id: 'QUICK_UP_4',
        defaultValue: '\u25B2 Move up'
      },
      {
        type: 'input',
        messageKey: 'QUICK_5',
        label: 'Command 5',
        defaultValue: 'Next song',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'button',
        id: 'QUICK_UP_5',
        defaultValue: '\u25B2 Move up'
      },
      {
        type: 'input',
        messageKey: 'QUICK_6',
        label: 'Command 6',
        defaultValue: '',
        attributes: {
          placeholder: 'Empty: not shown',
          maxlength: 63
        }
      },
      {
        type: 'button',
        id: 'QUICK_UP_6',
        defaultValue: '\u25B2 Move up'
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
