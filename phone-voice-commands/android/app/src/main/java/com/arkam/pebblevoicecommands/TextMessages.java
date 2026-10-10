package com.arkam.pebblevoicecommands;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.telephony.SmsManager;
import android.util.Log;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Sends a text with Android's default SIM and waits briefly to hear whether
 * it left the phone. Needs SEND_SMS. Android copies the message into the
 * messaging app's history, because this is not the default SMS app.
 */
final class TextMessages {
    enum Outcome { SENT, FAILED, PENDING }

    private static final String TAG = "PhoneVoiceCommands";
    private static final String ACTION_SENT = "com.arkam.pebblevoicecommands.SMS_SENT";
    // The watch gives up after 10 seconds, so leave time for the reply.
    private static final long WAIT_SECONDS = 6;

    private TextMessages() {
    }

    /** Blocks for up to {@link #WAIT_SECONDS}; never call on the main thread. */
    @SuppressWarnings("deprecation")
    static Outcome send(Context context, String number, String body) {
        SmsManager sms = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                ? context.getSystemService(SmsManager.class)
                : SmsManager.getDefault();
        if (sms == null) {
            return Outcome.FAILED;
        }

        ArrayList<String> parts = sms.divideMessage(body);
        CountDownLatch remaining = new CountDownLatch(parts.size());
        AtomicInteger failures = new AtomicInteger();
        // A fresh action per message, so a late result is never counted twice.
        String action = ACTION_SENT + "." + System.nanoTime();
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ignored, Intent intent) {
                if (getResultCode() != Activity.RESULT_OK) {
                    Log.w(TAG, "Text part failed with code " + getResultCode());
                    failures.incrementAndGet();
                }
                remaining.countDown();
            }
        };
        IntentFilter filter = new IntentFilter(action);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            context.registerReceiver(receiver, filter);
        }

        try {
            ArrayList<PendingIntent> sent = new ArrayList<>();
            for (int i = 0; i < parts.size(); i++) {
                Intent intent = new Intent(action).setPackage(context.getPackageName());
                sent.add(PendingIntent.getBroadcast(context, i, intent,
                        PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_ONE_SHOT));
            }
            if (parts.size() == 1) {
                sms.sendTextMessage(number, null, body, sent.get(0), null);
            } else {
                sms.sendMultipartTextMessage(number, null, parts, sent, null);
            }

            if (!remaining.await(WAIT_SECONDS, TimeUnit.SECONDS)) {
                return Outcome.PENDING;
            }
            return failures.get() == 0 ? Outcome.SENT : Outcome.FAILED;
        }
        catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            return Outcome.PENDING;
        }
        catch (IllegalArgumentException error) {
            Log.w(TAG, "Android rejected the text", error);
            return Outcome.FAILED;
        }
        finally {
            context.unregisterReceiver(receiver);
        }
    }
}
