package com.arkam.pebblevoicecommands;

import org.junit.Test;
import static org.junit.Assert.*;

import com.arkam.pebblevoicecommands.ParsedCommand.Action;

public class CommandParserTest {
    // 10:00 in the morning.
    private static final int NOW = 10 * 60;

    private static ParsedCommand parse(String text) {
        return CommandParser.parse(text, NOW);
    }

    private static void assertAction(Action action, String... phrases) {
        for (String phrase : phrases) {
            ParsedCommand command = parse(phrase);
            assertNotNull(phrase, command);
            assertEquals(phrase, action, command.action);
        }
    }

    private static void assertValue(Action action, int value, String... phrases) {
        for (String phrase : phrases) {
            ParsedCommand command = parse(phrase);
            assertNotNull(phrase, command);
            assertEquals(phrase, action, command.action);
            assertEquals(phrase, value, command.value);
        }
    }

    @Test public void ignoresEmptyAndUnknownCommands() {
        for (String phrase : new String[] { null, "", "   ", "...", "what's the weather",
                "tell me a joke", "cancel my alarm", "stop the timer" }) {
            assertNull(String.valueOf(phrase), parse(phrase));
        }
    }

    @Test public void stripsPolitenessAndPunctuation() {
        assertAction(Action.FLASHLIGHT_ON, "Hey, can you turn on the flashlight please?",
                "OK, flashlight on.", "Could you please turn the torch on for me");
    }

    @Test public void flashlight() {
        assertAction(Action.FLASHLIGHT_ON, "flashlight on", "turn on the torch", "enable flashlight");
        assertAction(Action.FLASHLIGHT_OFF, "flashlight off", "Turn off the flashlight",
                "switch the torch off");
        assertAction(Action.FLASHLIGHT_TOGGLE, "flashlight", "toggle the flash light");
    }

    @Test public void timerLengths() {
        assertValue(Action.TIMER, 300, "Set a timer for 5 minutes", "timer five minutes",
                "5 minute timer", "start a timer for 5", "set a timer for five mins");
        assertValue(Action.TIMER, 3600, "set a timer for an hour", "one hour timer");
        assertValue(Action.TIMER, 1800, "timer for half an hour", "set a half hour timer");
        assertValue(Action.TIMER, 5400, "set a timer for an hour and a half",
                "timer 1 and a half hours");
        assertValue(Action.TIMER, 630, "timer for 10 minutes and 30 seconds");
        assertValue(Action.TIMER, 1500, "timer for twenty five minutes");
        assertValue(Action.TIMER, 45, "45 second timer");
        assertValue(Action.TIMER, 0, "set a timer");
    }

    @Test public void alarmTimes() {
        assertValue(Action.ALARM, 7 * 60 + 30, "Set an alarm for 7:30 a.m.",
                "alarm for 7:30am", "wake me up at 730 am", "alarm 7 30 am",
                "alarm for 7:30 in the morning", "wake me up at seven thirty am");
        assertValue(Action.ALARM, 18 * 60, "set an alarm for 6 pm", "alarm at 6 PM",
                "wake me at 6 oclock tonight", "alarm for 18:00");
        assertValue(Action.ALARM, 12 * 60, "alarm at noon");
        assertValue(Action.ALARM, 0, "alarm at midnight");
        assertValue(Action.ALARM, 11 * 60 + 15, "alarm at quarter past 11");
        assertValue(Action.ALARM, 19 * 60 + 30, "alarm at half past 7");
        assertValue(Action.ALARM, -1, "set an alarm");
    }

    @Test public void alarmWithoutAmOrPmPicksTheNextOne() {
        // At 10:00, "11" is later this morning but "9" has passed, so 9 pm.
        assertValue(Action.ALARM, 11 * 60, "alarm for 11");
        assertValue(Action.ALARM, 21 * 60, "alarm for 9");
        assertValue(Action.ALARM, 12 * 60, "alarm for 12");
        // Late at night, the morning is next.
        assertEquals(6 * 60, CommandParser.parse("alarm for 6", 23 * 60).value);
    }

    @Test public void relativeAlarms() {
        assertValue(Action.ALARM, NOW + 20, "wake me up in 20 minutes",
                "set an alarm for 20 minutes");
        assertValue(Action.ALARM, NOW + 90, "set an alarm in an hour and a half");
        assertEquals(30, CommandParser.parse("wake me in 1 hour", 23 * 60 + 30).value);
    }

    @Test public void volume() {
        assertAction(Action.VOLUME_UP, "volume up", "turn it up", "turn up the volume",
                "louder", "increase volume", "turn the music up");
        assertAction(Action.VOLUME_DOWN, "volume down", "turn it down", "quieter",
                "lower the volume", "turn the volume down");
        assertAction(Action.VOLUME_MUTE, "mute", "mute the music");
        assertAction(Action.VOLUME_UNMUTE, "unmute", "unmute the sound");
        assertValue(Action.VOLUME_SET, 50, "set volume to 50%", "volume 50 percent",
                "set the volume to fifty percent", "volume to 5", "volume 50");
        assertValue(Action.VOLUME_SET, 7, "volume 7 percent");
        assertValue(Action.VOLUME_SET, 100, "max volume", "volume to max",
                "volume one hundred percent", "volume 150");
        assertValue(Action.VOLUME_SET, 0, "volume 0");
    }

    @Test public void media() {
        assertAction(Action.MEDIA_PLAY, "play", "resume", "play music", "resume playback",
                "continue playing", "unpause");
        assertAction(Action.MEDIA_PAUSE, "pause", "stop", "pause the music", "stop the music");
        assertAction(Action.MEDIA_NEXT, "next", "next song", "skip", "skip this song",
                "play the next track");
        assertAction(Action.MEDIA_PREVIOUS, "previous", "previous song", "last track",
                "go back", "go back a song", "play the previous song");
    }

