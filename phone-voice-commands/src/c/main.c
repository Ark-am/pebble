#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_TRANSCRIPT and friends.

enum {
  RESULT_OK = 0,
  RESULT_FAILED = 1,
  RESULT_NOT_UNDERSTOOD = 2,
  RESULT_PERMISSION_REQUIRED = 3,
};

static Window *s_window;
static Layer *s_icon_layer;
static TextLayer *s_transcript_layer;
static TextLayer *s_status_layer;
static DictationSession *s_dictation;
static AppTimer *s_response_timer;
static AppTimer *s_start_timer;
static bool s_waiting;

// Dictation results longer than this are truncated by the system.
#define TRANSCRIPT_SIZE 256
#define REPLY_SIZE 128
#define RESPONSE_TIMEOUT_MS 10000
// Give the window a moment to appear before dictation covers it.
#define START_DELAY_MS 100

#define ICON_WIDTH 28
#define ICON_HEIGHT 32
// The status bar is at least two lines tall and grows to fit longer replies.
#define STATUS_HEIGHT PBL_IF_ROUND_ELSE(52, 44)
#define STATUS_PADDING 8

static char s_transcript[TRANSCRIPT_SIZE + 2];
static char s_reply[REPLY_SIZE];
static int16_t s_text_top;

static void set_status(const char *text) {
  text_layer_set_text(s_status_layer, text);

  Layer *root = window_get_root_layer(s_window);
  const GRect bounds = layer_get_bounds(root);
  // Round screens narrow towards the bottom, so measure against a narrower box.
  const int16_t width = bounds.size.w - PBL_IF_ROUND_ELSE(bounds.size.w / 4, 8);
  const int16_t max_height = bounds.size.h - s_text_top - 24;
  const GSize size = graphics_text_layout_get_content_size(
    text, fonts_get_system_font(FONT_KEY_GOTHIC_18),
    GRect(0, 0, width, max_height),
    GTextOverflowModeWordWrap, GTextAlignmentCenter);
  const int16_t height = size.h + STATUS_PADDING > max_height
    ? max_height
    : (size.h + STATUS_PADDING < STATUS_HEIGHT ? STATUS_HEIGHT : size.h + STATUS_PADDING);

  layer_set_frame(text_layer_get_layer(s_status_layer),
                  GRect(0, bounds.size.h - height, bounds.size.w, height));
  GRect transcript = layer_get_frame(text_layer_get_layer(s_transcript_layer));
  transcript.size.h = bounds.size.h - height - s_text_top;
  layer_set_frame(text_layer_get_layer(s_transcript_layer), transcript);
}

static void set_prompt(void) {
  text_layer_set_text(s_transcript_layer, s_dictation
    ? "Press Select to speak"
    : "This watch has no microphone");
}

static void cancel_response_timer(void) {
  if (s_response_timer) {
    app_timer_cancel(s_response_timer);
    s_response_timer = NULL;
  }
}

static void response_timeout(void *context) {
  s_response_timer = NULL;
  s_waiting = false;
  set_status("No response from phone");
}

static void send_transcript(const char *transcript) {
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    set_status("Phone unavailable");
    return;
  }

  dict_write_cstring(iterator, MESSAGE_KEY_TRANSCRIPT, transcript);
  if (app_message_outbox_send() != APP_MSG_OK) {
    set_status("Send failed");
    return;
  }

  s_waiting = true;
  set_status("Sending...");
  cancel_response_timer();
  s_response_timer = app_timer_register(
    RESPONSE_TIMEOUT_MS,
    response_timeout,
    NULL
  );
}

static const char *dictation_error(DictationSessionStatus status) {
  switch (status) {
    case DictationSessionStatusFailureNoSpeechDetected:
      return "Didn't hear anything";
    case DictationSessionStatusFailureConnectivityError:
      return "Phone not connected";
    case DictationSessionStatusFailureDisabled:
      return "Dictation is turned off";
    case DictationSessionStatusFailureTranscriptionRejected:
    case DictationSessionStatusFailureTranscriptionRejectedWithError:
    case DictationSessionStatusFailureSystemAborted:
      return "Cancelled";
    default:
      return "Dictation failed";
  }
}

static void dictation_callback(DictationSession *session,
                               DictationSessionStatus status,
                               char *transcription,
                               void *context) {
  if (status != DictationSessionStatusSuccess) {
    if (!s_waiting) {
      set_prompt();
      set_status(dictation_error(status));
    }
    return;
  }

  snprintf(s_transcript, sizeof(s_transcript), "\"%s\"", transcription);
  text_layer_set_text(s_transcript_layer, s_transcript);
  send_transcript(transcription);
}

static void start_dictation(void) {
  if (!s_dictation) {
    return;
  }

  cancel_response_timer();
  s_waiting = false;
  dictation_session_start(s_dictation);
}

