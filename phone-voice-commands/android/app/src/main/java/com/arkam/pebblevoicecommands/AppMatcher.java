package com.arkam.pebblevoicecommands;

import java.util.List;
import java.util.Locale;

/** Picks the installed app whose name best matches a spoken one. */
final class AppMatcher {
    private AppMatcher() {
    }

    /** @return the index of the best label, or -1 when none is close enough. */
    static int best(String spoken, List<String> labels) {
        String query = key(spoken);
        String joinedQuery = query.replace(" ", "");
        if (joinedQuery.length() < 2) {
            return -1;
        }

        int bestIndex = -1;
        int bestScore = 0;
        int bestLength = Integer.MAX_VALUE;
        for (int i = 0; i < labels.size(); i++) {
            String label = key(labels.get(i));
            String joinedLabel = label.replace(" ", "");
            if (joinedLabel.isEmpty()) {
                continue;
            }

            int score;
            if (joinedLabel.equals(joinedQuery)) {
                score = 4;  // "you tube" and "YouTube"
            } else if ((" " + label + " ").contains(" " + query + " ")
                    || joinedLabel.startsWith(joinedQuery)) {
                score = 3;  // "maps" and "Google Maps"
            } else if (joinedLabel.length() >= 3 && (" " + query + " ").contains(" " + label + " ")) {
                score = 2;  // "google maps" and "Maps"
            } else if (joinedQuery.length() >= 4 && joinedLabel.contains(joinedQuery)) {
                score = 1;
            } else {
                continue;
            }

            // On a tie, the shorter name is the closer match.
            if (score > bestScore || (score == bestScore && joinedLabel.length() < bestLength)) {
                bestIndex = i;
                bestScore = score;
                bestLength = joinedLabel.length();
            }
        }
        return bestIndex;
    }

    private static String key(String text) {
        return text.toLowerCase(Locale.ROOT)
                .replace("'", "")
                .replaceAll("[^\\p{L}\\p{N}]+", " ")
                .trim();
    }
}
