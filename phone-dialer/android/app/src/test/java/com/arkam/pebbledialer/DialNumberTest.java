package com.arkam.pebbledialer;

import org.junit.Test;
import static org.junit.Assert.*;

public class DialNumberTest {
    @Test public void acceptsLocalInternationalAndServiceNumbers() {
        for (String number : new String[] { "01712345678", "+8801712345678", "*123#", "1234567890123456789012345678901" }) {
            assertTrue(number, DialNumber.isValid(number));
        }
    }

    @Test public void rejectsEmptyMalformedAndOversizedRequests() {
        for (String number : new String[] { null, "", "+", "*#", "12+34", "++123", "tel:123",
                "123\n", "1;2", "123 abc", "١٢٣", "12345678901234567890123456789012" }) {
            assertFalse(number, DialNumber.isValid(number));
        }
    }
}
