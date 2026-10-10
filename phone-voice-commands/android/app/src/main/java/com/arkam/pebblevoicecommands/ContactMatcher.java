package com.arkam.pebblevoicecommands;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Finds the contact a spoken name refers to. Stricter than {@link AppMatcher}:
 * only whole words count, so "Al" never calls Alice, and ties are reported
 * instead of guessed.
 */
final class ContactMatcher {
    static final class Match {
        /** The contact to use, or -1 when there is none or several tie. */
        final int index;
        /** The tied contacts when {@link #index} is -1, otherwise empty. */
        final List<Integer> ties;

        Match(int index, List<Integer> ties) {
            this.index = index;
            this.ties = ties;
        }
    }

    private ContactMatcher() {
    }

    static Match best(String spoken, List<Contact> contacts) {
        Match match = bestExact(spoken, contacts);
        // Dictation writes "mom's mobile" as "moms mobile".
        String key = key(spoken);
        if (match.index < 0 && match.ties.isEmpty() && key.endsWith("s") && key.length() > 2) {
            match = bestExact(key.substring(0, key.length() - 1), contacts);
        }
        return match;
    }

    static boolean matchesAny(String spoken, List<Contact> contacts) {
        Match match = best(spoken, contacts);
        return match.index >= 0 || !match.ties.isEmpty();
    }

    private static Match bestExact(String spoken, List<Contact> contacts) {
        String query = key(spoken);
        if (query.isEmpty()) {
            return new Match(-1, new ArrayList<>());
        }

        int bestScore = 0;
        List<Integer> best = new ArrayList<>();
        for (int i = 0; i < contacts.size(); i++) {
            int score = score(query, key(contacts.get(i).name));
            if (score > bestScore) {
                bestScore = score;
                best.clear();
            }
            if (score > 0 && score == bestScore) {
                best.add(i);
            }
        }

        if (best.size() == 1) {
            return new Match(best.get(0), new ArrayList<>());
        }
        if (best.isEmpty()) {
            return new Match(-1, new ArrayList<>());
        }

        // A favourite, or duplicates of one name from several accounts, settle a tie.
        List<Integer> starred = new ArrayList<>();
        Set<String> names = new HashSet<>();
        for (int index : best) {
            if (contacts.get(index).starred) {
                starred.add(index);
            }
            names.add(key(contacts.get(index).name));
        }
        if (starred.size() == 1) {
            return new Match(starred.get(0), new ArrayList<>());
        }
        if (names.size() == 1) {
            return new Match(best.get(0), new ArrayList<>());
        }
        return new Match(-1, best);
    }

    /** 2 for the whole name, 1 when every spoken word is a word of the name, else 0. */
    private static int score(String query, String name) {
        if (name.isEmpty()) {
            return 0;
        }
        if (name.replace(" ", "").equals(query.replace(" ", ""))) {
            return 2;
        }
        List<String> words = Arrays.asList(name.split(" "));
        for (String word : query.split(" ")) {
            if (!words.contains(word)) {
                return 0;
            }
        }
        return 1;
    }

    static String key(String text) {
        return text.toLowerCase(Locale.ROOT)
                .replace("'", "")
                .replace("’", "")
                .replaceAll("[^\\p{L}\\p{N}]+", " ")
                .trim();
    }
}
