package com.arkam.pebblevoicecommands;

import android.app.KeyguardManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.hardware.camera2.CameraAccessException;
import android.media.AudioManager;
import android.provider.AlarmClock;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.KeyEvent;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;

/** Works out what a dictated command asks for and carries it out. */
final class CommandRouter {
    private static final String TAG = "PhoneVoiceCommands";
    // The longest timer Android's AlarmClock contract accepts.
    private static final int MAX_TIMER_SECONDS = 24 * 60 * 60;

    private final Context context;
    private final Flashlight flashlight;

    CommandRouter(Context context, Flashlight flashlight) {
        this.context = context;
        this.flashlight = flashlight;
    }

    /** Runs on a background thread, so actions may block briefly. */
    CommandResult handle(String transcript) {
        Calendar now = Calendar.getInstance();
        int nowMinutes = now.get(Calendar.HOUR_OF_DAY) * 60 + now.get(Calendar.MINUTE);
        ParsedCommand command = CommandParser.parse(transcript, nowMinutes);
        if (command == null) {
            return CommandResult.notUnderstood("Sorry, I can't do that yet");
        }
        Log.i(TAG, "Running " + command);

        switch (command.action) {
            case HELP:
                return CommandResult.ok("Try: flashlight on, timer 5 minutes, next song, open Maps");
            case FLASHLIGHT_ON:
                return setFlashlight(true);
            case FLASHLIGHT_OFF:
                return setFlashlight(false);
            case FLASHLIGHT_TOGGLE:
                return setFlashlight(!flashlight.isOn());
            case TIMER:
                return setTimer(command.value);
            case ALARM:
                return setAlarm(command.value);
            case VOLUME_UP:
                return adjustVolume(AudioManager.ADJUST_RAISE);
            case VOLUME_DOWN:
                return adjustVolume(AudioManager.ADJUST_LOWER);
            case VOLUME_SET:
                return setVolume(command.value);
            case VOLUME_MUTE:
                return adjustVolume(AudioManager.ADJUST_MUTE);
            case VOLUME_UNMUTE:
                return adjustVolume(AudioManager.ADJUST_UNMUTE);
            case MEDIA_PLAY:
                return pressMediaKey(KeyEvent.KEYCODE_MEDIA_PLAY, "Playing");
            case MEDIA_PAUSE:
                return pressMediaKey(KeyEvent.KEYCODE_MEDIA_PAUSE, "Paused");
            case MEDIA_NEXT:
                return pressMediaKey(KeyEvent.KEYCODE_MEDIA_NEXT, "Next track");
            case MEDIA_PREVIOUS:
                return pressMediaKey(KeyEvent.KEYCODE_MEDIA_PREVIOUS, "Previous track");
            case OPEN_APP:
                return openApp(command.text);
            default:
                return CommandResult.notUnderstood("Sorry, I can't do that yet");
        }
    }

    private CommandResult setFlashlight(boolean enabled) {
        if (!flashlight.available()) {
            return CommandResult.failed("This phone has no flashlight");
        }
        try {
            flashlight.set(enabled);
            return CommandResult.ok(enabled ? "Flashlight on" : "Flashlight off");
        }
        catch (CameraAccessException error) {
            Log.w(TAG, "Torch unavailable", error);
            return CommandResult.failed("Camera is in use");
        }
    }

    private CommandResult setTimer(int seconds) {
        if (seconds <= 0) {
            return CommandResult.notUnderstood("How long? Say \"timer for 5 minutes\"");
        }
        if (seconds > MAX_TIMER_SECONDS) {
            return CommandResult.failed("Timers can be up to 24 hours");
        }
        Intent intent = new Intent(AlarmClock.ACTION_SET_TIMER)
                .putExtra(AlarmClock.EXTRA_LENGTH, seconds)
                .putExtra(AlarmClock.EXTRA_SKIP_UI, true);
        return startActivity(intent, "Timer: " + Formats.duration(seconds), "No clock app found");
    }

