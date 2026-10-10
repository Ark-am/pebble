package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class ContactMatcherTest {
    private static Contact contact(String name, boolean starred) {
        return new Contact(name, starred, new ArrayList<>());
    }

    private static final List<Contact> CONTACTS = Arrays.asList(
            contact("Mom", false), contact("Sam Smith", false), contact("Sam Lee", false),
            contact("Alice Jones", false), contact("Dad", true), contact("Dr. O'Brien", false));

    private static String match(String spoken) {
        ContactMatcher.Match match = ContactMatcher.best(spoken, CONTACTS);
        return match.index < 0 ? null : CONTACTS.get(match.index).name;
    }

    @Test public void wholeNamesAndWordsMatch() {
        assertEquals("Mom", match("mom"));
        assertEquals("Sam Smith", match("sam smith"));
        assertEquals("Sam Smith", match("smith"));
        assertEquals("Alice Jones", match("alice"));
        assertEquals("Dr. O'Brien", match("dr obrien"));
        assertEquals("Mom", match("moms"));
    }

    @Test public void neverGuessesFromPartOfAWord() {
        assertNull(match("al"));
        assertNull(match("sa"));
        assertNull(match("bob"));
        assertNull(match(""));
    }

    @Test public void reportsTies() {
        ContactMatcher.Match match = ContactMatcher.best("sam", CONTACTS);
        assertEquals(-1, match.index);
        assertEquals(Arrays.asList(1, 2), match.ties);
        assertTrue(ContactMatcher.matchesAny("sam", CONTACTS));
    }

    @Test public void favouritesAndDuplicatesSettleTies() {
        List<Contact> starred = Arrays.asList(contact("Sam Smith", true), contact("Sam Lee", false));
        assertEquals(0, ContactMatcher.best("sam", starred).index);

        List<Contact> duplicates = Arrays.asList(contact("Sam Smith", false), contact("Sam Smith", false));
        assertEquals(0, ContactMatcher.best("sam", duplicates).index);

        // The whole name beats a first-name match.
        List<Contact> exact = Arrays.asList(contact("Sam Smith", false), contact("Sam", false));
        assertEquals(1, ContactMatcher.best("sam", exact).index);
    }
}
