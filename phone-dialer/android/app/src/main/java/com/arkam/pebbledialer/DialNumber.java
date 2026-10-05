package com.arkam.pebbledialer;

/** The manual watch keypad's wire format. Never accept a URI or arbitrary intent. */
final class DialNumber {
    static final int MAX_LENGTH = 31;

    private DialNumber() { }

    static boolean isValid(String number) {
        if (number == null || number.isEmpty() || number.length() > MAX_LENGTH) {
            return false;
        }
        boolean digit = false;
        for (int i = 0; i < number.length(); i++) {
            char c = number.charAt(i);
            if (c >= '0' && c <= '9') {
                digit = true;
            } else if (c != '*' && c != '#' && !(c == '+' && i == 0)) {
                return false;
            }
        }
        return digit;
    }
}
