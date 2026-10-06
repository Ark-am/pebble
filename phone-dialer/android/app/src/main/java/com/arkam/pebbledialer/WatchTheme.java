package com.arkam.pebbledialer;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.HashMap;
import java.util.Map;

import io.rebble.pebblekit2.common.model.PebbleDictionaryItem;

/**
 * The watch app's colour theme, shared between the watch and this app like
 * {@link MenuOrder}: both sides keep it with the time it last changed, and the
 * newer one wins.
 */
final class WatchTheme {
    // Must match messageKeys in the watch app's package.json.
    static final int KEY_THEME = 16;
    static final int KEY_THEME_STAMP = 17;

    static final int DARK = 0;
    static final int LIGHT = 1;

    private static final String PREFERENCES = "theme";
    private static final String PREF_THEME = "theme";
    private static final String PREF_STAMP = "stamp";

    final int theme;
    final int stamp;

    WatchTheme(int theme, int stamp) {
        this.theme = theme;
        this.stamp = stamp;
    }

    static boolean isValid(int theme) {
        return theme == DARK || theme == LIGHT;
    }

    static synchronized WatchTheme load(Context context) {
        SharedPreferences preferences = preferences(context);
        int theme = preferences.getInt(PREF_THEME, DARK);
        return isValid(theme)
                ? new WatchTheme(theme, preferences.getInt(PREF_STAMP, 0))
                : new WatchTheme(DARK, 0);
    }

    static synchronized void save(Context context, WatchTheme watchTheme) {
        preferences(context).edit()
                .putInt(PREF_THEME, watchTheme.theme)
                .putInt(PREF_STAMP, watchTheme.stamp)
                .apply();
    }

    /** A theme chosen on the phone, stamped now so it beats the watch's older one. */
    static WatchTheme changedNow(int theme) {
        return new WatchTheme(theme, (int) (System.currentTimeMillis() / 1000));
    }

    Map<Integer, PebbleDictionaryItem> message() {
        Map<Integer, PebbleDictionaryItem> message = new HashMap<>();
        message.put(KEY_THEME, new PebbleDictionaryItem.Int32(theme));
        message.put(KEY_THEME_STAMP, new PebbleDictionaryItem.Int32(stamp));
        return message;
    }

    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }
}
