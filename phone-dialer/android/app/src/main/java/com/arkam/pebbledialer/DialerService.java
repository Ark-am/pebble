package com.arkam.pebbledialer;

import android.Manifest;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.TelecomManager;
import android.util.Log;

import org.jetbrains.annotations.NotNull;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

import io.rebble.pebblekit2.client.java.BaseJavaPebbleListenerService;
import io.rebble.pebblekit2.client.java.DefaultJavaPebbleSender;
import io.rebble.pebblekit2.client.java.JavaPebbleSender;
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem;
import io.rebble.pebblekit2.common.model.ReceiveResult;
import io.rebble.pebblekit2.common.model.TransmissionResult;
import io.rebble.pebblekit2.common.model.WatchIdentifier;

public final class DialerService extends BaseJavaPebbleListenerService {
    private static final String TAG = "PhoneDialer";
    static final UUID WATCHAPP_UUID =
            UUID.fromString("8269f312-4d4c-4149-95fa-9f5042fcf467");

    // Must match messageKeys in the watch app's package.json.
    private static final int KEY_REQUEST = 1;
    private static final int KEY_TOKEN = 2;
    private static final int KEY_LIST = 3;
    private static final int KEY_FILTER = 4;
    private static final int KEY_OFFSET = 5;
    private static final int KEY_LIMIT = 6;
    private static final int KEY_CAPACITY = 7;
    private static final int KEY_ITEM_ID = 8;
    private static final int KEY_RESULT = 9;
    private static final int KEY_TOTAL = 10;
    private static final int KEY_ITEMS = 11;
    private static final int KEY_FINAL = 12;
    private static final int KEY_NUMBER = 13;

    private static final int REQUEST_LIST = 1;
    private static final int REQUEST_CALL = 2;
    private static final int REQUEST_DIAL = 3;
    private static final int REQUEST_SYNC_SETTINGS = 4;

    private static final int LIST_FAVORITES = 0;
    private static final int LIST_LETTERS = 1;
    private static final int LIST_CONTACTS = 2;
    private static final int LIST_RECENTS = 3;

    private static final int RESULT_OK = 0;
    private static final int RESULT_FAILED = 1;
    private static final int RESULT_PERMISSION_REQUIRED = 2;
    private static final int RESULT_NOT_FOUND = 3;
    private static final int RESULT_CALL_LOG_PERMISSION = 4;
    private static final int RESULT_INVALID_NUMBER = 5;

    // Separators the watch splits ITEMS on; stripped from names before sending.
    private static final char FIELD_SEPARATOR = '\u001f';
    private static final char ENTRY_SEPARATOR = '\u001e';

