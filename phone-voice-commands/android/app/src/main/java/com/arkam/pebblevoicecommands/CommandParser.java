package com.arkam.pebblevoicecommands;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.arkam.pebblevoicecommands.ParsedCommand.Action;

/**
 * Turns dictated text into a {@link ParsedCommand}. Plain Java with no Android
 * calls, so every phrase it accepts is covered by unit tests.
 */
final class CommandParser {
    private static final Pattern FILLER_START = Pattern.compile(
            "^(?:ok|okay|hey|hi|please|so|um+|uh+|can you|could you|would you|will you"
                    + "|i want to|i want you to|id like to|i would like to|go ahead and|lets)\\s+");
    private static final Pattern FILLER_END = Pattern.compile(
            "\\s+(?:please|for me|now|thanks|thank you)$");

    private static final Pattern HELP = Pattern.compile(
            "^(?:help|commands|what can (?:you|i) (?:do|say))$");

    private static final Pattern FLASHLIGHT = Pattern.compile("\\b(?:flashlight|flash light|torch)\\b");
    private static final Pattern WORD_ON = Pattern.compile("\\b(?:on|enable|activate)\\b");
    private static final Pattern WORD_OFF = Pattern.compile("\\b(?:off|disable|deactivate)\\b");

    private static final Pattern ALARM = Pattern.compile("\\balarm\\b|\\bwake me\\b");
    private static final Pattern TIMER = Pattern.compile("\\btimer\\b|\\bcountdown\\b|\\bcount down\\b");
    // Cancelling and changing existing alarms or timers is not supported.
    private static final Pattern CANCEL = Pattern.compile(
            "^(?:cancel|delete|remove|clear|turn off|stop|dismiss|snooze)\\b");
    private static final Pattern BARE_NUMBER = Pattern.compile("\\b(\\d{1,3})\\b");
    private static final Pattern DURATION = Pattern.compile(
            "(\\d+)(\\s+and\\s+a\\s+half)?\\s*(hours?|hrs?|minutes?|mins?|seconds?|secs?)\\b"
                    + "(\\s+and\\s+a\\s+half)?");
    // "7", "7:30", "7 30", "730", each optionally followed by am or pm.
    private static final Pattern CLOCK = Pattern.compile(
            "\\b(\\d{1,4})(?::(\\d{2})|\\s+(\\d{2})(?!\\s*(?:hours?|hrs?|minutes?|mins?|seconds?|secs?)\\b))?"
                    + "\\s*(am|pm)?\\b(?!\\s*(?:hours?|hrs?|minutes?|mins?|seconds?|secs?)\\b)");

    private static final String MEDIA_OBJECT =
            "(?:\\s+(?:the\\s+)?(?:music|song|track|playback|playing|it|media|audio|podcast|video))?";
    private static final Pattern MEDIA_NEXT = Pattern.compile(
            "^(?:play\\s+(?:the\\s+)?)?(?:next|skip)(?:\\s+(?:this|the))?(?:\\s+(?:song|track|one))?$");
    private static final Pattern MEDIA_PREVIOUS = Pattern.compile(
            "^(?:play\\s+(?:the\\s+)?)?(?:previous|last)(?:\\s+(?:song|track|one))?$"
                    + "|^go back(?:\\s+(?:a|1)\\s+(?:song|track))?$");
    private static final Pattern MEDIA_PAUSE = Pattern.compile(
            "^(?:pause|stop)" + MEDIA_OBJECT + "$");
    private static final Pattern MEDIA_PLAY = Pattern.compile(
            "^(?:play|resume|unpause|continue|start)" + MEDIA_OBJECT + "$");

    private static final String VOLUME_OBJECT = "(?:\\s+(?:the\\s+)?(?:volume|music|sound|media|audio|it))";
    private static final Pattern VOLUME_SET = Pattern.compile(
            "^(?:(?:set|turn|change|put)\\s+(?:the\\s+)?)?(?:media\\s+|music\\s+)?volume\\s+"
                    + "(?:to\\s+|at\\s+)?(\\d{1,3})\\s*(%|percent)?$");
    private static final Pattern VOLUME_MAX = Pattern.compile(
            "\\b(?:max|maximum|full)\\s+volume\\b|\\bvolume\\s+(?:to\\s+)?(?:max|maximum|full)$");
    private static final Pattern VOLUME_UP = Pattern.compile(
            "^(?:volume\\s+up|louder|make it louder|turn" + VOLUME_OBJECT + "?\\s+up"
                    + "|(?:turn up|increase|raise)" + VOLUME_OBJECT + ")$");
    private static final Pattern VOLUME_DOWN = Pattern.compile(
            "^(?:volume\\s+down|quieter|softer|make it (?:quieter|softer)|turn" + VOLUME_OBJECT + "?\\s+down"
                    + "|(?:turn down|decrease|lower|reduce)" + VOLUME_OBJECT + ")$");
    private static final Pattern VOLUME_MUTE = Pattern.compile("^mute" + VOLUME_OBJECT + "?$");
    private static final Pattern VOLUME_UNMUTE = Pattern.compile("^unmute" + VOLUME_OBJECT + "?$");

