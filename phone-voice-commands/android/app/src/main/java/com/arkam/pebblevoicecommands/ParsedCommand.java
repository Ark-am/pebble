package com.arkam.pebblevoicecommands;

/** A recognised command and its argument, before anything is carried out. */
final class ParsedCommand {
    enum Action {
        HELP,
        FLASHLIGHT_ON,
        FLASHLIGHT_OFF,
        FLASHLIGHT_TOGGLE,
        /** {@link #value}: seconds, or 0 when no length was given. */
        TIMER,
        /** {@link #value}: minutes after midnight, or -1 when no time was given. */
        ALARM,
        VOLUME_UP,
        VOLUME_DOWN,
        /** {@link #value}: percent, 0 to 100. */
        VOLUME_SET,
        VOLUME_MUTE,
        VOLUME_UNMUTE,
        MEDIA_PLAY,
        MEDIA_PAUSE,
        MEDIA_NEXT,
        MEDIA_PREVIOUS,
        /** {@link #text}: the app name as spoken. */
        OPEN_APP,
    }

    final Action action;
    final int value;
    final String text;

    ParsedCommand(Action action, int value, String text) {
        this.action = action;
        this.value = value;
        this.text = text;
    }

    ParsedCommand(Action action) {
        this(action, 0, null);
    }

    @Override
    public String toString() {
        return action + (text != null ? "(" + text + ")" : "(" + value + ")");
    }
}