    // Matches the watch's TITLE_SIZE and SUBTITLE_SIZE, less the terminator.
    private static final int MAX_FIELD_BYTES = 31;
    private static final int MAX_PAGE_SIZE = 50;
    private static final int MIN_CAPACITY = 200;
    private static final int MAX_CAPACITY = 4000;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    // Only the newest request's pages are worth sending.
    private final AtomicInteger latestToken = new AtomicInteger();
    private JavaPebbleSender sender;
    // Accessed only on executor. Retransmitted call messages must not dial twice.
    private final Map<String, Integer> callResults = new java.util.LinkedHashMap<String, Integer>() {
        @Override protected boolean removeEldestEntry(Map.Entry<String, Integer> eldest) {
            return size() > 64;
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        sender = new DefaultJavaPebbleSender(this);
    }

    @Override
    public void onDestroy() {
        executor.shutdownNow();
        try {
            sender.close();
        }
        catch (Exception error) {
            Log.w(TAG, "Could not close Pebble sender", error);
        }
        super.onDestroy();
    }

    @Override
    protected void onMessageReceived(
            @NotNull UUID watchappUUID,
            @NotNull Map<Integer, ? extends PebbleDictionaryItem> data,
            @NotNull String watch,
            @NotNull Consumer<ReceiveResult> responder) {
        if (!WATCHAPP_UUID.equals(watchappUUID)) {
            responder.accept(ReceiveResult.Nack.INSTANCE);
            return;
        }

        int request = intValue(data.get(KEY_REQUEST), -1);
        if (request == REQUEST_SYNC_SETTINGS) {
            // Not part of the token-ordered requests, so it never cancels a list in progress.
            responder.accept(ReceiveResult.Ack.INSTANCE);
            MenuOrder watchOrder = new MenuOrder(
                    textValue(data.get(MenuOrder.KEY_MENU_ORDER)),
                    intValue(data.get(MenuOrder.KEY_MENU_STAMP), 0));
            // Older watch apps do not send a theme; a stamp of 0 lets the phone's win.
            WatchTheme watchTheme = new WatchTheme(
                    intValue(data.get(WatchTheme.KEY_THEME), WatchTheme.DARK),
                    intValue(data.get(WatchTheme.KEY_THEME_STAMP), 0));
            executor.execute(() -> syncSettings(watch, watchOrder, watchTheme));
            return;
        }

        int token = intValue(data.get(KEY_TOKEN), 0);
        if (request != REQUEST_LIST && request != REQUEST_CALL && request != REQUEST_DIAL) {
            responder.accept(ReceiveResult.Nack.INSTANCE);
            return;
        }

        responder.accept(ReceiveResult.Ack.INSTANCE);
        latestToken.set(token);

        // Contact queries can be slow on large address books; keep them off the main thread.
        Map<Integer, PebbleDictionaryItem> requestData = new HashMap<>(data);
        executor.execute(() -> {
            if (request == REQUEST_LIST) {
                sendList(watch, token, requestData);
            } else {
                String callKey = watch + ":" + token;
                Integer result = callResults.get(callKey);
                if (result == null) {
                    result = placeCall(request, requestData);
                    callResults.put(callKey, result);
                }
                send(watch, token, Collections.singletonList(resultMessage(token, result)), 0);
            }
        });
    }

    /** Keeps whichever menu order and theme changed last, on both the phone and the watch. */
    private void syncSettings(String watch, MenuOrder watchOrder, WatchTheme watchTheme) {
        Map<Integer, PebbleDictionaryItem> reply = new HashMap<>();

        MenuOrder phoneOrder = MenuOrder.load(this);
        if (MenuOrder.isValid(watchOrder.order) && watchOrder.stamp > phoneOrder.stamp) {
            MenuOrder.save(this, watchOrder);
        } else if (phoneOrder.stamp > watchOrder.stamp) {
            reply.putAll(phoneOrder.message());
        }

        WatchTheme phoneTheme = WatchTheme.load(this);
        if (WatchTheme.isValid(watchTheme.theme) && watchTheme.stamp > phoneTheme.stamp) {
            WatchTheme.save(this, watchTheme);
        } else if (phoneTheme.stamp > watchTheme.stamp) {
            reply.putAll(phoneTheme.message());
        }

        if (!reply.isEmpty()) {
            sender.sendDataToPebble(WATCHAPP_UUID, reply, results -> { },
                    Collections.singletonList(new WatchIdentifier(watch)));
        }
    }

    private void sendList(String watch, int token, Map<Integer, PebbleDictionaryItem> request) {
        int list = intValue(request.get(KEY_LIST), -1);
        boolean recents = list == LIST_RECENTS;
        String permission = recents ? Manifest.permission.READ_CALL_LOG : Manifest.permission.READ_CONTACTS;
        if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            send(watch, token, Collections.singletonList(
                    resultMessage(token, recents ? RESULT_CALL_LOG_PERMISSION : RESULT_PERMISSION_REQUIRED)), 0);
            return;
        }

        List<String> entries;
        try {
            entries = listEntries(list, textValue(request.get(KEY_FILTER)));
        }
        catch (SecurityException error) {
            send(watch, token, Collections.singletonList(resultMessage(token,
                    recents ? RESULT_CALL_LOG_PERMISSION : RESULT_PERMISSION_REQUIRED)), 0);
            return;
        }
        catch (RuntimeException error) {
            Log.e(TAG, "Could not read list", error);
            send(watch, token, Collections.singletonList(resultMessage(token, RESULT_FAILED)), 0);
            return;
        }
        if (entries == null) {
            send(watch, token, Collections.singletonList(resultMessage(token, RESULT_FAILED)), 0);
            return;
        }

        int offset = clamp(intValue(request.get(KEY_OFFSET), 0), 0, entries.size());
        int limit = clamp(intValue(request.get(KEY_LIMIT), 20), 1, MAX_PAGE_SIZE);
        int capacity = clamp(intValue(request.get(KEY_CAPACITY), MIN_CAPACITY),
                MIN_CAPACITY, MAX_CAPACITY);
        int end = Math.min(entries.size(), offset + limit);

        // Pack as many entries into each message as the watch's inbox can hold.
        List<Map<Integer, PebbleDictionaryItem>> messages = new ArrayList<>();
        StringBuilder batch = new StringBuilder();
        int batchBytes = 0;
        int batchStart = offset;
        for (int i = offset; i < end; i++) {
            String entry = entries.get(i);
            int entryBytes = entry.getBytes(StandardCharsets.UTF_8).length + 1;
            if (batchBytes > 0 && batchBytes + entryBytes > capacity) {
                messages.add(pageMessage(token, entries.size(), batchStart, batch.toString()));
                batch.setLength(0);
                batchBytes = 0;
                batchStart = i;
            }
            if (batchBytes > 0) {
                batch.append(ENTRY_SEPARATOR);
            }
            batch.append(entry);
            batchBytes += entryBytes;
        }
        Map<Integer, PebbleDictionaryItem> last =
                pageMessage(token, entries.size(), batchStart, batch.toString());
        last.put(KEY_FINAL, new PebbleDictionaryItem.Int32(1));
        messages.add(last);

        send(watch, token, messages, 0);
    }

