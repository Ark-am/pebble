package com.arkam.pebbledialer;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.CallLog.Calls;
import android.text.format.DateFormat;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/** A fresh, bounded view of the system call history, newest first. */
final class RecentCalls {
    static final int MAX_CALLS = 100;

    static final class Entry {
        final long id;
        final String title;
        final String subtitle;

        Entry(long id, String title, String subtitle) {
            this.id = id;
            this.title = title;
            this.subtitle = subtitle;
        }
    }

    private RecentCalls() { }

    static List<Entry> load(Context context) {
        List<Entry> entries = new ArrayList<>();
        Uri uri = Calls.CONTENT_URI.buildUpon()
                .appendQueryParameter(Calls.LIMIT_PARAM_KEY, String.valueOf(MAX_CALLS)).build();
        try (Cursor cursor = context.getContentResolver().query(uri,
                new String[] { Calls._ID, Calls.NUMBER, Calls.CACHED_NAME, Calls.TYPE,
                        Calls.DATE, Calls.NUMBER_PRESENTATION },
                null, null, Calls.DATE + " DESC, " + Calls._ID + " DESC")) {
            if (cursor == null) {
                throw new IllegalStateException("Call history is unavailable");
            }
            while (cursor.moveToNext() && entries.size() < MAX_CALLS) {
                String number = cursor.getString(1);
                int presentation = cursor.getInt(5);
                boolean callable = presentation == Calls.PRESENTATION_ALLOWED
                        && number != null && !number.trim().isEmpty()
                        && !number.equals("-1") && !number.equals("-2") && !number.equals("-3");
                String name = cursor.getString(2);
                String title = !callable
                        ? presentation == Calls.PRESENTATION_RESTRICTED ? "Private number" : "Unknown number"
                        : name == null || name.trim().isEmpty() ? number : name;
                Date date = new Date(cursor.getLong(4));
                String when = DateFormat.format("MMM d", date) + " "
                        + DateFormat.getTimeFormat(context).format(date);
                entries.add(new Entry(callable ? cursor.getLong(0) : 0,
                        title, typeLabel(cursor.getInt(3)) + " · " + when));
            }
        }
        return entries;
    }

    static String numberFor(Context context, long id) {
        try (Cursor cursor = context.getContentResolver().query(Calls.CONTENT_URI,
                new String[] { Calls.NUMBER, Calls.NUMBER_PRESENTATION },
                Calls._ID + " = ?", new String[] { String.valueOf(id) }, null)) {
            if (cursor != null && cursor.moveToFirst()
                    && cursor.getInt(1) == Calls.PRESENTATION_ALLOWED) {
                String number = cursor.getString(0);
                return number == null || number.trim().isEmpty() || number.startsWith("-")
                        ? null : number.trim();
            }
        }
        return null;
    }

    private static String typeLabel(int type) {
        switch (type) {
            case Calls.INCOMING_TYPE: return "Incoming";
            case Calls.OUTGOING_TYPE: return "Outgoing";
            case Calls.MISSED_TYPE: return "Missed";
            case Calls.REJECTED_TYPE: return "Declined";
            case Calls.BLOCKED_TYPE: return "Blocked";
            case Calls.VOICEMAIL_TYPE: return "Voicemail";
            default: return "Call";
        }
    }
}