static void start_timer_fired(void *context) {
  s_start_timer = NULL;
  start_dictation();
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  start_dictation();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  cancel_response_timer();
  s_waiting = false;

  const Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT);
  const Tuple *reply_tuple = dict_find(iterator, MESSAGE_KEY_REPLY);
  if (!result_tuple) {
    set_status("Invalid phone response");
    return;
  }

  const int32_t result = result_tuple->value->int32;
  if (reply_tuple && reply_tuple->type == TUPLE_CSTRING) {
    snprintf(s_reply, sizeof(s_reply), "%s", reply_tuple->value->cstring);
  } else if (result == RESULT_OK) {
    snprintf(s_reply, sizeof(s_reply), "Done");
  } else if (result == RESULT_NOT_UNDERSTOOD) {
    snprintf(s_reply, sizeof(s_reply), "Sorry, I didn't understand");
  } else if (result == RESULT_PERMISSION_REQUIRED) {
    snprintf(s_reply, sizeof(s_reply), "Grant access on phone");
  } else {
    snprintf(s_reply, sizeof(s_reply), "Could not do that");
  }
  set_status(s_reply);

  if (result == RESULT_OK) {
    vibes_short_pulse();
  } else {
    vibes_double_pulse();
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  cancel_response_timer();
  s_waiting = false;
  set_status("Phone response lost");
}

static void outbox_failed(DictionaryIterator *iterator,
                          AppMessageResult reason,
                          void *context) {
  cancel_response_timer();
  s_waiting = false;
  set_status("Phone unavailable");
}

static void icon_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int16_t x = (bounds.size.w - ICON_WIDTH) / 2;
  const GColor accent = PBL_IF_COLOR_ELSE(GColorTiffanyBlue, GColorWhite);

  // Capsule, U-shaped holder, stem and base of a microphone.
  graphics_context_set_fill_color(ctx, accent);
  graphics_context_set_stroke_color(ctx, accent);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_fill_rect(ctx, GRect(x + 8, 0, 12, 21), 6, GCornersAll);
  graphics_draw_arc(ctx, GRect(x + 2, 6, 24, 20), GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
  graphics_draw_line(ctx, GPoint(x + 14, 26), GPoint(x + 14, 30));
  graphics_draw_line(ctx, GPoint(x + 8, 30), GPoint(x + 20, 30));
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  const int16_t icon_top = PBL_IF_ROUND_ELSE(bounds.size.h / 8, 12);
  s_text_top = icon_top + ICON_HEIGHT + 8;
  const int16_t inset = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 4);

  window_set_background_color(window, GColorBlack);

  s_icon_layer = layer_create(GRect(0, icon_top, bounds.size.w, ICON_HEIGHT));
  layer_set_update_proc(s_icon_layer, icon_update_proc);
  layer_add_child(root, s_icon_layer);

  s_transcript_layer = text_layer_create(GRect(
    inset,
    s_text_top,
    bounds.size.w - inset * 2,
    bounds.size.h - s_text_top - STATUS_HEIGHT
  ));
  text_layer_set_background_color(s_transcript_layer, GColorClear);
  text_layer_set_text_color(s_transcript_layer, GColorWhite);
  text_layer_set_font(s_transcript_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_transcript_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_transcript_layer,
                               GTextOverflowModeTrailingEllipsis);
  layer_add_child(root, text_layer_get_layer(s_transcript_layer));

  s_status_layer = text_layer_create(GRect(
    0,
    bounds.size.h - STATUS_HEIGHT,
    bounds.size.w,
    STATUS_HEIGHT
  ));
  text_layer_set_background_color(s_status_layer, GColorWhite);
  text_layer_set_text_color(s_status_layer, GColorBlack);
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer,
                               GTextOverflowModeTrailingEllipsis);
  layer_add_child(root, text_layer_get_layer(s_status_layer));
#if defined(PBL_ROUND)
  // Round screens clip the bottom corners, so let the status follow the edge.
  text_layer_enable_screen_text_flow_and_paging(s_status_layer, 4);
#endif

  set_prompt();
  set_status("www.ark-am.com");
}

static void window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_transcript_layer);
  layer_destroy(s_icon_layer);
}

static void init(void) {
  // Returns NULL on watches without a microphone, such as the Pebble 2 SE.
  s_dictation = dictation_session_create(TRANSCRIPT_SIZE,
                                         dictation_callback,
                                         NULL);
  if (s_dictation) {
    // The phone acts on the words straight away, so let the user check them.
    dictation_session_enable_confirmation(s_dictation, true);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(REPLY_SIZE + 64, TRANSCRIPT_SIZE + 64);

  // Opening the app is the request to speak, so start listening right away.
  if (s_dictation) {
    s_start_timer = app_timer_register(START_DELAY_MS, start_timer_fired, NULL);
  }
}

static void deinit(void) {
  cancel_response_timer();
  if (s_start_timer) {
    app_timer_cancel(s_start_timer);
  }
  if (s_dictation) {
    dictation_session_destroy(s_dictation);
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
