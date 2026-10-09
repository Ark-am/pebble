package com.arkam.pebblevoicecommands;

import java.nio.charset.StandardCharsets;

/** Prepares text for the watch's fixed-size string buffers. */
final class WatchText {
    // The watch's reply buffer is 128 bytes, including the terminating NUL.
    static final int MAX_REPLY_BYTES = 127;

    private WatchText() {
    }

    /** Replaces control characters and trims to whole UTF-8 characters. */
    static String fit(String text, int maxBytes) {
        if (text == null) {
            return "";
        }

        StringBuilder clean = new StringBuilder();
        int bytes = 0;
        for (int i = 0; i < text.length(); ) {
            int codePoint = text.codePointAt(i);
            i += Character.charCount(codePoint);
            if (Character.isISOControl(codePoint)) {
                codePoint = ' ';
            }
            int size = new String(Character.toChars(codePoint))
                    .getBytes(StandardCharsets.UTF_8).length;
            if (bytes + size > maxBytes) {
                break;
            }
            clean.appendCodePoint(codePoint);
            bytes += size;
        }
        return clean.toString().trim();
    }
}
