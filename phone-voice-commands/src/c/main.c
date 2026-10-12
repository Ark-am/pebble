#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_TRANSCRIPT and friends.

enum {
  RESULT_OK = 0,
  RESULT_FAILED = 1,
  RESULT_NOT_UNDERSTOOD = 2,
  RESULT_PERMISSION_REQUIRED = 3,
};

// Settings come from the Settings page in the Pebble phone app (Clay, see
// src/pkjs/config.js) and are kept on the watch between launches.
enum {
  THEME_DARK = 0,
  THEME_LIGHT = 1,
};

enum {
  AFTER_SUCCESS_STAY = 0,
  AFTER_SUCCESS_CLOSE = 1,
};

#define THEME_STORAGE_KEY 1
#define AUTO_LISTEN_STORAGE_KEY 2
#define CONFIRM_STORAGE_KEY 3
#define AFTER_SUCCESS_STORAGE_KEY 4
#define VIBRATE_STORAGE_KEY 5
// Quick commands use storage keys 10 to 15, one per slot.
#define QUICK_STORAGE_KEY_FIRST 10

// Phrases saved on the Settings page that run without speaking. They are sent
// to the phone exactly as if dictated; an empty slot is hidden.
#define QUICK_COUNT 6
#define QUICK_SIZE 64
static char s_quick[QUICK_COUNT][QUICK_SIZE] = {
  "Flashlight on",
  "Flashlight off",
  "Set a timer for 5 minutes",
  "Pause",
  "Next song",
  "",
};

static int32_t s_theme = THEME_DARK;
static bool s_auto_listen = true;
static bool s_confirm = true;
static int32_t s_after_success = AFTER_SUCCESS_STAY;
static bool s_vibrate = true;

static Window *s_window;
static Window *s_quick_window;
static MenuLayer *s_quick_menu;
static Layer *s_icon_layer;
static TextLayer *s_transcript_layer;
static TextLayer *s_status_layer;
static DictationSession *s_dictation;
static AppTimer *s_response_timer;
static AppTimer *s_start_timer;
static AppTimer *s_exit_timer;
static bool s_waiting;

// Dictation results longer than this are truncated by the system.
#define TRANSCRIPT_SIZE 256
#define REPLY_SIZE 128
#define RESPONSE_TIMEOUT_MS 10000
// How long the "Get ready" screen shows before the listening screen opens,
// giving the phone's speech recognition a moment to be ready.
#define READY_DELAY_MS 1500
// Long enough to read the reply before returning to the watchface.
#define EXIT_DELAY_MS 2000

#define ICON_WIDTH 28
#define ICON_HEIGHT 32
// The status bar is at least two lines tall and grows to fit longer replies.
#define STATUS_HEIGHT PBL_IF_ROUND_ELSE(52, 44)
#define STATUS_PADDING 8

static char s_transcript[TRANSCRIPT_SIZE + 2];
static char s_reply[REPLY_SIZE];
// The phone's "Did you mean...?" phrase, sent back as if spoken when the user
// presses Select.
static char s_suggestion[TRANSCRIPT_SIZE];
static bool s_has_suggestion;
static int16_t s_text_top;

static GColor background_color(void) {
  return s_theme == THEME_LIGHT ? GColorWhite : GColorBlack;
}

static GColor text_color(void) {
  return s_theme == THEME_LIGHT ? GColorBlack : GColorWhite;
}

static GColor accent_color(void) {
  // The menu icon's teal reads well on both black and white.
  return PBL_IF_COLOR_ELSE(GColorTiffanyBlue, text_color());
}

static void apply_theme(void) {
  window_set_background_color(s_window, background_color());
  text_layer_set_text_color(s_transcript_layer, text_color());
  // The status bar is the inverse of the screen.
  text_layer_set_background_color(s_status_layer, text_color());
  text_layer_set_text_color(s_status_layer, background_color());
  layer_mark_dirty(s_icon_layer);
}

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
    ? "Select: speak\nUp: quick commands"
    : "No microphone\nUp: quick commands");
}

static void cancel_response_timer(void) {
  if (s_response_timer) {
    app_timer_cancel(s_response_timer);
    s_response_timer = NULL;
  }
}

static void cancel_exit_timer(void) {
  if (s_exit_timer) {
    app_timer_cancel(s_exit_timer);
    s_exit_timer = NULL;
  }
}