    private CommandResult setAlarm(int minutesOfDay) {
        if (minutesOfDay < 0) {
            return CommandResult.notUnderstood("What time? Say \"alarm for 7 am\"");
        }
        Intent intent = new Intent(AlarmClock.ACTION_SET_ALARM)
                .putExtra(AlarmClock.EXTRA_HOUR, minutesOfDay / 60)
                .putExtra(AlarmClock.EXTRA_MINUTES, minutesOfDay % 60)
                .putExtra(AlarmClock.EXTRA_SKIP_UI, true);
        String time = Formats.clock(minutesOfDay, DateFormat.is24HourFormat(context));
        return startActivity(intent, "Alarm set for " + time, "No clock app found");
    }

    private CommandResult adjustVolume(int direction) {
        AudioManager audio = context.getSystemService(AudioManager.class);
        try {
            audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, direction, 0);
        }
        catch (SecurityException error) {
            // Thrown when Do Not Disturb blocks the change.
            Log.w(TAG, "Volume change refused", error);
            return CommandResult.failed("Android blocked the change");
        }
        if (direction == AudioManager.ADJUST_MUTE) {
            return CommandResult.ok("Media muted");
        }
        return CommandResult.ok("Volume " + volumePercent(audio) + "%");
    }

    private CommandResult setVolume(int percent) {
        AudioManager audio = context.getSystemService(AudioManager.class);
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        try {
            audio.setStreamVolume(AudioManager.STREAM_MUSIC, Math.round(percent * max / 100f), 0);
        }
        catch (SecurityException error) {
            Log.w(TAG, "Volume change refused", error);
            return CommandResult.failed("Android blocked the change");
        }
        return CommandResult.ok("Volume " + volumePercent(audio) + "%");
    }

    private static int volumePercent(AudioManager audio) {
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int current = audio.getStreamVolume(AudioManager.STREAM_MUSIC);
        return max == 0 ? 0 : Math.round(current * 100f / max);
    }

    /** Goes to whichever app is playing, or last played, media. */
    private CommandResult pressMediaKey(int keyCode, String reply) {
        AudioManager audio = context.getSystemService(AudioManager.class);
        audio.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
        audio.dispatchMediaKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
        return CommandResult.ok(reply);
    }

    private CommandResult openApp(String spokenName) {
        PackageManager packages = context.getPackageManager();
        Intent launcher = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
        List<ResolveInfo> apps = packages.queryIntentActivities(launcher, 0);
        List<String> labels = new ArrayList<>();
        for (ResolveInfo app : apps) {
            labels.add(String.valueOf(app.loadLabel(packages)));
        }

        int index = AppMatcher.best(spokenName, labels);
        if (index < 0) {
            return CommandResult.notUnderstood("No app called " + spokenName);
        }
        Intent intent = packages.getLaunchIntentForPackage(apps.get(index).activityInfo.packageName);
        if (intent == null) {
            return CommandResult.failed("Could not open " + labels.get(index));
        }

        KeyguardManager keyguard = context.getSystemService(KeyguardManager.class);
        String reply = keyguard != null && keyguard.isKeyguardLocked()
                ? "Unlock phone to see " + labels.get(index)
                : "Opened " + labels.get(index);
        return startActivity(intent, reply, "Could not open " + labels.get(index));
    }

    private CommandResult startActivity(Intent intent, String success, String missing) {
        // Android drops background activity starts silently, so check first.
        if (!Companion.canStartActivities(context)) {
            return CommandResult.permissionRequired("Link your Pebble in the phone app");
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(intent);
            return CommandResult.ok(success);
        }
        catch (ActivityNotFoundException error) {
            return CommandResult.failed(missing);
        }
        catch (SecurityException error) {
            Log.w(TAG, "Activity start refused", error);
            return CommandResult.failed(missing);
        }
    }
}
