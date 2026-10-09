package com.arkam.pebblevoicecommands;

import java.util.Locale;

/** Short, watch-sized descriptions of durations and clock times. */
final class Formats {
    private Formats() {
    }

    /** "1 h 30 min", "5 min", "45 s". */
    static String duration(int seconds) {
        int hours = seconds / 3600;
        int minutes = seconds % 3600 / 60;
        int rest = seconds % 60;
        StringBuilder text = new StringBuilder();
        if (hours > 0) {
            text.append(hours).append(" h");
        }
        if (minutes > 0) {
            text.append(text.length() > 0 ? " " : "").append(minutes).append(" min");
        }
        if (rest > 0 || text.length() == 0) {
            text.append(text.length() > 0 ? " " : "").append(rest).append(" s");
        }
        return text.toString();
    }

    /** "7:05 AM", or "07:05" with the phone's 24-hour setting. */
    static String clock(int minutesOfDay, boolean twentyFourHour) {
        int hour = minutesOfDay / 60;
        int minute = minutesOfDay % 60;
        if (twentyFourHour) {
            return String.format(Locale.ROOT, "%02d:%02d", hour, minute);
        }
        int twelve = hour % 12 == 0 ? 12 : hour % 12;
        return String.format(Locale.ROOT, "%d:%02d %s", twelve, minute, hour < 12 ? "AM" : "PM");
    }
}
