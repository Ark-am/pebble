package com.arkam.pebblevoicecommands;

import android.app.Activity;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.Intent;
import android.content.IntentSender;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
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

    private TextView lastCommandView;
    private Button linkButton;
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