    private static final Pattern OPEN_APP = Pattern.compile(
            "^(?:open|launch|start|run|show)\\s+(?:the\\s+|my\\s+)?(.+?)(?:\\s+(?:app|application))?$");

    private static final Map<String, Integer> SMALL_NUMBERS = new HashMap<>();
    private static final Map<String, Integer> TENS = new HashMap<>();

    static {
        String[] small = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
                "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                "seventeen", "eighteen", "nineteen"};
        for (int i = 0; i < small.length; i++) {
            SMALL_NUMBERS.put(small[i], i);
        }
        String[] tens = {"twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
        for (int i = 0; i < tens.length; i++) {
            TENS.put(tens[i], (i + 2) * 10);
        }
    }

    private CommandParser() {
    }

    /**
     * @param nowMinutes the phone's current time in minutes after midnight, used
     *                   to pick am or pm when an alarm time leaves it out
     * @return the command, or null when the words are not one we know
     */
    static ParsedCommand parse(String transcript, int nowMinutes) {
        String plain = stripFillers(clean(transcript));
        if (plain.isEmpty()) {
            return null;
        }
        String text = numbersToDigits(plain);

        if (HELP.matcher(text).matches()) {
            return new ParsedCommand(Action.HELP);
        }
        if (FLASHLIGHT.matcher(text).find()) {
            return flashlight(text);
        }
        if (ALARM.matcher(text).find()) {
            return CANCEL.matcher(text).find() ? null : alarm(text, nowMinutes);
        }
        if (TIMER.matcher(text).find()) {
            return CANCEL.matcher(text).find() ? null : timer(text);
        }

        ParsedCommand volume = volume(text);
        if (volume != null) {
            return volume;
        }
        ParsedCommand media = media(text);
        if (media != null) {
            return media;
        }

        // App names are matched on the words as spoken, so "one note" stays words.
        Matcher open = OPEN_APP.matcher(plain);
        if (open.matches()) {
            return new ParsedCommand(Action.OPEN_APP, 0, open.group(1));
        }
        return null;
    }

    /** Lower-cases, joins "a.m." into "am" and drops punctuation other than "7:30" colons and "%". */
    static String clean(String text) {
        if (text == null) {
            return "";
        }
        String s = text.toLowerCase(Locale.ROOT);
        s = s.replaceAll("(\\d)\\s*([ap])\\.?\\s?m\\b\\.?", "$1 $2m");
        s = s.replace("'", "").replace("’", "");
        s = s.replaceAll("[^\\p{L}\\p{N}:%\\s]", " ");
        s = s.replaceAll("(?<!\\d):|:(?!\\d)", " ");
        return s.replaceAll("\\s+", " ").trim();
    }

    private static String stripFillers(String text) {
        String previous;
        do {
            previous = text;
            text = FILLER_START.matcher(text).replaceFirst("");
            text = FILLER_END.matcher(text).replaceFirst("");
        } while (!text.equals(previous));
        return text;
    }

    /** Rewrites spoken numbers up to the hundreds ("twenty five", "one hundred") as digits. */
    static String numbersToDigits(String text) {
        String[] words = text.split(" ");
        StringBuilder out = new StringBuilder();
        int i = 0;
        while (i < words.length) {
            int value = -1;
            Integer tens = TENS.get(words[i]);
            Integer small = SMALL_NUMBERS.get(words[i]);
            if (tens != null) {
                value = tens;
                i++;
                Integer unit = i < words.length ? SMALL_NUMBERS.get(words[i]) : null;
                if (unit != null && unit > 0 && unit < 10) {
                    value += unit;
                    i++;
                }
            } else if (small != null) {
                value = small;
                i++;
                if (i < words.length && words[i].equals("hundred")) {
                    value *= 100;
                    i++;
                }
            }

            if (out.length() > 0) {
                out.append(' ');
            }
            if (value >= 0) {
                out.append(value);
            } else {
                out.append(words[i]);
                i++;
            }
        }
        return out.toString()
                .replaceAll("\\b(\\d+) oclock\\b", "$1")
                .replaceAll("\\bnoon\\b", "12 pm")
                .replaceAll("\\bmidnight\\b", "12 am")
                .replaceAll("\\bhalf past (\\d{1,2})\\b", "$1:30")
                .replaceAll("\\bquarter past (\\d{1,2})\\b", "$1:15")
                .replaceAll("\\b(?:half an|a half) hour\\b", "30 minutes")
                .replaceAll("\\b(?:a|an) (hour|minute|second)\\b", "1 $1")
                .replaceAll("\\b(\\d{1,2}(?::\\d{2})?) in the morning\\b", "$1 am")
                .replaceAll("\\b(\\d{1,2}(?::\\d{2})?) (?:in the (?:afternoon|evening)|at night|tonight)\\b",
                        "$1 pm");
    }

    private static ParsedCommand flashlight(String text) {
        if (WORD_OFF.matcher(text).find()) {
            return new ParsedCommand(Action.FLASHLIGHT_OFF);
        }
        if (WORD_ON.matcher(text).find()) {
            return new ParsedCommand(Action.FLASHLIGHT_ON);
        }
        return new ParsedCommand(Action.FLASHLIGHT_TOGGLE);
    }

    private static ParsedCommand timer(String text) {
        int seconds = durationSeconds(text);
        if (seconds < 0) {
            // "Timer for 5" means minutes.
            Matcher bare = BARE_NUMBER.matcher(text);
            seconds = bare.find() ? Integer.parseInt(bare.group(1)) * 60 : 0;
        }
        return new ParsedCommand(Action.TIMER, seconds, null);
    }

    private static ParsedCommand alarm(String text, int nowMinutes) {
        // "Wake me up in 20 minutes" sets an alarm relative to now.
        int seconds = durationSeconds(text);
        if (seconds > 0) {
            int minutes = (seconds + 59) / 60;
            return new ParsedCommand(Action.ALARM, (nowMinutes + minutes) % (24 * 60), null);
        }
        return new ParsedCommand(Action.ALARM, clockMinutes(text, nowMinutes), null);
    }

    /** @return the total length of every "5 minutes"-style phrase, or -1 if there is none. */
    static int durationSeconds(String text) {
        Matcher matcher = DURATION.matcher(text);
        int total = -1;
        while (matcher.find()) {
            int amount = Integer.parseInt(matcher.group(1));
            String unit = matcher.group(3);
            int scale = unit.startsWith("h") ? 3600 : unit.startsWith("m") ? 60 : 1;
            boolean half = matcher.group(2) != null || matcher.group(4) != null;
            total = Math.max(total, 0) + amount * scale + (half ? scale / 2 : 0);
        }
        return total;
    }

    /** @return minutes after midnight, or -1 when there is no valid time. */
    static int clockMinutes(String text, int nowMinutes) {
        Matcher matcher = CLOCK.matcher(text);
        while (matcher.find()) {
            String digits = matcher.group(1);
            String minuteText = matcher.group(2) != null ? matcher.group(2) : matcher.group(3);
            String meridiem = matcher.group(4);
            int hour;
            int minute;
            if (digits.length() >= 3 && minuteText == null) {
                int compact = Integer.parseInt(digits);
                hour = compact / 100;
                minute = compact % 100;
            } else if (digits.length() <= 2) {
                hour = Integer.parseInt(digits);
                minute = minuteText == null ? 0 : Integer.parseInt(minuteText);
            } else {
                continue;
            }
            if (minute > 59) {
                continue;
            }

            if (meridiem != null) {
                if (hour < 1 || hour > 12) {
                    continue;
                }
                hour = hour % 12 + (meridiem.equals("pm") ? 12 : 0);
                return hour * 60 + minute;
            }
            if (hour > 23) {
                continue;
            }
            if (hour == 0 || hour > 12) {
                return hour * 60 + minute;
            }
            // No am or pm: take whichever of the two comes next.
            int morning = (hour % 12) * 60 + minute;
            int evening = morning + 12 * 60;
            if (morning > nowMinutes) {
                return morning;
            }
            return evening > nowMinutes ? evening : morning;
        }
        return -1;
    }

    private static ParsedCommand volume(String text) {
        Matcher set = VOLUME_SET.matcher(text);
        if (set.matches()) {
            int amount = Integer.parseInt(set.group(1));
            // "Volume 7" is read as 7 out of 10; "volume 7 percent" as 7%.
            int percent = set.group(2) == null && amount <= 10 ? amount * 10 : amount;
            return new ParsedCommand(Action.VOLUME_SET, Math.min(100, percent), null);
        }
        if (VOLUME_MAX.matcher(text).find()) {
            return new ParsedCommand(Action.VOLUME_SET, 100, null);
        }
        if (VOLUME_UP.matcher(text).matches()) {
            return new ParsedCommand(Action.VOLUME_UP);
        }
        if (VOLUME_DOWN.matcher(text).matches()) {
            return new ParsedCommand(Action.VOLUME_DOWN);
        }
        if (VOLUME_MUTE.matcher(text).matches()) {
            return new ParsedCommand(Action.VOLUME_MUTE);
        }
        if (VOLUME_UNMUTE.matcher(text).matches()) {
            return new ParsedCommand(Action.VOLUME_UNMUTE);
        }
        return null;
    }

    private static ParsedCommand media(String text) {
        if (MEDIA_NEXT.matcher(text).matches()) {
            return new ParsedCommand(Action.MEDIA_NEXT);
        }
        if (MEDIA_PREVIOUS.matcher(text).matches()) {
            return new ParsedCommand(Action.MEDIA_PREVIOUS);
        }
        if (MEDIA_PAUSE.matcher(text).matches()) {
            return new ParsedCommand(Action.MEDIA_PAUSE);
        }
        if (MEDIA_PLAY.matcher(text).matches()) {
            return new ParsedCommand(Action.MEDIA_PLAY);
        }
        return null;
    }
}
