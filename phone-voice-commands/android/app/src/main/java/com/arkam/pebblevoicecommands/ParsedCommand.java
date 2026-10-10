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
        /** {@link #text}: the contact name; {@link #value}: a Contact.KIND_* number kind. */
        CALL_CONTACT,
        /** {@link #text}: the digits to dial, with any leading "+". */
        CALL_NUMBER,
        /** {@link #text}: recipient and message as dictated, split once contacts are known. */
        SEND_TEXT,
        /** {@link #text}: the destination; {@link #value}: 1 for walking, else 0. */
        NAVIGATE,
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
