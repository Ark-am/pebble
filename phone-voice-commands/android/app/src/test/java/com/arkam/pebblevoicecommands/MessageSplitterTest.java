package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class MessageSplitterTest {
    private static final List<Contact> CONTACTS = Arrays.asList(
            new Contact("Sam Smith", false, new ArrayList<>()),
            new Contact("Mom", false, new ArrayList<>()));

    private static MessageSplitter.Split split(String text) {
        return MessageSplitter.split(text, name -> ContactMatcher.matchesAny(name, CONTACTS));
    }

    private static void assertSplit(String recipient, String body, String text) {
        MessageSplitter.Split split = split(text);
        assertEquals(text, recipient, split.recipient);
        assertEquals(text, body, split.body);
    }

    @Test public void findsTheNameWithoutPunctuation() {
        assertSplit("Sam Smith", "I'm running late", "Sam Smith I'm running late");
        assertSplit("Sam", "I'm running late", "Sam I'm running late");
        assertSplit("Mom", "I love you", "Mom I love you");
        assertSplit("Mom", "Set a timer for 5 minutes", "Mom set a timer for 5 minutes");
    }

    @Test public void usesCommasAndSaying() {
        assertSplit("Sam Smith", "On my way, see you soon.", "Sam Smith, on my way, see you soon.");
        assertSplit("Mom", "Happy birthday", "Mom saying happy birthday");
        assertSplit("Mom", "Call me", "Mom that says \"call me\"");
        // An unknown name still splits at an explicit marker.
        assertSplit("Jo Bloggs", "Hello", "Jo Bloggs: hello");
    }

    @Test public void unknownNameFallsBackToTheFirstWord() {
        assertSplit("Bob", "See you at 6", "Bob see you at 6");
    }

    @Test public void phoneNumbers() {
        MessageSplitter.Split split = split("+1 555 123 4567 on my way");
        assertTrue(split.isNumber);
        assertEquals("+15551234567", split.recipient);
        assertEquals("On my way", split.body);
        assertEquals("5551234", split("555-1234, hi").recipient);
    }

    @Test public void emptyMessage() {
        assertSplit("Sam Smith", "", "Sam Smith");
        assertSplit("Mom", "", "Mom.");
    }
}
