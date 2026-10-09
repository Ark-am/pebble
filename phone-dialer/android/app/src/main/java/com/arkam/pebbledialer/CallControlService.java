package com.arkam.pebbledialer;

import android.annotation.TargetApi;
import android.os.Build;
import android.telecom.Call;
import android.telecom.CallAudioState;
import android.telecom.InCallService;
import android.telephony.PhoneNumberUtils;

import java.util.List;

/**
 * Android binds this service to every call once the Pebble is linked to this
 * app as a companion watch (see MainActivity; Android 12 and newer). The watch
 * profile grants MANAGE_ONGOING_CALLS, which is what Android checks before it
 * binds a non-UI InCallService like this one. It gives the watch control of
 * the call's audio, the output (phone earpiece, speaker, wired headset or
 * Bluetooth) and mute, lets it end the call, and reports how the call is
 * going. It shows no UI; Android's own phone app still owns the call.
 *
 * Android calls it on the main thread, as does DialerService, so the static
 * state below is only touched from there.
 */
// Android only binds it from Android 12 (see above), so it may use 12's APIs.
@TargetApi(Build.VERSION_CODES.S)
public final class CallControlService extends InCallService {
    /** Told about the audio output and mute whenever they change during a call. */
    interface AudioListener {
        void onAudioChanged(CallAudioState state);
    }

    /** Told how the watch's call is going, and when it ends. */
    interface CallListener {
        /** A Call.STATE_* value, and when the call connected (0 until then). */
        void onCallState(int state, long connectedAtMillis);

        void onCallEnded();
    }

    private static CallControlService running;
    private static AudioListener listener;
    private static CallListener callListener;
    // The number the watch dialled, to tell its call from any other.
    private static String callNumber;

    private final Call.Callback callCallback = new Call.Callback() {
        @Override
        public void onStateChanged(Call call, int state) {
            reportCall(call);
        }
    };

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
    public void onCallAdded(Call call) {
        call.registerCallback(callCallback);
        reportCall(call);
    }

    @Override
    public void onCallRemoved(Call call) {
        call.unregisterCallback(callCallback);
        if (callListener != null && isWatchCall(call)) {
            callListener.onCallEnded();
        }
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
     * Starts (or stops, with null) reporting the state of the call to number,
     * starting with its current state if Android has already bound the service.
     */
    static void setCallListener(String number, CallListener listener) {
        callNumber = number;
        callListener = listener;
        if (listener != null && running != null) {
            for (Call call : running.getCalls()) {
                reportCall(call);
            }
        }
    }

    private static void reportCall(Call call) {
        if (callListener != null && isWatchCall(call)) {
            callListener.onCallState(call.getDetails().getState(),
                    call.getDetails().getConnectTimeMillis());
        }
    }

    /**
     * Whether this is the call the watch placed: the call to its number, or
     * the only call, in case the network reports the number differently.
     */
    private static boolean isWatchCall(Call call) {
        if (callNumber == null || running == null) {
            return false;
        }
        return sameNumber(call, callNumber) || running.getCalls().size() == 1;
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

    /**
     * Ends the call the watch placed: the one to this number if there is one,
     * otherwise the newest call that is not ringing. A ringing call is left
     * alone, so a call waiting on top is never rejected by mistake. Returns
     * false when there is no such call, or the watch is not linked.
     */
    static boolean disconnect(String number) {
        if (running == null) {
            return false;
        }
        List<Call> calls = running.getCalls();
        Call fallback = null;
        for (Call call : calls) {
            int state = call.getDetails().getState();
            if (state == Call.STATE_RINGING || state == Call.STATE_DISCONNECTING
                    || state == Call.STATE_DISCONNECTED) {
                continue;
            }
            if (number != null && sameNumber(call, number)) {
                call.disconnect();
                return true;
            }
            fallback = call;
        }
        if (fallback == null) {
            return false;
        }
        fallback.disconnect();
        return true;
    }

    private static boolean sameNumber(Call call, String number) {
        if (call.getDetails().getHandle() == null) {
            return false;
        }
        String handle = call.getDetails().getHandle().getSchemeSpecificPart();
        return handle != null && PhoneNumberUtils.normalizeNumber(handle)
                .equals(PhoneNumberUtils.normalizeNumber(number));
    }

    /** Mutes or unmutes the current call; false when the watch is not linked. */
    static boolean requestMute(boolean muted) {
        if (running == null || running.getCalls().isEmpty()) {
            return false;
        }
        running.setMuted(muted);
        return true;
    }
}
