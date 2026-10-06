package com.arkam.pebbledialer;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final String[] PERMISSIONS = {
            Manifest.permission.CALL_PHONE,
            Manifest.permission.READ_CONTACTS,
            Manifest.permission.READ_CALL_LOG,
    };
    private static final String[] LABELS = { "Phone", "Contacts", "Call history" };
    private final Button[] permissionButtons = new Button[PERMISSIONS.length];
    private TextView statusView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setFitsSystemWindows(true);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(padding, padding, padding, padding);
        scroll.addView(layout);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        TextView instructions = new TextView(this);
        instructions.setText(R.string.setup_instructions);
        instructions.setTextSize(16);
        instructions.setPadding(0, padding, 0, padding);
        layout.addView(instructions);

        statusView = new TextView(this);
        statusView.setTextSize(17);
        statusView.setPadding(0, 0, 0, padding);
        layout.addView(statusView);

        for (int i = 0; i < PERMISSIONS.length; i++) {
            final int index = i;
            Button button = new Button(this);
            button.setOnClickListener(view -> requestAccess(index));
            permissionButtons[i] = button;
            layout.addView(button, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        TextView help = new TextView(this);
        help.setText(R.string.permission_help);
        help.setTextSize(14);
        help.setPadding(0, padding, 0, 0);
        layout.addView(help);
        setContentView(scroll);
    }

    private boolean granted(String permission) {
        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    private boolean blocked(String permission) {
        return !granted(permission)
                && getPreferences(MODE_PRIVATE).getBoolean(permission, false)
                && !shouldShowRequestPermissionRationale(permission);
    }

    private void requestAccess(int index) {
        String permission = PERMISSIONS[index];
        if (blocked(permission)) {
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                    Uri.fromParts("package", getPackageName(), null)));
        } else {
            requestPermissions(new String[] { permission }, index + 1);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshPermissionState();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        // An empty result means the prompt was interrupted; it is safe to ask again.
        for (int i = 0; i < Math.min(permissions.length, results.length); i++) {
            getPreferences(MODE_PRIVATE).edit().putBoolean(permissions[i], true).apply();
        }
        refreshPermissionState();
    }

    private void refreshPermissionState() {
        StringBuilder status = new StringBuilder();
        for (int i = 0; i < PERMISSIONS.length; i++) {
            boolean allowed = granted(PERMISSIONS[i]);
            status.append(LABELS[i]).append(allowed ? ": allowed" : ": access needed").append('\n');
            permissionButtons[i].setEnabled(!allowed);
            permissionButtons[i].setText(allowed ? LABELS[i] + " allowed"
                    : blocked(PERMISSIONS[i]) ? "Open settings: " + LABELS[i]
                    : "Allow " + LABELS[i]);
        }
        status.append(granted(Manifest.permission.CALL_PHONE)
                ? "\nDialer ready on your Pebble."
                : "\nAllow Phone to place calls from your Pebble.");
        statusView.setText(status);
    }
}
