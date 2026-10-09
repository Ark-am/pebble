package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import java.nio.charset.StandardCharsets;

public class WatchTextTest {
    @Test public void keepsShortTextAndTrimsSpaces() {
        assertEquals("Heard: lights on", WatchText.fit("  Heard: lights on ", 127));
        assertEquals("", WatchText.fit(null, 127));
    }

    @Test public void replacesControlCharacters() {
        assertEquals("call mom", WatchText.fit("call\nmom", 127));
    }

    @Test public void cutsAtWholeUtf8Characters() {
        // "é" is two bytes, so only two of them fit in five bytes.
        assertEquals("éé", WatchText.fit("ééé", 5));
        // A four-byte emoji never gets split.
        assertEquals("ok", WatchText.fit("ok😀", 5));
    }

    @Test public void neverExceedsTheReplyBuffer() {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < 100; i++) {
            text.append("ü");
        }
        String fitted = WatchText.fit(text.toString(), WatchText.MAX_REPLY_BYTES);
        assertTrue(fitted.getBytes(StandardCharsets.UTF_8).length <= WatchText.MAX_REPLY_BYTES);
    }
}
