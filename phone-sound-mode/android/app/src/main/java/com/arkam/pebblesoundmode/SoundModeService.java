package com.arkam.pebblesoundmode;

import android.app.NotificationManager;
import android.content.Context;
import android.media.AudioManager;
import android.util.Log;

import org.jetbrains.annotations.NotNull;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import java.util.function.Consumer;

import io.rebble.pebblekit2.client.java.BaseJavaPebbleListenerService;
import io.rebble.pebblekit2.client.java.DefaultJavaPebbleSender;
import io.rebble.pebblekit2.client.java.JavaPebbleSender;
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem;
import io.rebble.pebblekit2.common.model.ReceiveResult;
import io.rebble.pebblekit2.common.model.WatchIdentifier;

public final class SoundModeService extends BaseJavaPebbleListenerService {
    private static final String TAG = "PhoneSoundMode";
    private static final UUID WATCHAPP_UUID =
            UUID.fromString("9e7fbef8-d934-4538-ae25-11f29029b3a4");

    private static final int KEY_COMMAND = 1;
    private static final int KEY_RESULT = 2;
    private static final int KEY_CURRENT_MODE = 3;

    private static final int MODE_SILENT = 0;
    private static final int MODE_VIBRATE = 1;
    private static final int MODE_NORMAL = 2;

    private static final int RESULT_OK = 0;
    private static final int RESULT_FAILED = 1;
    private static final int RESULT_PERMISSION_REQUIRED = 2;

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

        PebbleDictionaryItem item = data.get(KEY_COMMAND);
        if (!(item instanceof PebbleDictionaryItem.Int32)) {
            responder.accept(ReceiveResult.Nack.INSTANCE);
            return;
        }

        int requestedMode = ((PebbleDictionaryItem.Int32) item).getValue();
        int result = applyMode(requestedMode);
        int currentMode = getCurrentMode();

        responder.accept(ReceiveResult.Ack.INSTANCE);
        sendResult(watch, result, currentMode);
    }

    private int applyMode(int requestedMode) {
        if (requestedMode < MODE_SILENT || requestedMode > MODE_NORMAL) {
            return RESULT_FAILED;
        }

        NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (!notificationManager.isNotificationPolicyAccessGranted()) {
            return RESULT_PERMISSION_REQUIRED;
        }

        AudioManager audioManager =
                (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager.isVolumeFixed()) {
            return RESULT_FAILED;
        }

        int androidMode;
        switch (requestedMode) {
            case MODE_SILENT:
                androidMode = AudioManager.RINGER_MODE_SILENT;
                break;
            case MODE_VIBRATE:
                androidMode = AudioManager.RINGER_MODE_VIBRATE;
                break;
            case MODE_NORMAL:
                androidMode = AudioManager.RINGER_MODE_NORMAL;
                break;
            default:
                return RESULT_FAILED;
        }

        try {
            audioManager.setRingerMode(androidMode);
            return audioManager.getRingerMode() == androidMode
                    ? RESULT_OK
                    : RESULT_FAILED;
        }
        catch (SecurityException error) {
            Log.e(TAG, "Android rejected the ringer-mode change", error);
            return RESULT_PERMISSION_REQUIRED;
        }
    }

    private int getCurrentMode() {
        AudioManager audioManager =
                (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        switch (audioManager.getRingerMode()) {
            case AudioManager.RINGER_MODE_SILENT:
                return MODE_SILENT;
            case AudioManager.RINGER_MODE_VIBRATE:
                return MODE_VIBRATE;
            case AudioManager.RINGER_MODE_NORMAL:
            default:
                return MODE_NORMAL;
        }
    }

    private void sendResult(String watch, int result, int currentMode) {
        Map<Integer, PebbleDictionaryItem> response = new HashMap<>();
        response.put(KEY_RESULT, new PebbleDictionaryItem.Int32(result));
        response.put(KEY_CURRENT_MODE, new PebbleDictionaryItem.Int32(currentMode));

        JavaPebbleSender sender = new DefaultJavaPebbleSender(this);
        sender.sendDataToPebble(
                WATCHAPP_UUID,
                response,
                transmission -> {
                    try {
                        sender.close();
                    }
                    catch (Exception error) {
                        Log.w(TAG, "Could not close Pebble sender", error);
                    }
                },
                Collections.singletonList(new WatchIdentifier(watch)));
    }
}
