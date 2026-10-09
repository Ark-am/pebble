package com.arkam.pebblevoicecommands;

import android.content.Context;
import android.content.SharedPreferences;

/** Remembers the last command so the phone screen can show what happened. */
final class CommandLog {
    static final String PREFERENCES = "command_log";
    private static final String KEY_TRANSCRIPT = "transcript";
    private static final String KEY_REPLY = "reply";

    private CommandLog() {
    }

    static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }

    static void record(Context context, String transcript, CommandResult result) {
        preferences(context).edit()
                .putString(KEY_TRANSCRIPT, transcript)
                .putString(KEY_REPLY, result.reply)
                .apply();
    }

    static String transcript(Context context) {
        return preferences(context).getString(KEY_TRANSCRIPT, null);
    }

    static String reply(Context context) {
        return preferences(context).getString(KEY_REPLY, null);
    }
}
