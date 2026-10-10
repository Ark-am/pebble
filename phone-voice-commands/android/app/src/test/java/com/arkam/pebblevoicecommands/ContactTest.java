package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import java.util.ArrayList;
import java.util.Arrays;

public class ContactTest {
    private static final Contact.Number HOME = new Contact.Number("111", Contact.KIND_HOME, true);
    private static final Contact.Number MOBILE = new Contact.Number("222", Contact.KIND_MOBILE, false);
    private static final Contact.Number WORK = new Contact.Number("333", Contact.KIND_WORK, false);
    private static final Contact SAM = new Contact("Sam", false, Arrays.asList(HOME, MOBILE, WORK));

    @Test public void callsTheDefaultNumberAndTextsTheMobile() {
        assertSame(HOME, SAM.pick(Contact.KIND_ANY, false));
        assertSame(MOBILE, SAM.pick(Contact.KIND_ANY, true));
    }

    @Test public void usesTheRequestedKind() {
        assertSame(WORK, SAM.pick(Contact.KIND_WORK, false));
        Contact noWork = new Contact("Mom", false, Arrays.asList(HOME));
        assertNull(noWork.pick(Contact.KIND_WORK, false));
    }

    @Test public void fallsBackWithoutADefault() {
        Contact.Number other = new Contact.Number("444", Contact.KIND_OTHER, false);
        assertSame(MOBILE, new Contact("A", false, Arrays.asList(other, MOBILE)).pick(Contact.KIND_ANY, false));
        assertSame(other, new Contact("B", false, Arrays.asList(other)).pick(Contact.KIND_ANY, true));
        assertNull(new Contact("C", false, new ArrayList<>()).pick(Contact.KIND_ANY, false));
    }
}
