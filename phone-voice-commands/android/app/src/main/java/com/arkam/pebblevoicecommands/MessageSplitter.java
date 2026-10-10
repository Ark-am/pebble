package com.arkam.pebblevoicecommands;

import java.util.function.Predicate;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Splits "Sam Smith I'm running late" into a recipient and a message. Dictation
 * rarely marks where the name ends, so the longest run of leading words that
 * names a contact wins.
 */
final class MessageSplitter {
    static final class Split {
        final String recipient;
        /** True when {@link #recipient} is a phone number rather than a name. */
        final boolean isNumber;
        final String body;

        Split(String recipient, boolean isNumber, String body) {
            this.recipient = recipient;
            this.isNumber = isNumber;
            this.body = body;
        }
    }

    private static final int MAX_NAME_WORDS = 4;
    private static final Pattern LEADING_NUMBER = Pattern.compile(
            "^(\\+?\\d[\\d\\s().-]*\\d)\\s*[,:]?\\s*(.*)$", Pattern.DOTALL);
    private static final Pattern DELIMITER = Pattern.compile(
            "\\s*[,:]\\s*|\\s+(?:saying|that says|that reads)\\s+", Pattern.CASE_INSENSITIVE);
    private static final Pattern BODY_LEAD = Pattern.compile(
            "^(?:[,:\\s]+|(?:saying|that says|that reads|says)\\s+)+", Pattern.CASE_INSENSITIVE);

    private MessageSplitter() {
    }

    static Split split(String rest, Predicate<String> isContact) {
        String text = rest == null ? "" : rest.trim();

        Matcher number = LEADING_NUMBER.matcher(text);
        if (number.matches()) {
            String digits = number.group(1).replaceAll("[^+\\d]", "");
            if (digits.replace("+", "").length() >= 3) {
                return new Split(digits, true, body(number.group(2)));
            }
        }

        String[] words = text.split("\\s+");
        for (int count = Math.min(MAX_NAME_WORDS, words.length); count >= 1; count--) {
            String name = join(words, count).replaceAll("[,:.!?]+$", "");
            if (isContact.test(name)) {
                return new Split(name, false, body(join(words, count, words.length)));
            }
        }

        // No contact matched: an explicit "," or "saying" still marks the name.
        Matcher delimiter = DELIMITER.matcher(text);
        if (delimiter.find() && delimiter.start() > 0) {
            String name = text.substring(0, delimiter.start());
            if (name.split("\\s+").length <= MAX_NAME_WORDS) {
                return new Split(name, false, body(text.substring(delimiter.end())));
            }
        }
        return new Split(words[0].replaceAll("[,:.!?]+$", ""), false,
                body(join(words, 1, words.length)));
    }

    /** Drops a leading "saying" and wrapping quotes, and capitalises the first letter. */
    private static String body(String text) {
        String body = BODY_LEAD.matcher(text.trim()).replaceFirst("").trim();
        body = body.replaceAll("^[\"“]+|[\"”]+$", "").trim();
        if (body.isEmpty()) {
            return body;
        }
        int first = body.codePointAt(0);
        return new StringBuilder()
                .appendCodePoint(Character.toUpperCase(first))
                .append(body.substring(Character.charCount(first)))
                .toString();
    }

    private static String join(String[] words, int count) {
        return join(words, 0, count);
    }

    private static String join(String[] words, int from, int to) {
        StringBuilder text = new StringBuilder();
        for (int i = from; i < to; i++) {
            if (text.length() > 0) {
                text.append(' ');
            }
            text.append(words[i]);
        }
        return text.toString();
    }
}
