// Settings page, rendered on the phone by Clay. Each messageKey matches one in
// package.json; the watch reads them in inbox_received() in src/c/main.c.

// What can show above and below the time; values match the Slot enum in
// src/c/main.c.
var SLOT_OPTIONS = [
  { label: 'Name', value: '0' },
  { label: 'Date', value: '1' },
  { label: 'Weather', value: '2' },
  { label: 'Steps', value: '3' },
  { label: 'Heart rate', value: '4' },
  { label: 'Battery', value: '5' },
  { label: 'Nothing', value: '6' }
];

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
        type: 'select',
        messageKey: 'TOP_SLOT',
        label: 'Above the time',
        description: 'Uses the name size and font below.',
        defaultValue: '0',
        options: SLOT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'BOTTOM_SLOT',
        label: 'Below the time',
        defaultValue: '1',
        options: SLOT_OPTIONS
      },
      {
        type: 'select',
        messageKey: 'TEMPERATURE_UNIT',
        label: 'Temperature',
        defaultValue: 'C',
        options: [
          { label: 'Celsius', value: 'C' },
          { label: 'Fahrenheit', value: 'F' }
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
        description: 'Long press the screen, or flick your wrist on watches without a ' +
          'touchscreen, to choose one of twelve avatars and transform into it. The face ' +
          'also powers up when it appears, and a light runs round the ring on the hour. ' +
          'Touch uses a little more battery.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'SELECT_TIME',
        label: 'Time to choose',
        description: 'How long the selector waits before transforming into the avatar ' +
          'showing. Each swipe, drag or flick starts it again.',
        defaultValue: '3',
        options: [
          { label: '3 seconds', value: '3' },
          { label: '5 seconds', value: '5' },
          { label: '10 seconds', value: '10' },
          { label: '15 seconds', value: '15' },
          { label: '30 seconds', value: '30' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'KEEP_LIGHT',
        label: 'Keep the light on',
        description: 'While you choose an avatar and until the time comes back. Uses more ' +
          'battery while it is on.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'LONG_PRESS',
        label: 'Long press',
        description: 'How long to hold the screen to open the avatars.',
        defaultValue: '10',
        capabilities: ['TOUCH'],
        options: [
          { label: '0.5 seconds', value: '5' },
          { label: '1 second', value: '10' },
          { label: '1.5 seconds', value: '15' },
          { label: '2 seconds', value: '20' },
          { label: '3 seconds', value: '30' }
        ]
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
