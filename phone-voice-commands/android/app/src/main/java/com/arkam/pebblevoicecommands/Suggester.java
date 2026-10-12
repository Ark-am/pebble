package com.arkam.pebblevoicecommands;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Finds the command a misheard phrase most likely meant, for the watch to offer
 * as "Did you mean...?". Words that are close in spelling to a command word, or
 * are a known sound-alike of one, are swapped for it until the phrase parses.
 * The other words are kept exactly as dictated, so a corrected text message
 * keeps its wording. Plain Java, for unit tests.
 */
final class Suggester {
    /** Words the parser looks for. Contact, app and place names are never "corrected". */
    private static final String[] VOCABULARY = {
            "call", "phone", "dial", "ring", "text", "message", "send", "tell",
            "navigate", "directions", "drive", "walk", "walking", "take",
            "timer", "alarm", "wake", "countdown",
            "flashlight", "torch", "flash", "light",
            "volume", "louder", "quieter", "softer", "down", "mute", "unmute", "increase", "decrease",
            "play", "pause", "resume", "stop", "next", "previous", "skip", "song", "track", "music",
            "open", "launch", "start",
            "minutes", "minute", "hours", "hour", "seconds", "second", "percent", "maximum",
            "help",
    };

    /** Sound-alikes that dictation produces and spelling distance would miss. */
    private static final Map<String, String> SOUND_ALIKES = new HashMap<>();

    static {
        String[][] pairs = {
                {"paws", "pause"}, {"pores", "pause"}, {"pours", "pause"}, {"pawn", "pause"},
                {"tax", "text"}, {"techs", "text"}, {"tacks", "text"},
                {"valium", "volume"}, {"column", "volume"},
                {"flesh", "flash"}, {"slashlight", "flashlight"},
                {"lunch", "launch"}, {"opened", "open"},
                {"tie", "timer"}, {"tyler", "timer"}, {"timor", "timer"},
                {"allarm", "alarm"},
        };
        for (String[] pair : pairs) {
            SOUND_ALIKES.put(pair[0], pair[1]);
        }
    }

    /**
     * At most two words are swapped, so a suggestion stays close to what was
     * said; the second swap is only tried among the first words, where command
     * words sit.
     */
    private static final int SECOND_EDIT_WORDS = 6;

    private static final class Edit {
        final int start;
        /** Number of dictated words replaced: 1, or 2 when "time her" becomes "timer". */
        final int length;
        final String word;
        final int cost;

        Edit(int start, int length, String word, int cost) {
            this.start = start;
            this.length = length;
            this.word = word;
            this.cost = cost;
        }
    }

    private Suggester() {
    }

    /**
     * @return the corrected phrase, or null when no small change makes it a command
     */
    static String suggest(String transcript, int nowMinutes) {
        if (transcript == null || transcript.trim().isEmpty()
                || CommandParser.parse(transcript, nowMinutes) != null) {
            return null;
        }
        String[] words = transcript.trim().split("\\s+");
        List<Edit> edits = edits(words);

        String best = null;
        int bestCost = Integer.MAX_VALUE;
        for (Edit edit : edits) {
            String candidate = apply(words, edit, null);
            if (edit.cost < bestCost && CommandParser.parse(candidate, nowMinutes) != null) {
                best = candidate;
                bestCost = edit.cost;
            }
        }
        if (best != null) {
            return best;
        }

        for (int i = 0; i < edits.size(); i++) {
            Edit first = edits.get(i);
            if (first.start >= SECOND_EDIT_WORDS) {
                continue;
            }
            for (int j = i + 1; j < edits.size(); j++) {
                Edit second = edits.get(j);
                if (second.start < first.start + first.length || second.start >= SECOND_EDIT_WORDS) {
                    continue;
                }
                int cost = first.cost + second.cost;
                if (cost >= bestCost) {
                    continue;
                }
                String candidate = apply(words, first, second);
                if (CommandParser.parse(candidate, nowMinutes) != null) {
                    best = candidate;
                    bestCost = cost;
                }
            }
        }
        return best;
    }

