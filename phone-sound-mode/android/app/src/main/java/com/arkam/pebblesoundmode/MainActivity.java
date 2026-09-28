package com.arkam.pebblesoundmode;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private TextView statusView;
    private Button permissionButton;

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
        permissionButton.setText(R.string.grant_access);
        permissionButton.setOnClickListener(view -> startActivity(
                new Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS)));
        layout.addView(permissionButton);

        setContentView(layout);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshPermissionState();
    }

    private void refreshPermissionState() {
        NotificationManager manager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        boolean granted = manager.isNotificationPolicyAccessGranted();
        statusView.setText(granted
                ? R.string.status_ready
                : R.string.status_permission_required);
        permissionButton.setEnabled(!granted);
        permissionButton.setText(granted
                ? R.string.permission_granted
                : R.string.grant_access);
    }
}
