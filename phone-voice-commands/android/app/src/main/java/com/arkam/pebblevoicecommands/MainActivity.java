package com.arkam.pebblevoicecommands;

import android.Manifest;
import android.app.Activity;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.Intent;
import android.content.IntentSender;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.regex.Pattern;

public final class MainActivity extends Activity {
    private static final int LINK_REQUEST = 100;
    // Pebble and Core watches advertise names such as "Pebble Time 1A2B".
    private static final Pattern PEBBLE_NAME = Pattern.compile("(?i).*(pebble|core).*");
    private static final int PERMISSION_REQUEST = 101;
    // Each is optional: without one, only the commands that need it are refused.
    private static final String[] PERMISSIONS = {
            Manifest.permission.READ_CONTACTS,
            Manifest.permission.CALL_PHONE,
            Manifest.permission.SEND_SMS,
    };
    private static final int[] PERMISSION_LABELS = {
            R.string.permission_contacts,
            R.string.permission_phone,
            R.string.permission_sms,
    };

    private TextView lastCommandView;
    private Button linkButton;
    private final Button[] permissionButtons = new Button[PERMISSIONS.length];
    private final SharedPreferences.OnSharedPreferenceChangeListener logListener =
            (preferences, key) -> refreshLastCommand();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        ScrollView scroll = new ScrollView(this);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.setPadding(padding, padding, padding, padding);
        scroll.addView(layout);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        layout.addView(title, matchWidth());

        TextView statusView = new TextView(this);
        statusView.setText(R.string.status_ready);
        statusView.setTextSize(18);
        statusView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusParams = matchWidth();
        statusParams.setMargins(0, padding, 0, padding);
        layout.addView(statusView, statusParams);

        layout.addView(heading(R.string.permissions_title), matchWidth());
        TextView permissionHelp = new TextView(this);
        permissionHelp.setText(R.string.permissions_help);
        permissionHelp.setTextSize(14);
        permissionHelp.setPadding(0, 0, 0, padding / 3);
        layout.addView(permissionHelp, matchWidth());
        for (int i = 0; i < PERMISSIONS.length; i++) {
            final int index = i;
            permissionButtons[i] = new Button(this);
            permissionButtons[i].setOnClickListener(view -> requestAccess(index));
            layout.addView(permissionButtons[i], matchWidth());
        }
        View spacer = new View(this);
        layout.addView(spacer, new LinearLayout.LayoutParams(1, padding));

        if (Companion.linkNeeded()) {
            layout.addView(heading(R.string.link_title), matchWidth());

            TextView linkHelp = new TextView(this);
            linkHelp.setText(R.string.link_help);
            linkHelp.setTextSize(14);
            linkHelp.setPadding(0, 0, 0, padding / 3);
            layout.addView(linkHelp, matchWidth());

            linkButton = new Button(this);
            linkButton.setOnClickListener(view -> linkWatch());
            LinearLayout.LayoutParams linkParams = matchWidth();
            linkParams.setMargins(0, 0, 0, padding);
            layout.addView(linkButton, linkParams);
        }

        layout.addView(heading(R.string.examples_title), matchWidth());
        TextView examples = new TextView(this);
        examples.setText(R.string.examples);
        examples.setTextSize(16);
        examples.setLineSpacing(0, 1.2f);
        examples.setPadding(0, 0, 0, padding);
        layout.addView(examples, matchWidth());

        layout.addView(heading(R.string.last_command), matchWidth());
        lastCommandView = new TextView(this);
        lastCommandView.setTextSize(16);
        layout.addView(lastCommandView, matchWidth());

