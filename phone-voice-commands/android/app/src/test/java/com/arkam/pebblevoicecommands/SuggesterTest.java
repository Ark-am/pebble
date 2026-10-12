package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

public class SuggesterTest {
    private static final int NOW = 10 * 60;

    private static String suggest(String text) {
        return Suggester.suggest(text, NOW);
    }

    @Test public void fixesMisheardCommandWords() {
        assertEquals("Timer for 5 minutes", suggest("Time for 5 minutes"));
        assertEquals("set a timer for 5 minutes", suggest("set a time for 5 minutes"));
        assertEquals("next song", suggest("next sung"));
        assertEquals("play music", suggest("plays music"));
        assertEquals("Call Mom", suggest("Coll Mom"));
        assertEquals("Launch Spotify", suggest("Lunch Spotify"));
        assertEquals("flashlight on", suggest("fleshlight on"));
    }

    @Test public void knowsCommonSoundAlikes() {
        assertEquals("Pause the music", suggest("Paws the music"));
        assertEquals("Volume up", suggest("Valium up"));
        assertEquals("turn on the flash light", suggest("turn on the flesh light"));
    }

    @Test public void joinsSplitWords() {
        assertEquals("unmute", suggest("un mute"));
    }

    @Test public void makesTwoSwapsWhenOneIsNotEnough() {
        assertEquals("volume down", suggest("valium dawn"));
    }

    @Test public void keepsTheRestAsDictated() {
        assertEquals("Text Sam I'm running late, sorry!", suggest("Texts Sam I'm running late, sorry!"));
        assertEquals("Text Mom, see you at dinner.", suggest("Tax Mom, see you at dinner."));
    }

    @Test public void staysQuietWhenNothingIsClose() {
        for (String phrase : new String[] { null, "", "Open Spotify", "what's the weather",
                "tell me a joke", "good morning", "how are you" }) {
            assertNull(String.valueOf(phrase), suggest(phrase));
        }
    }

    @Test public void editDistance() {
        assertEquals(0, Suggester.distance("timer", "timer"));
        assertEquals(1, Suggester.distance("time", "timer"));
        assertEquals(1, Suggester.distance("paly", "play"));
        assertEquals(2, Suggester.distance("paul", "call"));
    }
}
