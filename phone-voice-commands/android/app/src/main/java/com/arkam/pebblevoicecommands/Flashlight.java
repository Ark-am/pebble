package com.arkam.pebblevoicecommands;

import android.content.Context;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/** The phone's torch. Needs no permission, only a camera with a flash. */
final class Flashlight extends CameraManager.TorchCallback {
    private static final String TAG = "PhoneVoiceCommands";

    private final CameraManager cameras;
    private final String cameraId;
    // Android reports the torch state on registration and on every change.
    private volatile boolean on;

    Flashlight(Context context) {
        cameras = context.getSystemService(CameraManager.class);
        cameraId = findCamera(cameras);
        if (cameraId != null) {
            cameras.registerTorchCallback(this, new Handler(Looper.getMainLooper()));
        }
    }

    boolean available() {
        return cameraId != null;
    }

    boolean isOn() {
        return on;
    }

    void set(boolean enabled) throws CameraAccessException {
        cameras.setTorchMode(cameraId, enabled);
        on = enabled;
    }

    void close() {
        if (cameraId != null) {
            cameras.unregisterTorchCallback(this);
        }
    }

    @Override
    public void onTorchModeChanged(String id, boolean enabled) {
        if (id.equals(cameraId)) {
            on = enabled;
        }
    }

    @Override
    public void onTorchModeUnavailable(String id) {
        if (id.equals(cameraId)) {
            on = false;
        }
    }

    /** Prefers the rear camera, since that is where the flash usually is. */
    private static String findCamera(CameraManager cameras) {
        if (cameras == null) {
            return null;
        }
        String fallback = null;
        try {
            for (String id : cameras.getCameraIdList()) {
                CameraCharacteristics characteristics = cameras.getCameraCharacteristics(id);
                if (!Boolean.TRUE.equals(characteristics.get(CameraCharacteristics.FLASH_INFO_AVAILABLE))) {
                    continue;
                }
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    return id;
                }
                if (fallback == null) {
                    fallback = id;
                }
            }
        }
        catch (CameraAccessException error) {
            Log.w(TAG, "Could not list cameras", error);
        }
        return fallback;
    }
}
