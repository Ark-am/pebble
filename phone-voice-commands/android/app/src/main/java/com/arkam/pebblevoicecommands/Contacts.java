package com.arkam.pebblevoicecommands;

import android.content.Context;
import android.database.Cursor;
import android.provider.ContactsContract.CommonDataKinds.Phone;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Reads every contact that has a phone number. Needs READ_CONTACTS. */
final class Contacts {
    private Contacts() {
    }

    static List<Contact> load(Context context) {
        String[] projection = {
                Phone.CONTACT_ID,
                Phone.DISPLAY_NAME_PRIMARY,
                Phone.NUMBER,
                Phone.TYPE,
                Phone.IS_SUPER_PRIMARY,
                Phone.STARRED,
        };
        Map<Long, String> names = new LinkedHashMap<>();
        Map<Long, Boolean> starred = new LinkedHashMap<>();
        Map<Long, List<Contact.Number>> numbers = new LinkedHashMap<>();

        try (Cursor cursor = context.getContentResolver().query(
                Phone.CONTENT_URI, projection, null, null, null)) {
            if (cursor == null) {
                return new ArrayList<>();
            }
            while (cursor.moveToNext()) {
                String name = cursor.getString(1);
                String number = cursor.getString(2);
                if (name == null || number == null || number.trim().isEmpty()) {
                    continue;
                }
                long id = cursor.getLong(0);
                names.put(id, name);
                starred.put(id, cursor.getInt(5) != 0);
                List<Contact.Number> list = numbers.get(id);
                if (list == null) {
                    list = new ArrayList<>();
                    numbers.put(id, list);
                }
                list.add(new Contact.Number(number.trim(), kind(cursor.getInt(3)), cursor.getInt(4) != 0));
            }
        }

        List<Contact> contacts = new ArrayList<>();
        for (Map.Entry<Long, String> entry : names.entrySet()) {
            long id = entry.getKey();
            contacts.add(new Contact(entry.getValue(), starred.get(id), numbers.get(id)));
        }
        return contacts;
    }

    private static int kind(int type) {
        switch (type) {
            case Phone.TYPE_MOBILE:
            case Phone.TYPE_WORK_MOBILE:
                return Contact.KIND_MOBILE;
            case Phone.TYPE_HOME:
                return Contact.KIND_HOME;
            case Phone.TYPE_WORK:
            case Phone.TYPE_COMPANY_MAIN:
                return Contact.KIND_WORK;
            default:
                return Contact.KIND_OTHER;
        }
    }
}
