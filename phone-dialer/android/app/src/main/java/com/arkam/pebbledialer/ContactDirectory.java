package com.arkam.pebbledialer;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.SystemClock;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.telephony.PhoneNumberUtils;

import java.text.Collator;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Every phone number in the address book, sorted by contact name, with a short
 * cache so paging through a list on the watch does not re-query each page.
 */
final class ContactDirectory {
    static final String OTHER_GROUP = "#";

    private static final long CACHE_TTL_MS = 30_000;

    private static List<PhoneEntry> cachedNumbers;
    private static long cachedAt;

    static final class PhoneEntry {
        final long dataId;
        final long contactId;
        final String name;
        final String label;
        final String number;
        final boolean starred;
        final String group;

        PhoneEntry(long dataId, long contactId, String name, String label,
               String number, boolean starred) {
            this.dataId = dataId;
            this.contactId = contactId;
            this.name = name;
            this.label = label;
            this.number = number;
            this.starred = starred;
            this.group = groupFor(name);
        }
    }

    private ContactDirectory() {
    }

    static synchronized List<PhoneEntry> load(Context context) {
        long now = SystemClock.elapsedRealtime();
        if (cachedNumbers != null && now - cachedAt < CACHE_TTL_MS) {
            return cachedNumbers;
        }
        cachedNumbers = Collections.unmodifiableList(query(context));
        cachedAt = now;
        return cachedNumbers;
    }

    static List<PhoneEntry> favorites(Context context) {
        List<PhoneEntry> favorites = new ArrayList<>();
        for (PhoneEntry number : load(context)) {
            if (number.starred) {
                favorites.add(number);
            }
        }
        return favorites;
    }

    static List<PhoneEntry> inGroup(Context context, String group) {
        List<PhoneEntry> numbers = new ArrayList<>();
        for (PhoneEntry number : load(context)) {
            if (number.group.equals(group)) {
                numbers.add(number);
            }
        }
        return numbers;
    }

    /** Number of distinct contacts under each letter, A to Z first and then "#". */
    static Map<String, Integer> groupSizes(Context context) {
        Map<String, Set<Long>> contacts = new LinkedHashMap<>();
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            contacts.put(String.valueOf(letter), new HashSet<>());
        }
        contacts.put(OTHER_GROUP, new HashSet<>());
        for (PhoneEntry number : load(context)) {
            contacts.get(number.group).add(number.contactId);
        }

        Map<String, Integer> sizes = new LinkedHashMap<>();
        for (Map.Entry<String, Set<Long>> entry : contacts.entrySet()) {
            if (!entry.getValue().isEmpty()) {
                sizes.put(entry.getKey(), entry.getValue().size());
            }
        }
        return sizes;
    }

    /** Looks up the number to dial, straight from the provider so it is never stale. */
    static String numberFor(Context context, long dataId) {
        try (Cursor cursor = context.getContentResolver().query(
                Phone.CONTENT_URI,
                new String[] { Phone.NUMBER },
                Phone._ID + " = ?",
                new String[] { String.valueOf(dataId) },
                null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String number = cursor.getString(0);
                return number == null || number.trim().isEmpty() ? null : number.trim();
            }
        }
        return null;
    }

    private static List<PhoneEntry> query(Context context) {
        ContentResolver resolver = context.getContentResolver();
        Resources resources = context.getResources();
        String[] projection = {
                Phone._ID,
                Phone.CONTACT_ID,
                Phone.DISPLAY_NAME_PRIMARY,
                Phone.NUMBER,
                Phone.TYPE,
                Phone.LABEL,
                Phone.STARRED,
        };

        List<PhoneEntry> numbers = new ArrayList<>();
        // Synced accounts often store the same number twice for one contact.
        Set<String> seen = new HashSet<>();
        try (Cursor cursor = resolver.query(Phone.CONTENT_URI, projection, null, null, null)) {
            if (cursor == null) {
                return numbers;
            }
            while (cursor.moveToNext()) {
                String number = cursor.getString(3);
                if (number == null || number.trim().isEmpty()) {
                    continue;
                }
                number = number.trim();
                long contactId = cursor.getLong(1);
                if (!seen.add(contactId + ":" + PhoneNumberUtils.normalizeNumber(number))) {
                    continue;
                }

                String name = cursor.getString(2);
                if (name == null || name.trim().isEmpty()) {
                    name = number;
                }
                CharSequence label = Phone.getTypeLabel(
                        resources, cursor.getInt(4), cursor.getString(5));
                numbers.add(new PhoneEntry(
                        cursor.getLong(0),
                        contactId,
                        name.trim(),
                        label == null ? "" : label.toString(),
                        number,
                        cursor.getInt(6) != 0));
            }
        }

        Collator collator = Collator.getInstance();
        collator.setStrength(Collator.SECONDARY);
        numbers.sort((a, b) -> {
            int byName = collator.compare(a.name, b.name);
            if (byName != 0) {
                return byName;
            }
            int byContact = Long.compare(a.contactId, b.contactId);
            return byContact != 0 ? byContact : a.label.compareTo(b.label);
        });
        return numbers;
    }

    /** "A" to "Z" by the name's first letter with accents removed, otherwise "#". */
    private static String groupFor(String name) {
        String plain = Normalizer.normalize(name, Normalizer.Form.NFD)
                .replaceAll("\\p{M}", "")
                .toUpperCase(Locale.ROOT);
        for (int i = 0; i < plain.length(); i++) {
            char c = plain.charAt(i);
            if (Character.isWhitespace(c)) {
                continue;
            }
            return c >= 'A' && c <= 'Z' ? String.valueOf(c) : OTHER_GROUP;
        }
        return OTHER_GROUP;
    }
}
