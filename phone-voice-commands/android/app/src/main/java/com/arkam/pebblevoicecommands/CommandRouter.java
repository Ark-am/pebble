package com.arkam.pebblevoicecommands;

import android.content.Context;

/** Works out what a dictated command asks for and carries it out. */
final class CommandRouter {
    private CommandRouter() {
    }

    /** Runs on a background thread, so actions may block briefly. */
    static CommandResult handle(Context context, String transcript) {
        String command = transcript == null ? "" : transcript.trim();
        if (command.isEmpty()) {
            return CommandResult.notUnderstood("I didn't catch that");
        }

        // No actions yet: echo the words so the round trip can be checked.
        return CommandResult.ok("Heard: " + command);
    }
}
