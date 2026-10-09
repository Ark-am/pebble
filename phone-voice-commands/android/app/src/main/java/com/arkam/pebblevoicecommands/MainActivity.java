package com.arkam.pebblevoicecommands;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private TextView lastCommandView;
    private final SharedPreferences.OnSharedPreferenceChangeListener logListener =
            (preferences, key) -> refreshLastCommand();

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
        layout.addView(title, matchWidth());

        TextView statusView = new TextView(this);
        statusView.setText(R.string.status_ready);
        statusView.setTextSize(18);
        statusView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusParams = matchWidth();
        statusParams.setMargins(0, padding, 0, padding);
        layout.addView(statusView, statusParams);

        TextView heading = new TextView(this);
        heading.setText(R.string.last_command);
        heading.setTextSize(14);
        heading.setAllCaps(true);
        heading.setGravity(Gravity.CENTER);
        layout.addView(heading, matchWidth());

        lastCommandView = new TextView(this);
        lastCommandView.setTextSize(16);
        lastCommandView.setGravity(Gravity.CENTER);
        layout.addView(lastCommandView, matchWidth());

        setContentView(layout);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Update live while open, so the phone shows each command as it arrives.
        CommandLog.preferences(this).registerOnSharedPreferenceChangeListener(logListener);
        refreshLastCommand();
    }

    @Override
    protected void onPause() {
        CommandLog.preferences(this).unregisterOnSharedPreferenceChangeListener(logListener);
        super.onPause();
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

    private static LinearLayout.LayoutParams matchWidth() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