        setContentView(scroll);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Update live while open, so the phone shows each command as it arrives.
        CommandLog.preferences(this).registerOnSharedPreferenceChangeListener(logListener);
        refreshLastCommand();
        refreshLinkState();
        refreshPermissionState();
    }

    @Override
    protected void onPause() {
        CommandLog.preferences(this).unregisterOnSharedPreferenceChangeListener(logListener);
        super.onPause();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == LINK_REQUEST) {
            refreshLinkState();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        // Remember that Android has asked, to tell "not yet asked" from "blocked".
        SharedPreferences.Editor asked = getPreferences(MODE_PRIVATE).edit();
        for (int i = 0; i < Math.min(permissions.length, results.length); i++) {
            asked.putBoolean(permissions[i], true);
        }
        asked.apply();
        refreshPermissionState();
    }

    private void requestAccess(int index) {
        String permission = PERMISSIONS[index];
        if (blocked(permission)) {
            // Android stops showing the prompt after repeated denials.
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                    Uri.fromParts("package", getPackageName(), null)));
            return;
        }
        requestPermissions(new String[] { permission }, PERMISSION_REQUEST);
    }

    private boolean granted(String permission) {
        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    private boolean blocked(String permission) {
        return !granted(permission)
                && getPreferences(MODE_PRIVATE).getBoolean(permission, false)
                && !shouldShowRequestPermissionRationale(permission);
    }

    private void refreshPermissionState() {
        for (int i = 0; i < PERMISSIONS.length; i++) {
            String label = getString(PERMISSION_LABELS[i]);
            boolean allowed = granted(PERMISSIONS[i]);
            permissionButtons[i].setEnabled(!allowed);
            permissionButtons[i].setText(allowed
                    ? getString(R.string.permission_allowed, label)
                    : blocked(PERMISSIONS[i])
                    ? getString(R.string.permission_settings, label)
                    : getString(R.string.permission_allow, label));
        }
    }

    private TextView heading(int text) {
        TextView heading = new TextView(this);
        heading.setText(text);
        heading.setTextSize(14);
        heading.setAllCaps(true);
        heading.setPadding(0, 0, 0, Math.round(6 * getResources().getDisplayMetrics().density));
        return heading;
    }

    private void refreshLastCommand() {
        String transcript = CommandLog.transcript(this);
        if (transcript == null) {
            lastCommandView.setText(R.string.no_command_yet);
            return;
        }
        lastCommandView.setText(getString(
                R.string.last_command_detail, transcript, CommandLog.reply(this)));
    }

    private void refreshLinkState() {
        if (linkButton == null) {
            return;
        }
        boolean linked = Companion.linked(this);
        linkButton.setEnabled(!linked);
        linkButton.setText(linked ? R.string.link_done : R.string.link_button);
    }

    /**
     * Asks Android to link the Pebble to this app. Android shows its own list of
     * Pebble watches. No device profile is requested: the link alone lifts the
     * background activity limit, and the Pebble phone app keeps its own link.
     */
    private void linkWatch() {
        // The button only appears where Companion.linkNeeded(), Android 10 and newer.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return;
        }
        CompanionDeviceManager manager = getSystemService(CompanionDeviceManager.class);
        if (manager == null) {
            return;
        }
        AssociationRequest request = new AssociationRequest.Builder()
                .addDeviceFilter(new BluetoothDeviceFilter.Builder()
                        .setNamePattern(PEBBLE_NAME).build())
                .addDeviceFilter(new BluetoothLeDeviceFilter.Builder()
                        .setNamePattern(PEBBLE_NAME).build())
                .build();
        manager.associate(request, new CompanionDeviceManager.Callback() {
            // Android 13 and newer.
            @Override
            public void onAssociationPending(IntentSender chooser) {
                showChooser(chooser);
            }

            // Android 12 and older.
            @Override
            @SuppressWarnings("deprecation")
            public void onDeviceFound(IntentSender chooser) {
                showChooser(chooser);
            }

            @Override
            public void onFailure(CharSequence error) {
                refreshLinkState();
            }
        }, null);
    }

    private void showChooser(IntentSender chooser) {
        try {
            startIntentSenderForResult(chooser, LINK_REQUEST, null, 0, 0, 0);
        }
        catch (IntentSender.SendIntentException error) {
            refreshLinkState();
        }
    }

    private static LinearLayout.LayoutParams matchWidth() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
