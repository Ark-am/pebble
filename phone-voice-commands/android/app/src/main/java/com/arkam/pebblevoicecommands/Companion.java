package com.arkam.pebblevoicecommands;

import android.companion.CompanionDeviceManager;
import android.content.Context;
import android.os.Build;

/**
 * Android 10 and newer stop background services from opening activities, which
 * timers, alarms and app launches need. Apps linked to a companion device, such
 * as a watch, are exempt, so the Pebble must be linked first.
 */
final class Companion {
    private Companion() {
    }

    static boolean linkNeeded() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    }

    /** Whether this app may open activities from the background. */
    static boolean canStartActivities(Context context) {
        return !linkNeeded() || linked(context);
    }

    @SuppressWarnings("deprecation")
    static boolean linked(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }
        CompanionDeviceManager manager = context.getSystemService(CompanionDeviceManager.class);
        if (manager == null) {
            return false;
        }
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                ? !manager.getMyAssociations().isEmpty()
                : !manager.getAssociations().isEmpty();
    }
}
