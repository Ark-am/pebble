package com.arkam.pebblevoicecommands;

import android.util.Log;

import org.jetbrains.annotations.NotNull;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;

import io.rebble.pebblekit2.client.java.BaseJavaPebbleListenerService;
import io.rebble.pebblekit2.client.java.DefaultJavaPebbleSender;
import io.rebble.pebblekit2.client.java.JavaPebbleSender;
import io.rebble.pebblekit2.common.model.PebbleDictionaryItem;
import io.rebble.pebblekit2.common.model.ReceiveResult;
import io.rebble.pebblekit2.common.model.WatchIdentifier;

public final class VoiceCommandService extends BaseJavaPebbleListenerService {
    private static final String TAG = "PhoneVoiceCommands";
    private static final UUID WATCHAPP_UUID =
            UUID.fromString("01430625-332e-46aa-b212-89d36377c0a5");

    private static final int KEY_TRANSCRIPT = 1;
    private static final int KEY_RESULT = 2;
    private static final int KEY_REPLY = 3;

    // Commands run in order, off the main thread, one at a time.
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private JavaPebbleSender sender;
    private Flashlight flashlight;
    private CommandRouter router;

    @Override
    public void onCreate() {
        super.onCreate();
        sender = new DefaultJavaPebbleSender(this);
        flashlight = new Flashlight(this);
        router = new CommandRouter(this, flashlight);
    }

    @Override
    public void onDestroy() {
        executor.shutdownNow();
        flashlight.close();
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

        PebbleDictionaryItem item = data.get(KEY_TRANSCRIPT);
        if (!(item instanceof PebbleDictionaryItem.Text)) {
            responder.accept(ReceiveResult.Nack.INSTANCE);
            return;
        }

        String transcript = ((PebbleDictionaryItem.Text) item).getValue();
        responder.accept(ReceiveResult.Ack.INSTANCE);
        executor.execute(() -> {
            CommandResult result;
            try {
                result = router.handle(transcript);
            }
            catch (RuntimeException error) {
                Log.e(TAG, "Command failed: " + transcript, error);
                result = CommandResult.failed("Something went wrong");
            }
            CommandLog.record(this, transcript, result);
            sendResult(watch, result);
        });
    }

    private void sendResult(String watch, CommandResult result) {
        Map<Integer, PebbleDictionaryItem> response = new HashMap<>();
        response.put(KEY_RESULT, new PebbleDictionaryItem.Int32(result.code));
        response.put(KEY_REPLY, new PebbleDictionaryItem.Text(
                WatchText.fit(result.reply, WatchText.MAX_REPLY_BYTES)));

        sender.sendDataToPebble(
                WATCHAPP_UUID,
                response,
                transmission -> { },
                Collections.singletonList(new WatchIdentifier(watch)));
    }
}
