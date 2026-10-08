package com.arkam.pebbledialer;

import android.telecom.CallAudioState;
import android.telecom.InCallService;

/**
 * Android binds this service to every call once the Pebble is linked to this
 * app as a companion watch (see MainActivity; Android 12 and newer). The watch
 * profile grants MANAGE_ONGOING_CALLS, which is what Android checks before it
 * binds a non-UI InCallService like this one. It gives the watch control of
 * the call's audio: the output (phone earpiece, speaker, wired headset or
 * Bluetooth) and mute. It shows no UI; Android's own phone app still owns the
 * call.
 *
 * Android calls it on the main thread, as does DialerService, so the static
 * state below is only touched from there.
 */
public final class CallControlService extends InCallService {
    /** Told about the audio output and mute whenever they change during a call. */
    interface AudioListener {
        void onAudioChanged(CallAudioState state);
    }

    private static CallControlService running;
    private static AudioListener listener;

    @Override
    public void onCreate() {
        super.onCreate();
        running = this;
    }

    @Override
    public void onDestroy() {
        if (running == this) {
            running = null;
        }
        super.onDestroy();
    }

    @Override
    public void onCallAudioStateChanged(CallAudioState state) {
        if (listener != null && state != null) {
            listener.onAudioChanged(state);
        }
    }

    /** Starts (or stops, with null) reporting audio changes, starting with the current output. */
    @SuppressWarnings("deprecation")
    static void setListener(AudioListener audioListener) {
        listener = audioListener;
        if (listener != null && running != null && running.getCallAudioState() != null) {
            listener.onAudioChanged(running.getCallAudioState());
        }
    }

    /**
     * Moves the current call to a CallAudioState route. Returns false when
     * Android has not bound this service, which means the watch is not linked.
     */
    @SuppressWarnings("deprecation")
    static boolean setRoute(int route) {
        if (running == null || running.getCalls().isEmpty()) {
            return false;
        }
        // Deprecated from Android 14 in favour of call endpoints, but still
        // supported, and it works on every version this service runs on.
        running.setAudioRoute(route);
        return true;
    }

    /** Mutes or unmutes the current call; false when the watch is not linked. */
    static boolean setMuted(boolean muted) {
        if (running == null || running.getCalls().isEmpty()) {
            return false;
        }
        running.setMuted(muted);
        return true;
    }
}