static void exit_to_watchface(void *context) {
  s_exit_timer = NULL;
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  window_stack_pop_all(true);
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

static void cancel_start_timer(void) {
  if (s_start_timer) {
    app_timer_cancel(s_start_timer);
    s_start_timer = NULL;
  }
}

// Opens the system's listening screen.
static void listen_now(void) {
  cancel_start_timer();
  dictation_session_start(s_dictation);
}

static void start_timer_fired(void *context) {
  s_start_timer = NULL;
  listen_now();
}

// The listening screen belongs to the system and cannot show our own hints,
// so explain it first, then open it after a short pause.
static void start_dictation(void) {
  if (!s_dictation) {
    return;
  }

  cancel_response_timer();
  cancel_exit_timer();
  s_waiting = false;
  s_has_suggestion = false;
  text_layer_set_text(s_transcript_layer, "Get ready...");
  set_status("Speak when the microphone appears, then press Select when done");
  cancel_start_timer();
  s_start_timer = app_timer_register(READY_DELAY_MS, start_timer_fired, NULL);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_has_suggestion) {
    // Accept "Did you mean...?": send the phrase as if it had been spoken.
    s_has_suggestion = false;
    send_transcript(s_suggestion);
    return;
  }
  // Select on the "Get ready" screen skips the pause.
  if (s_start_timer) {
    listen_now();
    return;
  }
  start_dictation();
}

static uint16_t quick_count(void) {
  uint16_t count = 0;
  for (int i = 0; i < QUICK_COUNT; i++) {
    if (s_quick[i][0]) {
      count++;
    }
  }
  return count;
}

// The phrase shown in a row: the row-th filled slot, skipping empty ones.
static const char *quick_at(uint16_t row) {
  for (int i = 0; i < QUICK_COUNT; i++) {
    if (s_quick[i][0] && row-- == 0) {
      return s_quick[i];
    }
  }
  return NULL;
}

static uint16_t quick_get_num_rows(MenuLayer *menu_layer,
                                   uint16_t section_index,
                                   void *context) {
  const uint16_t count = quick_count();
  // With nothing saved, one row explains where to add them.
  return count ? count : 1;
}

static int16_t quick_get_cell_height(MenuLayer *menu_layer,
                                     MenuIndex *cell_index,
                                     void *context) {
  return PBL_IF_ROUND_ELSE(52, 48);
}

static void quick_draw_row(GContext *ctx,
                           const Layer *cell_layer,
                           MenuIndex *cell_index,
                           void *context) {
  const char *text = quick_count()
    ? quick_at(cell_index->row)
    : "Add quick commands in the Pebble app's settings";
  const GRect bounds = layer_get_bounds(cell_layer);
  const int16_t inset = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 6);
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GTextAlignment alignment = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);
  // Two lines fit, so most phrases show in full; centre one or two lines.
  GRect box = GRect(inset, 0, bounds.size.w - inset * 2, bounds.size.h - 4);
  const GSize size = graphics_text_layout_get_content_size(
    text, font, box, GTextOverflowModeTrailingEllipsis, alignment);
  // Gothic glyphs sit low in their line box, so lift the text a little.
  box.origin.y = (bounds.size.h - size.h) / 2 - 3;
  box.size.h = size.h + 4;
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static void quick_select(MenuLayer *menu_layer,
                         MenuIndex *cell_index,
                         void *context) {
  const char *phrase = quick_at(cell_index->row);
  window_stack_remove(s_quick_window, true);
  if (!phrase) {
    return;
  }
  // Runs straight away: choosing it from the list is the confirmation.
  cancel_exit_timer();
  s_has_suggestion = false;
  snprintf(s_transcript, sizeof(s_transcript), "\"%s\"", phrase);
  text_layer_set_text(s_transcript_layer, s_transcript);
  send_transcript(phrase);
}

static void quick_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  window_set_background_color(window, background_color());

  s_quick_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_quick_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = quick_get_num_rows,
    .get_cell_height = quick_get_cell_height,
    .draw_row = quick_draw_row,
    .select_click = quick_select,
  });
  menu_layer_set_normal_colors(s_quick_menu, background_color(), text_color());
  menu_layer_set_highlight_colors(s_quick_menu,
                                  PBL_IF_COLOR_ELSE(accent_color(), text_color()),
                                  PBL_IF_COLOR_ELSE(GColorBlack, background_color()));
  menu_layer_set_click_config_onto_window(s_quick_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_quick_menu));
}

