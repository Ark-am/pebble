package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import java.util.Arrays;
import java.util.List;

public class AppMatcherTest {
    private static final List<String> APPS = Arrays.asList(
            "Maps", "Google Pay", "YouTube", "YouTube Music", "Spotify: Music and Podcasts",
            "OneNote", "Camera", "Calculator", "Calendar", "Clock", "Files by Google");

    private static String match(String spoken) {
        int index = AppMatcher.best(spoken, APPS);
        return index < 0 ? null : APPS.get(index);
    }

    @Test public void exactNamesWin() {
        assertEquals("Maps", match("maps"));
        assertEquals("YouTube", match("YouTube"));
        assertEquals("YouTube", match("you tube"));
        assertEquals("OneNote", match("one note"));
        assertEquals("Camera", match("camera"));
    }

    @Test public void partialNames() {
        assertEquals("YouTube Music", match("youtube music"));
        assertEquals("Spotify: Music and Podcasts", match("spotify"));
        assertEquals("Files by Google", match("files"));
        assertEquals("Maps", match("google maps"));
        assertEquals("Calculator", match("calc"));
    }

    @Test public void rejectsUnknownAndTooShortNames() {
        assertNull(match("whatsapp"));
        assertNull(match("x"));
        assertNull(match(""));
    }
}
