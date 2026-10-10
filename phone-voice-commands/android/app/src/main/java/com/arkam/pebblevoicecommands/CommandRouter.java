package com.arkam.pebblevoicecommands;

import android.Manifest;
import android.app.KeyguardManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.hardware.camera2.CameraAccessException;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.AlarmClock;
import android.telecom.TelecomManager;
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
    private static final String GOOGLE_MAPS = "com.google.android.apps.maps";

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
                return CommandResult.ok("Try: call Mom, timer 5 minutes, next song, open Maps");
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
            case CALL_CONTACT:
                return callContact(command.text, command.value);
            case CALL_NUMBER:
                return placeCall(command.text, command.text);
            case SEND_TEXT:
                return sendText(command.text);
            case NAVIGATE:
                return navigate(command.text, command.value == 1);
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

    private CommandResult callContact(String spokenName, int kind) {
        if (!granted(Manifest.permission.READ_CONTACTS)) {
            return CommandResult.permissionRequired("Allow Contacts in the phone app");
        }
        List<Contact> contacts = Contacts.load(context);
        Found found = findContact(spokenName, contacts);
        if (found.problem != null) {
            return found.problem;
        }
        Contact contact = found.contact;
        Contact.Number number = contact.pick(kind, false);
        if (number == null) {
            return CommandResult.failed("No " + Contact.kindName(kind) + " number for " + contact.name);
        }
        String who = kind == Contact.KIND_ANY && contact.numbers.size() > 1
                ? contact.name + " (" + Contact.kindName(number.kind) + ")"
                : contact.name;
        return placeCall(number.value, who);
    }

    /** Telecom owns the call screen, so this needs no background activity start. */
    private CommandResult placeCall(String number, String who) {
        if (!granted(Manifest.permission.CALL_PHONE)) {
            return CommandResult.permissionRequired("Allow Phone in the phone app");
        }
        TelecomManager telecom = context.getSystemService(TelecomManager.class);
        if (telecom == null || !context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return CommandResult.failed("This phone cannot make calls");
        }
        try {
            telecom.placeCall(Uri.fromParts("tel", number, null), new Bundle());
            return CommandResult.ok("Calling " + who);
        }
        catch (SecurityException error) {
            Log.w(TAG, "Android rejected the call", error);
            return CommandResult.permissionRequired("Allow Phone in the phone app");
        }
    }

    private CommandResult sendText(String dictated) {
        if (!granted(Manifest.permission.SEND_SMS)) {
            return CommandResult.permissionRequired("Allow SMS in the phone app");
        }
        if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY_MESSAGING)) {
            return CommandResult.failed("This phone cannot send texts");
        }

        // Contacts are needed to tell where the name ends, except for a number.
        boolean canReadContacts = granted(Manifest.permission.READ_CONTACTS);
        List<Contact> contacts = canReadContacts ? Contacts.load(context) : new ArrayList<>();
        MessageSplitter.Split split = MessageSplitter.split(
                dictated, name -> ContactMatcher.matchesAny(name, contacts));
        if (!split.isNumber && !canReadContacts) {
            return CommandResult.permissionRequired("Allow Contacts in the phone app");
        }
        if (split.body.isEmpty()) {
            return CommandResult.notUnderstood("What should it say? Say \"text Sam on my way\"");
        }

        String number;
        String who;
        if (split.isNumber) {
            number = split.recipient;
            who = split.recipient;
        } else {
            Found found = findContact(split.recipient, contacts);
            if (found.problem != null) {
                return found.problem;
            }
            Contact contact = found.contact;
            Contact.Number picked = contact.pick(Contact.KIND_ANY, true);
            if (picked == null) {
                return CommandResult.failed("No number for " + contact.name);
            }
            number = picked.value;
            who = contact.name;
        }

        switch (TextMessages.send(context, number, split.body)) {
            case SENT:
                return CommandResult.ok("Text sent to " + who);
            case PENDING:
                return CommandResult.ok("Sending to " + who);
            default:
                return CommandResult.failed("Text to " + who + " not sent");
        }
    }

    /** A matched contact, or the reply explaining why there is none. */
    private static final class Found {
        final Contact contact;
        final CommandResult problem;

        Found(Contact contact, CommandResult problem) {
            this.contact = contact;
            this.problem = problem;
        }
    }

    private static Found findContact(String spokenName, List<Contact> contacts) {
        ContactMatcher.Match match = ContactMatcher.best(spokenName, contacts);
        if (match.index >= 0) {
            return new Found(contacts.get(match.index), null);
        }
        if (match.ties.isEmpty()) {
            return new Found(null, CommandResult.notUnderstood("No contact called " + spokenName));
        }
        int shown = Math.min(3, match.ties.size());
        StringBuilder names = new StringBuilder();
        for (int i = 0; i < shown; i++) {
            names.append(i == 0 ? "" : i == shown - 1 ? " or " : ", ")
                    .append(contacts.get(match.ties.get(i)).name);
        }
        return new Found(null, CommandResult.notUnderstood("Which one? " + names));
    }

    private CommandResult navigate(String destination, boolean walking) {
        PackageManager packages = context.getPackageManager();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(
                "google.navigation:q=" + Uri.encode(destination) + (walking ? "&mode=w" : "")))
                .setPackage(GOOGLE_MAPS);
        if (intent.resolveActivity(packages) == null) {
            // Other maps apps understand a geo: search instead of turn-by-turn.
            intent = new Intent(Intent.ACTION_VIEW,
                    Uri.parse("geo:0,0?q=" + Uri.encode(destination)));
        }
        return startActivity(intent, "Directions to " + destination, "No maps app found");
    }

    private boolean granted(String permission) {
        return context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
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
