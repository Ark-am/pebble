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
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final String[] PERMISSIONS = {
            Manifest.permission.READ_CONTACTS,
            Manifest.permission.CALL_PHONE,
    };
    private static final int PERMISSION_REQUEST = 1;

    private TextView statusView;
    private Button permissionButton;
    // Set once Android stops showing the permission prompt for this app.
    private boolean blocked;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        layout.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        statusView = new TextView(this);
        statusView.setTextSize(18);
        statusView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        statusParams.setMargins(0, padding, 0, padding);
        layout.addView(statusView, statusParams);

        permissionButton = new Button(this);
        permissionButton.setOnClickListener(view -> {
            if (blocked) {
                startActivity(new Intent(
                        Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        Uri.fromParts("package", getPackageName(), null)));
            } else {
                requestPermissions(PERMISSIONS, PERMISSION_REQUEST);
            }
        });
        layout.addView(permissionButton);

        setContentView(layout);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshPermissionState();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != PERMISSION_REQUEST) {
            return;
        }
        // A denial without a rationale means "don't ask again", or a dismissed
        // dialog on some versions; either way settings is the only way forward.
        for (int i = 0; i < permissions.length; i++) {
            if (grantResults[i] != PackageManager.PERMISSION_GRANTED
                    && !shouldShowRequestPermissionRationale(permissions[i])) {
                blocked = true;
            }
        }
        refreshPermissionState();
    }

    private boolean allGranted() {
        for (String permission : PERMISSIONS) {
            if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    private void refreshPermissionState() {
        boolean granted = allGranted();
        if (granted) {
            blocked = false;
        }
        statusView.setText(granted
                ? R.string.status_ready
                : blocked
                        ? R.string.status_permission_blocked
                        : R.string.status_permission_required);
        permissionButton.setEnabled(!granted);
        permissionButton.setText(granted
                ? R.string.permission_granted
                : blocked ? R.string.open_settings : R.string.grant_access);
    }
}