    /** Encoded "id, title, subtitle" entries for a whole list, or null for an unknown list. */
    private List<String> listEntries(int list, String filter) {
        List<String> entries = new ArrayList<>();
        switch (list) {
            case LIST_RECENTS:
                for (RecentCalls.Entry entry : RecentCalls.load(this)) {
                    entries.add(encode(entry.id, entry.title, entry.subtitle));
                }
                return entries;
            case LIST_FAVORITES:
                for (ContactDirectory.PhoneEntry entry : ContactDirectory.favorites(this)) {
                    entries.add(encodeNumber(entry));
                }
                return entries;
            case LIST_LETTERS:
                for (Map.Entry<String, Integer> group : ContactDirectory.groupSizes(this).entrySet()) {
                    int size = group.getValue();
                    entries.add(encode(0, group.getKey(),
                            size + (size == 1 ? " contact" : " contacts")));
                }
                return entries;
            case LIST_CONTACTS:
                if (filter == null || filter.isEmpty()) {
                    return null;
                }
                for (ContactDirectory.PhoneEntry entry : ContactDirectory.inGroup(this, filter)) {
                    entries.add(encodeNumber(entry));
                }
                return entries;
            default:
                return null;
        }
    }

    private int placeCall(int request, Map<Integer, PebbleDictionaryItem> data) {
        if (checkSelfPermission(Manifest.permission.CALL_PHONE) != PackageManager.PERMISSION_GRANTED) {
            return RESULT_PERMISSION_REQUIRED;
        }
        boolean recent = intValue(data.get(KEY_LIST), LIST_CONTACTS) == LIST_RECENTS;
        try {
            String number;
            if (request == REQUEST_DIAL) {
                number = textValue(data.get(KEY_NUMBER));
                if (!DialNumber.isValid(number)) {
                    return RESULT_INVALID_NUMBER;
                }
            } else {
                String permission = recent ? Manifest.permission.READ_CALL_LOG : Manifest.permission.READ_CONTACTS;
                if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                    return recent ? RESULT_CALL_LOG_PERMISSION : RESULT_PERMISSION_REQUIRED;
                }
                long id = idValue(data.get(KEY_ITEM_ID));
                if (id <= 0) {
                    return RESULT_NOT_FOUND;
                }
                number = recent ? RecentCalls.numberFor(this, id) : ContactDirectory.numberFor(this, id);
                if (number == null) {
                    return RESULT_NOT_FOUND;
                }
            }
            TelecomManager telecom = getSystemService(TelecomManager.class);
            if (telecom == null || !getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
                return RESULT_FAILED;
            }
            // The system owns the call UI and SIM choice; this does not launch
            // an activity from the background or replace the default dialer.
            telecom.placeCall(Uri.fromParts("tel", number, null), new Bundle());
            return RESULT_OK; // Accepted by Telecom, not a connected-call guarantee.
        }
        catch (SecurityException error) {
            Log.e(TAG, "Android rejected the call", error);
            return RESULT_PERMISSION_REQUIRED;
        }
        catch (RuntimeException error) {
            Log.e(TAG, "Could not place the call", error);
            return RESULT_FAILED;
        }
    }

    private static long idValue(PebbleDictionaryItem item) {
        if (item instanceof PebbleDictionaryItem.Int32) {
            return ((PebbleDictionaryItem.Int32) item).getValue();
        }
        try {
            return Long.parseLong(textValue(item));
        } catch (NumberFormatException error) {
            return -1;
        }
    }

    /** Sends messages one at a time, as each needs the watch's ACK before the next. */
    private void send(String watch, int token, List<Map<Integer, PebbleDictionaryItem>> messages,
                      int index) {
        if (index >= messages.size() || token != latestToken.get()) {
            return;
        }
        sender.sendDataToPebble(
                WATCHAPP_UUID,
                messages.get(index),
                results -> {
                    TransmissionResult result = results == null || results.isEmpty()
                            ? null
                            : results.values().iterator().next();
                    if (result instanceof TransmissionResult.Success) {
                        send(watch, token, messages, index + 1);
                    } else {
                        Log.w(TAG, "Stopped sending to the watch: " + result);
                    }
                },
                Collections.singletonList(new WatchIdentifier(watch)));
    }

    private static Map<Integer, PebbleDictionaryItem> resultMessage(int token, int result) {
        Map<Integer, PebbleDictionaryItem> message = new HashMap<>();
        message.put(KEY_TOKEN, new PebbleDictionaryItem.Int32(token));
        message.put(KEY_RESULT, new PebbleDictionaryItem.Int32(result));
        message.put(KEY_FINAL, new PebbleDictionaryItem.Int32(1));
        return message;
    }

    private static Map<Integer, PebbleDictionaryItem> pageMessage(int token, int total, int offset,
                                                                  String items) {
        Map<Integer, PebbleDictionaryItem> message = new HashMap<>();
        message.put(KEY_TOKEN, new PebbleDictionaryItem.Int32(token));
        message.put(KEY_RESULT, new PebbleDictionaryItem.Int32(RESULT_OK));
        message.put(KEY_TOTAL, new PebbleDictionaryItem.Int32(total));
        message.put(KEY_OFFSET, new PebbleDictionaryItem.Int32(offset));
        message.put(KEY_ITEMS, new PebbleDictionaryItem.Text(items));
        return message;
    }

    private static String encodeNumber(ContactDirectory.PhoneEntry entry) {
        String subtitle = entry.label.isEmpty() ? entry.number : entry.label + " " + entry.number;
        return encode(entry.dataId, entry.name, subtitle);
    }

    private static String encode(long id, String title, String subtitle) {
        return String.valueOf(id) + FIELD_SEPARATOR + field(title) + FIELD_SEPARATOR + field(subtitle);
    }

    /** Removes separators and control characters and trims to whole UTF-8 characters. */
    private static String field(String text) {
        StringBuilder clean = new StringBuilder();
        int bytes = 0;
        for (int i = 0; i < text.length(); ) {
            int codePoint = text.codePointAt(i);
            i += Character.charCount(codePoint);
            if (Character.isISOControl(codePoint)) {
                codePoint = ' ';
            }
            int size = new String(Character.toChars(codePoint))
                    .getBytes(StandardCharsets.UTF_8).length;
            if (bytes + size > MAX_FIELD_BYTES) {
                break;
            }
            clean.appendCodePoint(codePoint);
            bytes += size;
        }
        return clean.toString().trim();
    }

    private static int intValue(PebbleDictionaryItem item, int fallback) {
        return item instanceof PebbleDictionaryItem.Int32
                ? ((PebbleDictionaryItem.Int32) item).getValue()
                : fallback;
    }

    private static String textValue(PebbleDictionaryItem item) {
        return item instanceof PebbleDictionaryItem.Text
                ? ((PebbleDictionaryItem.Text) item).getValue()
                : null;
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }
}
