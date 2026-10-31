// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in inbox_received() in src/c/main.c.

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Alien Core'
  },
  {
    type: 'section',
    items: [
      {
        type: 'select',
        messageKey: 'STYLE',
        label: 'Time',
        description: 'Glowing digits or hands in the core.',
        defaultValue: '0',
        options: [
          { label: 'Digital', value: '0' },
          { label: 'Analog', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TIME_FORMAT',
        label: 'Time format',
        description: 'For digital time. 12-hour time shows AM or PM beside the minutes.',
        defaultValue: '0',
        options: [
          { label: 'Same as the watch', value: '0' },
          { label: '12-hour', value: '1' },
          { label: '24-hour', value: '2' }
        ]
      },
      {
        type: 'select',
        messageKey: 'GLOW_COLOR',
        label: 'Glow colour',
        defaultValue: '65280',
        capabilities: ['COLOR'],
        options: [
          { label: 'Alien green', value: '65280' },
          { label: 'Lime', value: '11206400' },
          { label: 'Cyan', value: '65535' },
          { label: 'Blue', value: '22015' },
          { label: 'Purple', value: '11141375' },
          { label: 'Red', value: '16711680' },
          { label: 'Orange', value: '16733440' },
          { label: 'Yellow', value: '16776960' }
        ]
      },
      {
        type: 'input',
        messageKey: 'NAME',
        label: 'Name in the core',
        description: 'Leave empty to show no name.',
        defaultValue: 'PEBBLE',
        attributes: {
          maxlength: 20
        }
      },
      {
        type: 'select',
        messageKey: 'NAME_SIZE',
        label: 'Name size',
        defaultValue: '1',
        options: [
          { label: 'Small', value: '0' },
          { label: 'Medium', value: '1' },
          { label: 'Large', value: '2' }
        ]
      },
      {
        type: 'select',
        messageKey: 'NAME_FONT',
        label: 'Name font',
        defaultValue: '0',
        options: [
          { label: 'Pebble (the watch\'s own Gothic)', value: '0' },
          { label: 'Serif (IBM Plex Serif)', value: '1' },
          { label: 'Rounded (Varela Round)', value: '2' },
          { label: 'Mono (DM Mono)', value: '3' },
          { label: 'Sketchy (Patrick Hand)', value: '4' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'ANIMATION',
        label: 'Avatars and animations',
        description: 'Flick your wrist, or tap on touch watches, to choose one of twelve ' +
          'avatars and transform into it. The face also powers up when it appears, and a ' +
          'light runs round the ring on the hour. Touch uses a little more battery.',
        defaultValue: true
      }
    ]
  },
  {
    type: 'section',
    // Only shown on watches with a speaker.
    capabilities: ['SPEAKER'],
    items: [
      {
        type: 'heading',
        defaultValue: 'Sound'
      },
      {
        type: 'select',
        messageKey: 'SOUND',
        label: 'Sound effects',
        description: 'Silent during Quiet Time, and when the watch is muted.',
        defaultValue: '1',
        options: [
          { label: 'Off', value: '0' },
          { label: 'Avatars only', value: '1' },
          { label: 'Avatars, power-up and on the hour', value: '2' }
        ]
      },
      {
        type: 'select',
        messageKey: 'SOUND_VOLUME',
        label: 'Volume',
        defaultValue: '60',
        options: [
          { label: 'Quiet', value: '30' },
          { label: 'Medium', value: '60' },
          { label: 'Loud', value: '100' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
