package com.arkam.pebblevoicecommands;

/** The outcome of a command and the short reply the watch shows for it. */
final class CommandResult {
    // Must match the RESULT_* values in the watch app's main.c.
    static final int OK = 0;
    static final int FAILED = 1;
    static final int NOT_UNDERSTOOD = 2;
    static final int PERMISSION_REQUIRED = 3;

    final int code;
    final String reply;

    private CommandResult(int code, String reply) {
        this.code = code;
        this.reply = reply;
    }

    static CommandResult ok(String reply) {
        return new CommandResult(OK, reply);
    }

    static CommandResult failed(String reply) {
        return new CommandResult(FAILED, reply);
    }

    static CommandResult notUnderstood(String reply) {
        return new CommandResult(NOT_UNDERSTOOD, reply);
    }

    static CommandResult permissionRequired(String reply) {
        return new CommandResult(PERMISSION_REQUIRED, reply);
    }
}
