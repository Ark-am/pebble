package com.arkam.pebblevoicecommands;

import java.util.List;

/** An address-book entry with its phone numbers. Plain Java, for unit tests. */
final class Contact {
    static final int KIND_ANY = -1;
    static final int KIND_OTHER = 0;
    static final int KIND_MOBILE = 1;
    static final int KIND_HOME = 2;
    static final int KIND_WORK = 3;

    static final class Number {
        final String value;
        final int kind;
        /** The contact's default number, as chosen in the Contacts app. */
        final boolean primary;

        Number(String value, int kind, boolean primary) {
            this.value = value;
            this.kind = kind;
            this.primary = primary;
        }
    }

    final String name;
    final boolean starred;
    final List<Number> numbers;

    Contact(String name, boolean starred, List<Number> numbers) {
        this.name = name;
        this.starred = starred;
        this.numbers = numbers;
    }

    /**
     * @param kind  a KIND_* value the user asked for, or {@link #KIND_ANY}
     * @param texting prefer a mobile number over the default, since landlines
     *                cannot receive texts
     * @return the number to use, or null if there is none of that kind
     */
    Number pick(int kind, boolean texting) {
        if (kind != KIND_ANY) {
            Number match = null;
            for (Number number : numbers) {
                if (number.kind == kind && (match == null || number.primary)) {
                    match = number;
                }
            }
            return match;
        }

        Number primary = null;
        Number mobile = null;
        for (Number number : numbers) {
            if (number.primary && primary == null) {
                primary = number;
            }
            if (number.kind == KIND_MOBILE && mobile == null) {
                mobile = number;
            }
        }
        if (texting && mobile != null) {
            return mobile;
        }
        if (primary != null) {
            return primary;
        }
        if (mobile != null) {
            return mobile;
        }
        return numbers.isEmpty() ? null : numbers.get(0);
    }

    static String kindName(int kind) {
        switch (kind) {
            case KIND_MOBILE:
                return "mobile";
            case KIND_HOME:
                return "home";
            case KIND_WORK:
                return "work";
            default:
                return "other";
        }
    }
}