static void quick_window_unload(Window *window) {
  menu_layer_destroy(s_quick_menu);
  s_quick_menu = NULL;
}

static void show_quick_commands(void) {
  s_has_suggestion = false;
  // Up or Down on the "Get ready" screen picks a quick command instead.
  if (s_start_timer) {
    cancel_start_timer();
    set_prompt();
    set_status("www.ark-am.com");
  }
  window_stack_push(s_quick_window, true);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  show_quick_commands();
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  // While "Did you mean...?" is showing, Down turns it down and listens again.
  if (s_has_suggestion) {
    start_dictation();
    return;
  }
  show_quick_commands();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static int32_t tuple_int(const Tuple *tuple) {
  if (tuple->type == TUPLE_CSTRING) {
    return atoi(tuple->value->cstring);
  }
  // Clay sends toggles as one-byte integers.
  switch (tuple->length) {
    case 1:
      return tuple->type == TUPLE_INT ? tuple->value->int8 : tuple->value->uint8;
    case 2:
      return tuple->type == TUPLE_INT ? tuple->value->int16 : tuple->value->uint16;
    default:
      return tuple->value->int32;
  }
}

static void load_settings(void) {
  if (persist_exists(THEME_STORAGE_KEY)) {
    s_theme = persist_read_int(THEME_STORAGE_KEY) == THEME_LIGHT ? THEME_LIGHT : THEME_DARK;
  }
  if (persist_exists(AUTO_LISTEN_STORAGE_KEY)) {
    s_auto_listen = persist_read_bool(AUTO_LISTEN_STORAGE_KEY);
  }
  if (persist_exists(CONFIRM_STORAGE_KEY)) {
    s_confirm = persist_read_bool(CONFIRM_STORAGE_KEY);
  }
  if (persist_exists(AFTER_SUCCESS_STORAGE_KEY)) {
    s_after_success = persist_read_int(AFTER_SUCCESS_STORAGE_KEY) == AFTER_SUCCESS_CLOSE
      ? AFTER_SUCCESS_CLOSE
      : AFTER_SUCCESS_STAY;
  }
  if (persist_exists(VIBRATE_STORAGE_KEY)) {
    s_vibrate = persist_read_bool(VIBRATE_STORAGE_KEY);
  }
  for (int i = 0; i < QUICK_COUNT; i++) {
    if (persist_exists(QUICK_STORAGE_KEY_FIRST + i)) {
      persist_read_string(QUICK_STORAGE_KEY_FIRST + i, s_quick[i], QUICK_SIZE);
    }
  }
}

// Applies the values saved on the Settings page. Returns false if the message
// held no settings (it is then the phone's reply to a command).
static bool apply_settings(DictionaryIterator *iterator) {
  bool found = false;
  const Tuple *tuple;

  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    found = true;
    s_theme = tuple_int(tuple) == THEME_LIGHT ? THEME_LIGHT : THEME_DARK;
    persist_write_int(THEME_STORAGE_KEY, s_theme);
    apply_theme();
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_AUTO_LISTEN))) {
    found = true;
    s_auto_listen = tuple_int(tuple) != 0;
    persist_write_bool(AUTO_LISTEN_STORAGE_KEY, s_auto_listen);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CONFIRM))) {
    found = true;
    s_confirm = tuple_int(tuple) != 0;
    persist_write_bool(CONFIRM_STORAGE_KEY, s_confirm);
    if (s_dictation) {
      dictation_session_enable_confirmation(s_dictation, s_confirm);
    }
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_AFTER_SUCCESS))) {
    found = true;
    s_after_success = tuple_int(tuple) == AFTER_SUCCESS_CLOSE
      ? AFTER_SUCCESS_CLOSE
      : AFTER_SUCCESS_STAY;
    persist_write_int(AFTER_SUCCESS_STORAGE_KEY, s_after_success);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_VIBRATE))) {
    found = true;
    s_vibrate = tuple_int(tuple) != 0;
    persist_write_bool(VIBRATE_STORAGE_KEY, s_vibrate);
  }

  const uint32_t quick_keys[QUICK_COUNT] = {
    MESSAGE_KEY_QUICK_1, MESSAGE_KEY_QUICK_2, MESSAGE_KEY_QUICK_3,
    MESSAGE_KEY_QUICK_4, MESSAGE_KEY_QUICK_5, MESSAGE_KEY_QUICK_6,
  };
  for (int i = 0; i < QUICK_COUNT; i++) {
    tuple = dict_find(iterator, quick_keys[i]);
    if (tuple && tuple->type == TUPLE_CSTRING) {
      found = true;
      snprintf(s_quick[i], QUICK_SIZE, "%s", tuple->value->cstring);
      persist_write_string(QUICK_STORAGE_KEY_FIRST + i, s_quick[i]);
    }
  }
  if (s_quick_menu) {
    menu_layer_reload_data(s_quick_menu);
  }
  return found;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  // Settings from the Settings page can arrive at any time.
  if (apply_settings(iterator)) {
    return;
  }

  cancel_response_timer();
  s_waiting = false;

  const Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT);
  const Tuple *reply_tuple = dict_find(iterator, MESSAGE_KEY_REPLY);
  if (!result_tuple) {
    set_status("Invalid phone response");
    return;
  }

  const int32_t result = result_tuple->value->int32;
  const Tuple *suggestion_tuple = dict_find(iterator, MESSAGE_KEY_SUGGESTION);
  if (suggestion_tuple && suggestion_tuple->type == TUPLE_CSTRING) {
    snprintf(s_suggestion, sizeof(s_suggestion), "%s", suggestion_tuple->value->cstring);
    s_has_suggestion = true;
    snprintf(s_transcript, sizeof(s_transcript), "\"%s\"", s_suggestion);
    text_layer_set_text(s_transcript_layer, s_transcript);
    set_status("Did you mean this?\nSelect: yes. Down: speak again");
    if (s_vibrate) {
      vibes_double_pulse();
    }
    return;
  }

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

  if (s_vibrate) {
    if (result == RESULT_OK) {
      vibes_short_pulse();
    } else {
      vibes_double_pulse();
    }
  }

  // Only leave after success, so failures stay visible for a retry.
  if (result == RESULT_OK && s_after_success == AFTER_SUCCESS_CLOSE) {
    cancel_exit_timer();
    s_exit_timer = app_timer_register(EXIT_DELAY_MS, exit_to_watchface, NULL);
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
  const GColor accent = accent_color();

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
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer,
                               GTextOverflowModeTrailingEllipsis);
  layer_add_child(root, text_layer_get_layer(s_status_layer));