    @Test public void openApp() {
        assertEquals("spotify", parse("Open Spotify").text);
        assertEquals("google maps", parse("launch the Google Maps app").text);
        assertEquals("one note", parse("open one note").text);
        assertEquals("camera", parse("please start my camera").text);
        assertAction(Action.OPEN_APP, "run calculator", "show calendar");
    }

    @Test public void callContacts() {
        ParsedCommand call = parse("Call Mom.");
        assertEquals(Action.CALL_CONTACT, call.action);
        assertEquals("mom", call.text);
        assertEquals(Contact.KIND_ANY, call.value);

        assertEquals("sam smith", parse("please call Sam Smith").text);
        assertEquals("one direction", parse("call one direction").text);
        assertEquals("home", parse("call home").text);

        ParsedCommand mobile = parse("call Sam on his mobile");
        assertEquals("sam", mobile.text);
        assertEquals(Contact.KIND_MOBILE, mobile.value);
        assertEquals(Contact.KIND_MOBILE, parse("call Sam cell phone").value);
        assertEquals(Contact.KIND_WORK, parse("ring Dad at the office").value);
        ParsedCommand home = parse("call mom's home number");
        assertEquals("moms", home.text);
        assertEquals(Contact.KIND_HOME, home.value);
    }

    @Test public void callNumbers() {
        assertValueText(Action.CALL_NUMBER, "5551234", "call 555 1234", "dial 555-1234",
                "call five five five one two three four");
        assertValueText(Action.CALL_NUMBER, "+15551234567", "Call +1 555 123 4567");
        assertNull(parse("call 12"));
    }

    @Test public void textMessagesKeepTheirWording() {
        assertValueText(Action.SEND_TEXT, "Sam I'm running late, sorry!",
                "Text Sam I'm running late, sorry!",
                "Hey, can you text Sam I'm running late, sorry!",
                "send a text to Sam I'm running late, sorry!",
                "Send a text message to Sam I'm running late, sorry!",
                "message Sam I'm running late, sorry!");
        assertValueText(Action.SEND_TEXT, "Dad the flashlight is in the drawer",
                "tell Dad the flashlight is in the drawer");
        assertValueText(Action.SEND_TEXT, "Mom set a timer for 5 minutes",
                "text Mom set a timer for 5 minutes");
    }

    @Test public void navigation() {
        assertValueText(Action.NAVIGATE, "the airport", "Navigate to the airport",
                "directions to the airport", "get directions to the airport",
                "take me to the airport", "how do I get to the airport", "drive to the airport");
        assertValueText(Action.NAVIGATE, "home", "navigate home", "take me home");
        assertValueText(Action.NAVIGATE, "221 baker street london",
                "Navigate to 221 Baker Street, London.");
        assertEquals(1, parse("walk to the park").value);
        assertEquals(1, parse("walking directions to the park").value);
        assertEquals(0, parse("drive to the park").value);
    }

    private static void assertValueText(Action action, String text, String... phrases) {
        for (String phrase : phrases) {
            ParsedCommand command = parse(phrase);
            assertNotNull(phrase, command);
            assertEquals(phrase, action, command.action);
            assertEquals(phrase, text, command.text);
        }
    }

    /** Every example in STORE_DESCRIPTION.txt must work as written. */
    @Test public void storeDescriptionExamples() {
        assertAction(Action.CALL_CONTACT, "Call Mom", "Call Sam on his mobile", "Ring Dad at work");
        assertAction(Action.CALL_NUMBER, "Call 555 1234", "Dial +1 555 123 4567");
        assertAction(Action.SEND_TEXT, "Text Sam I'm running late",
                "Send a message to Mom saying happy birthday");
        assertAction(Action.NAVIGATE, "Navigate to the airport", "Take me home",
                "Directions to 221 Baker Street", "Walk to the park");
        assertAction(Action.TIMER, "Set a timer for 5 minutes", "Timer for an hour and a half");
        assertAction(Action.ALARM, "Set an alarm for 7:30 am", "Wake me up at 6", "Alarm at noon",
                "Wake me up in 20 minutes");
        assertAction(Action.MEDIA_PLAY, "Play");
        assertAction(Action.MEDIA_PAUSE, "Pause");
        assertAction(Action.MEDIA_NEXT, "Next song");
        assertAction(Action.MEDIA_PREVIOUS, "Previous song");
        assertAction(Action.VOLUME_UP, "Volume up");
        assertAction(Action.VOLUME_DOWN, "Volume down");
        assertAction(Action.VOLUME_SET, "Volume 50 percent", "Max volume");
        assertAction(Action.VOLUME_MUTE, "Mute");
        assertAction(Action.VOLUME_UNMUTE, "Unmute");
        assertAction(Action.FLASHLIGHT_ON, "Flashlight on",
                "Hey, can you turn on the flashlight please?");
        assertAction(Action.FLASHLIGHT_OFF, "Turn off the torch");
        assertAction(Action.OPEN_APP, "Open Spotify", "Launch the camera");
        assertAction(Action.HELP, "Help");
    }

    @Test public void help() {
        assertAction(Action.HELP, "help", "what can you do", "What can I say?");
    }

    @Test public void spokenNumbers() {
        assertEquals("25 minutes", CommandParser.numbersToDigits("twenty five minutes"));
        assertEquals("100 percent", CommandParser.numbersToDigits("one hundred percent"));
        assertEquals("7 30 am", CommandParser.numbersToDigits("seven thirty am"));
        assertEquals("open notes", CommandParser.numbersToDigits("open notes"));
    }
}
