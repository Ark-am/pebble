package com.arkam.pebbledialer;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

import io.rebble.pebblekit2.client.java.DefaultJavaPebbleSender;
import io.rebble.pebblekit2.client.java.JavaPebbleSender;
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem;

/**
 * The order of the watch's main menu, shared between the watch and this app.
 * Both sides keep the order with the time it last changed, and the newer one wins.
 */
final class MenuOrder {
    // Must match messageKeys in the watch app's package.json.
    static final int KEY_MENU_ORDER = 14;
    static final int KEY_MENU_STAMP = 15;

    /** Row names, indexed by the digits the watch uses ("0123" is the default). */
    static final String[] LABELS = { "Dialer", "Recent calls", "Favorites", "Contacts" };
    static final String DEFAULT_ORDER = "0123";

    private static final String TAG = "PhoneDialer";
    private static final String PREFERENCES = "menu";
    private static final String PREF_ORDER = "order";
    private static final String PREF_STAMP = "stamp";

    final String order;
    final int stamp;

    MenuOrder(String order, int stamp) {
        this.order = order;
        this.stamp = stamp;
    }

    /** A complete arrangement of the known rows, one digit each. */
    static boolean isValid(String order) {
        if (order == null || order.length() != LABELS.length) {
            return false;
        }
        boolean[] seen = new boolean[LABELS.length];
        for (int i = 0; i < order.length(); i++) {
            int row = order.charAt(i) - '0';
            if (row < 0 || row >= LABELS.length || seen[row]) {
                return false;
            }
            seen[row] = true;
        }
        return true;
    }

    static synchronized MenuOrder load(Context context) {
        SharedPreferences preferences = preferences(context);
        String order = preferences.getString(PREF_ORDER, DEFAULT_ORDER);
        return isValid(order)
                ? new MenuOrder(order, preferences.getInt(PREF_STAMP, 0))
                : new MenuOrder(DEFAULT_ORDER, 0);
    }

    static synchronized void save(Context context, MenuOrder menuOrder) {
        preferences(context).edit()
                .putString(PREF_ORDER, menuOrder.order)
                .putInt(PREF_STAMP, menuOrder.stamp)
                .apply();
    }

    /** A new order chosen on the phone, stamped now so it beats the watch's older one. */
    static MenuOrder changedNow(String order) {
        return new MenuOrder(order, (int) (System.currentTimeMillis() / 1000));
    }

    Map<Integer, PebbleDictionaryItem> message() {
        Map<Integer, PebbleDictionaryItem> message = new HashMap<>();
        message.put(KEY_MENU_ORDER, new PebbleDictionaryItem.Text(order));
        message.put(KEY_MENU_STAMP, new PebbleDictionaryItem.Int32(stamp));
        return message;
    }

    /**
     * Sends settings to the watch app if it is open. When it is not, the watch
     * picks them up the next time it starts.
     */
    static void pushToWatch(Context context, Map<Integer, PebbleDictionaryItem> message) {
        JavaPebbleSender sender = new DefaultJavaPebbleSender(context.getApplicationContext());
        sender.sendDataToPebble(DialerService.WATCHAPP_UUID, message, results -> {
            try {
                sender.close();
            }
            catch (Exception error) {
                Log.w(TAG, "Could not close Pebble sender", error);
            }
        });
    }

    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }
}
