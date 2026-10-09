package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

public class FormatsTest {
    @Test public void durations() {
        assertEquals("5 min", Formats.duration(300));
        assertEquals("1 h 30 min", Formats.duration(5400));
        assertEquals("10 min 30 s", Formats.duration(630));
        assertEquals("45 s", Formats.duration(45));
        assertEquals("2 h", Formats.duration(7200));
    }

    @Test public void clockTimes() {
        assertEquals("7:05 AM", Formats.clock(7 * 60 + 5, false));
        assertEquals("12:00 PM", Formats.clock(12 * 60, false));
        assertEquals("12:30 AM", Formats.clock(30, false));
        assertEquals("6:00 PM", Formats.clock(18 * 60, false));
        assertEquals("18:00", Formats.clock(18 * 60, true));
        assertEquals("07:05", Formats.clock(7 * 60 + 5, true));
    }
}