#if defined(PBL_ROUND)
  // Round screens clip the bottom corners, so let the status follow the edge.
  text_layer_enable_screen_text_flow_and_paging(s_status_layer, 4);
#endif

  apply_theme();
  set_prompt();
  set_status("www.ark-am.com");
}

static void window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_transcript_layer);
  layer_destroy(s_icon_layer);
}

static void init(void) {
  load_settings();

  // Returns NULL on watches without a microphone, such as the Pebble 2 SE.
  s_dictation = dictation_session_create(TRANSCRIPT_SIZE,
                                         dictation_callback,
                                         NULL);
  if (s_dictation) {
    // The phone acts on the words straight away, so by default let the user
    // check them; the Settings page can turn this off.
    dictation_session_enable_confirmation(s_dictation, s_confirm);
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  s_quick_window = window_create();
  window_set_window_handlers(s_quick_window, (WindowHandlers) {
    .load = quick_window_load,
    .unload = quick_window_unload,
  });

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  // A reply can carry the reply text and a full-length suggestion, and the
  // Settings page sends every setting, quick commands included, at once.
  app_message_open(REPLY_SIZE + TRANSCRIPT_SIZE + 128, TRANSCRIPT_SIZE + 64);

  // Opening the app is the request to speak, so start listening right away
  // unless the Settings page says to wait for Select.
  if (s_dictation && s_auto_listen) {
    start_dictation();
  }
}

static void deinit(void) {
  cancel_response_timer();
  cancel_exit_timer();
  cancel_start_timer();
  if (s_dictation) {
    dictation_session_destroy(s_dictation);
  }
  window_destroy(s_quick_window);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
