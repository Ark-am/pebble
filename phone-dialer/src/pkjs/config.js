// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in apply_settings() in src/c/main.c.

// Values match the MainRow enum on the watch.
var MENU_OPTIONS = [
  { label: 'Dialer', value: '0' },
  { label: 'Recent calls', value: '1' },
  { label: 'Favorites', value: '2' },
  { label: 'Contacts', value: '3' }
];

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Phone Dialer'
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
        defaultValue: 'Main menu order'
      },
      {
        type: 'text',
        defaultValue: 'Choose what appears first, second, third and fourth. ' +
          'Picking an item that is already in another place swaps the two.'
      },
      {
        type: 'select',
        messageKey: 'MENU_1',
        label: 'First',
        defaultValue: '0',
        options: MENU_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'MENU_2',
        label: 'Second',
        defaultValue: '1',
        options: MENU_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'MENU_3',
        label: 'Third',
        defaultValue: '2',
        options: MENU_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'MENU_4',
        label: 'Fourth',
        defaultValue: '3',
        options: MENU_OPTIONS
      }
    ]
  },
  {
    type: 'section',
    // Only shown for watches with a touchscreen.
    capabilities: ['TOUCH'],
    items: [
      {
        type: 'heading',
        defaultValue: 'Touch'
      },
      {
        type: 'toggle',
        messageKey: 'TOUCH_ENABLED',
        label: 'Touch input',
        description: 'Tap keys, rows and the Call button, and swipe through lists. ' +
          'When off, use the buttons only.',
        defaultValue: true
      },
      {
        type: 'toggle',
        messageKey: 'TOUCH_VIBE',
        label: 'Vibrate on tap',
        description: 'A short tick when you tap a keypad key or the Call button.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
