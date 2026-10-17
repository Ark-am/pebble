package com.arkam.pebbledialer;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

public final class MainActivity extends Activity {
    // Each button asks for one group. Phone covers placing calls and, from
    // Android 9, ending them from the watch; both are in the same Android group,
    // so one prompt grants both.
    private static final String[][] PERMISSIONS = {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    ? new String[] { Manifest.permission.CALL_PHONE,
                                     Manifest.permission.ANSWER_PHONE_CALLS }
                    : new String[] { Manifest.permission.CALL_PHONE },
            { Manifest.permission.READ_CONTACTS },
            { Manifest.permission.READ_CALL_LOG },
    };
    private static final String[] LABELS = { "Phone", "Contacts", "Call history" };
    private final Button[] permissionButtons = new Button[PERMISSIONS.length];
    private TextView statusView;
    private Button linkButton;
    private TextView linkStatus;

    private static final int LINK_REQUEST = 100;
    private static final int NEARBY_REQUEST = 101;
    // The Bluetooth names Pebble watches use, to list them first.
    private static final Pattern PEBBLE_NAME = Pattern.compile("(?i).*(pebble|core).*");

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
        // Switching a call's audio needs the watch companion profile, which
        // Android 12 added; it is what grants the call-management permission.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            TextView linkTitle = new TextView(this);
            linkTitle.setText(R.string.link_title);
            linkTitle.setTextSize(20);
            linkTitle.setPadding(0, padding, 0, padding / 3);
            layout.addView(linkTitle);

            TextView linkHelp = new TextView(this);
            linkHelp.setText(R.string.link_help);
            linkHelp.setTextSize(14);
            linkHelp.setPadding(0, 0, 0, padding / 3);
            layout.addView(linkHelp);

            linkButton = new Button(this);
            linkButton.setOnClickListener(view -> linkWatch());
            layout.addView(linkButton, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

            linkStatus = new TextView(this);
            linkStatus.setTextSize(14);
            layout.addView(linkStatus);
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

    private boolean granted(String[] group) {
        for (String permission : group) {
            if (!granted(permission)) {
                return false;
            }
        }
        return true;
    }

    private boolean blocked(String permission) {
        return !granted(permission)
                && getPreferences(MODE_PRIVATE).getBoolean(permission, false)
                && !shouldShowRequestPermissionRationale(permission);
    }

    private boolean blocked(String[] group) {
        for (String permission : group) {
            if (blocked(permission)) {
                return true;
            }
        }
        return false;
    }

    private void requestAccess(int index) {
        String[] group = PERMISSIONS[index];
        if (blocked(group)) {
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                    Uri.fromParts("package", getPackageName(), null)));
        } else {
            requestPermissions(group, index + 1);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshPermissionState();
        refreshLinkState();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == LINK_REQUEST) {
            refreshLinkState();
        }
    }

    /** Whether the Pebble is linked to this app as a companion device. */
    @SuppressWarnings("deprecation")
    private boolean linked() {
        CompanionDeviceManager manager = getSystemService(CompanionDeviceManager.class);
        if (manager == null) {
            return false;
        }
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                ? !manager.getMyAssociations().isEmpty()
                : !manager.getAssociations().isEmpty();
    }

    private void refreshLinkState() {
        if (linkButton == null) {
            return;
        }
        boolean linked = linked();
        linkButton.setEnabled(!linked);
        linkButton.setText(linked ? R.string.link_done : R.string.link_button);
        if (linked) {
            linkStatus.setText("");
        }
    }

    /**
     * Links the Pebble to this app with Android's watch profile, which grants
     * MANAGE_ONGOING_CALLS; Android then binds CallControlService to calls,
     * which lets the watch switch their audio. The watch role is not exclusive,
     * so the Pebble phone app keeps its own.
     *
     * Android's companion list only includes a device that is already paired
     * and connected when it is asked for by its exact Bluetooth address; a
     * Pebble connected to the Pebble app does not advertise, so a search by
     * name finds nothing. So the paired devices are listed here (which needs
     * the Nearby devices permission), and the chosen one is linked by address.
     */
    @SuppressLint("MissingPermission")
    private void linkWatch() {
        if (!granted(Manifest.permission.BLUETOOTH_CONNECT)) {
            if (blocked(Manifest.permission.BLUETOOTH_CONNECT)) {
                startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        Uri.fromParts("package", getPackageName(), null)));
            } else {
                requestPermissions(new String[] { Manifest.permission.BLUETOOTH_CONNECT },
                        NEARBY_REQUEST);
            }
            return;
        }
        BluetoothManager bluetooth = getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = bluetooth == null ? null : bluetooth.getAdapter();
        if (adapter == null || !adapter.isEnabled()) {
            linkStatus.setText(R.string.link_bluetooth_off);
            return;
        }

        // Pebble watches first; any other paired device can still be chosen,
        // in case the watch has been renamed.
        List<BluetoothDevice> devices = new ArrayList<>();
        List<BluetoothDevice> pebbles = new ArrayList<>();
        for (BluetoothDevice device : adapter.getBondedDevices()) {
            if (isPebble(device)) {
                pebbles.add(device);
            }
        }
        devices.addAll(pebbles);
        for (BluetoothDevice device : adapter.getBondedDevices()) {
            if (!isPebble(device)) {
                devices.add(device);
            }
        }
        if (devices.isEmpty()) {
            linkStatus.setText(R.string.link_no_devices);
            return;
        }
        if (pebbles.size() == 1) {
            associate(pebbles.get(0));
            return;
        }

        String[] names = new String[devices.size()];
        for (int i = 0; i < devices.size(); i++) {
            String name = devices.get(i).getName();
            names[i] = name == null || name.isEmpty() ? devices.get(i).getAddress() : name;
        }
        new AlertDialog.Builder(this)
                .setTitle(R.string.link_choose)
                .setItems(names, (dialog, which) -> associate(devices.get(which)))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    @SuppressLint("MissingPermission")
    private static boolean isPebble(BluetoothDevice device) {
        String name = device.getName();
        return name != null && PEBBLE_NAME.matcher(name).matches();
    }

    /** Asks Android to link this exact device; it then only asks to confirm. */
    private void associate(BluetoothDevice device) {
        CompanionDeviceManager manager = getSystemService(CompanionDeviceManager.class);
        if (manager == null) {
            return;
        }
        AssociationRequest request = new AssociationRequest.Builder()
                .addDeviceFilter(new BluetoothDeviceFilter.Builder()
                        .setAddress(device.getAddress()).build())
                .setSingleDevice(true)
                .setDeviceProfile(AssociationRequest.DEVICE_PROFILE_WATCH)
                .build();
        linkStatus.setText("");
        manager.associate(request, new CompanionDeviceManager.Callback() {
            // Android 13 and newer.
            @Override
            public void onAssociationPending(IntentSender chooser) {
                showChooser(chooser);
            }

            // Android 12.
            @Override
            @SuppressWarnings("deprecation")
            public void onDeviceFound(IntentSender chooser) {
                showChooser(chooser);
            }

            @Override
            public void onFailure(CharSequence error) {
                linkStatus.setText(getString(R.string.link_failed, error));
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

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == NEARBY_REQUEST) {
            getPreferences(MODE_PRIVATE).edit()
                    .putBoolean(Manifest.permission.BLUETOOTH_CONNECT, true).apply();
            if (granted(Manifest.permission.BLUETOOTH_CONNECT)) {
                linkWatch();
            } else {
                linkStatus.setText(R.string.link_nearby_needed);
            }
            return;
        }
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
                : "\nAllow Phone to place and end calls from your Pebble.");
        statusView.setText(status);
    }
}