    /** Every single-word or two-word swap worth trying, in word order. */
    private static List<Edit> edits(String[] words) {
        List<Edit> edits = new ArrayList<>();
        for (int i = 0; i < words.length; i++) {
            addEdits(edits, i, 1, core(words[i]));
            if (i + 1 < words.length) {
                addEdits(edits, i, 2, core(words[i]) + core(words[i + 1]));
            }
        }
        return edits;
    }

    private static void addEdits(List<Edit> edits, int start, int length, String spoken) {
        if (spoken.isEmpty() || spoken.chars().anyMatch(Character::isDigit)) {
            return;
        }
        String alike = SOUND_ALIKES.get(spoken);
        if (alike != null) {
            edits.add(new Edit(start, length, alike, 1));
            return;
        }
        // A word that is already a command word is right as it is, unless two
        // dictated words join into one ("un mute").
        if (length == 1 && isVocabulary(spoken)) {
            return;
        }
        // Joining two words is for splits such as "un mute"; a looser match
        // would swallow a word ("tell me" is not "tell").
        int allowed = length == 2 ? Math.min(1, allowedDistance(spoken.length()))
                : allowedDistance(spoken.length());
        for (String word : VOCABULARY) {
            int distance = distance(spoken, word);
            if (distance <= allowed && !(length == 1 && distance == 0)) {
                edits.add(new Edit(start, length, word, Math.max(1, distance)));
            }
        }
    }

    /** Short words are left alone: "on" is one letter from "in", "an" and "at". */
    private static int allowedDistance(int length) {
        if (length <= 3) {
            return 0;
        }
        if (length == 4) {
            return 1;
        }
        return length <= 7 ? 2 : 3;
    }

    private static boolean isVocabulary(String word) {
        for (String known : VOCABULARY) {
            if (known.equals(word)) {
                return true;
            }
        }
        return false;
    }

    /** Rebuilds the phrase with the swaps applied, keeping every other word as dictated. */
    private static String apply(String[] words, Edit first, Edit second) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < words.length; ) {
            Edit edit = first.start == i ? first : second != null && second.start == i ? second : null;
            if (text.length() > 0) {
                text.append(' ');
            }
            if (edit == null) {
                text.append(words[i]);
                i++;
                continue;
            }
            String head = words[i];
            String tail = words[i + edit.length - 1];
            text.append(leading(head)).append(matchCase(head, edit.word)).append(trailing(tail));
            i += edit.length;
        }
        return text.toString();
    }

    /** The word without punctuation or apostrophes, lower-cased. */
    static String core(String word) {
        return word.toLowerCase(Locale.ROOT).replaceAll("[^\\p{L}\\p{N}]", "");
    }

    private static String leading(String word) {
        int i = 0;
        while (i < word.length() && !Character.isLetterOrDigit(word.charAt(i))) {
            i++;
        }
        return word.substring(0, i);
    }

    private static String trailing(String word) {
        int i = word.length();
        while (i > 0 && !Character.isLetterOrDigit(word.charAt(i - 1))) {
            i--;
        }
        return word.substring(i);
    }

    private static String matchCase(String original, String word) {
        String letters = original.replaceAll("[^\\p{L}]", "");
        if (!letters.isEmpty() && Character.isUpperCase(letters.charAt(0))) {
            return Character.toUpperCase(word.charAt(0)) + word.substring(1);
        }
        return word;
    }

    /** Edit distance counting a swap of two neighbouring letters as one edit. */
    static int distance(String a, String b) {
        int[][] d = new int[a.length() + 1][b.length() + 1];
        for (int i = 0; i <= a.length(); i++) {
            d[i][0] = i;
        }
        for (int j = 0; j <= b.length(); j++) {
            d[0][j] = j;
        }
        for (int i = 1; i <= a.length(); i++) {
            for (int j = 1; j <= b.length(); j++) {
                int cost = a.charAt(i - 1) == b.charAt(j - 1) ? 0 : 1;
                d[i][j] = Math.min(Math.min(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + cost);
                if (i > 1 && j > 1 && a.charAt(i - 1) == b.charAt(j - 2)
                        && a.charAt(i - 2) == b.charAt(j - 1)) {
                    d[i][j] = Math.min(d[i][j], d[i - 2][j - 2] + 1);
                }
            }
        }
        return d[a.length()][b.length()];
    }
}
