// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in inbox_received() in src/c/main.c.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Minimalist'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Appearance'
      },
      {
        type: 'radiogroup',
        messageKey: 'STYLE',
        label: 'Style',
        defaultValue: '0',
        options: [
          { label: 'Analog', value: '0' },
          { label: 'Digital', value: '1' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'THEME',
        label: 'Background',
        defaultValue: '0',
        options: [
          { label: 'Dark', value: '0' },
          { label: 'Light', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'FONT_STYLE',
        label: 'Font',
        description: 'For all the text on the face. The wider fonts make the digital time smaller, so that it still fits.',
        defaultValue: '0',
        options: [
          // Values match FONT_STYLE_* in src/c/font_styles.h.
          { label: 'Condensed (Antonio)', value: '0' },
          { label: 'Serif (IBM Plex Serif)', value: '1' },
          { label: 'Rounded (Varela Round)', value: '2' },
          { label: 'Mono (DM Mono)', value: '3' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'SHOW_DATE',
        label: 'Show the date',
        defaultValue: true
      },
      {
        type: 'input',
        messageKey: 'BRAND',
        label: 'Name',
        description: 'Shown above the centre on the analog face and above the date on the digital face. Leave it empty to hide it.',
        defaultValue: 'PEBBLE',
        attributes: { maxlength: 16, placeholder: 'e.g. PEBBLE' }
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Analog'
      },
      {
        type: 'select',
        messageKey: 'HAND_STYLE',
        label: 'Hands',
        defaultValue: '0',
        options: [
          // Values match HANDS_* in src/c/main.c.
          { label: 'Baton (plain lines)', value: '0' },
          { label: 'Dauphine (tapered)', value: '1' },
          { label: 'Skeleton (open outline)', value: '2' },
          { label: 'Lollipop (ring at the tip)', value: '3' },
          { label: 'Leaf (widest in the middle)', value: '4' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'NUMBERS',
        label: 'At 12, 3, 6 and 9',
        defaultValue: '0',
        options: [
          { label: 'Long marks', value: '0' },
          { label: 'Numbers', value: '1' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Digital'
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
        messageKey: 'LEADING_ZERO',
        label: 'Leading zero on the hour',
        description: 'Shows 09 rather than 9. Without it the first place is left blank, as on an LCD.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
