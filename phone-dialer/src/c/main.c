#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_REQUEST and friends.
// The Android companion owns the contact data; the watch only ever holds one
// page of a list at a time so it fits in the smallest app heap.

enum {
  REQUEST_LIST = 1,
  REQUEST_CALL = 2,
  REQUEST_DIAL = 3,
  // 4 was the old settings sync; keep it unused so an old companion is not misread.
  REQUEST_HANGUP = 5,
  // Not reply-driven: asks for an audio output; the phone reports the result
  // with an audio update, like any other change.
  REQUEST_AUDIO = 6,
  // Not reply-driven either: mutes or unmutes the call (AUDIO_MUTED); the
  // phone reports the result with an audio update.
  REQUEST_MUTE = 7,
};

// The phone app's COMPANION_VERSION from which it can end calls, and from
// which it can switch the call's audio output.
#define COMPANION_CAN_HANG_UP 2
#define COMPANION_CAN_SWITCH_AUDIO 3
#define COMPANION_CAN_MUTE 4

// Audio outputs, as Android's CallAudioState routes (also used as a bit mask).
enum {
  AUDIO_EARPIECE = 1,
  AUDIO_BLUETOOTH = 2,
  AUDIO_HEADSET = 4,
  AUDIO_SPEAKER = 8,
};

enum {
  LIST_FAVORITES = 0,
  LIST_LETTERS = 1,
  LIST_CONTACTS = 2,
  LIST_RECENTS = 3,
};

enum {
  RESULT_OK = 0,
  RESULT_FAILED = 1,
  RESULT_PERMISSION_REQUIRED = 2,
  RESULT_NOT_FOUND = 3,
  RESULT_CALL_LOG_PERMISSION = 4,
  RESULT_INVALID_NUMBER = 5,
  RESULT_HANGUP_PERMISSION = 6,
  RESULT_HANGUP_UNSUPPORTED = 7,
};

// Separators the companion uses inside the ITEMS string.
#define FIELD_SEPARATOR '\x1f'
#define ENTRY_SEPARATOR '\x1e'

#define PAGE_SIZE 20
#define TITLE_SIZE 32
#define SUBTITLE_SIZE 32
#define FILTER_SIZE 4
#define HEADING_SIZE 20
#define MAX_INBOX_SIZE 2048
// Room for the dictionary header and the non-ITEMS tuples in each reply.
#define INBOX_OVERHEAD 80
#define OUTBOX_SIZE 128
#define NUMBER_SIZE 32
#define TOKEN_STORAGE_KEY 1
#define MENU_ORDER_STORAGE_KEY 2
// Keys 3, 4 and 6 held earlier settings (reorder hint, sync stamps); keep them unused.
#define THEME_STORAGE_KEY 5
#define TOUCH_STORAGE_KEY 7
#define TOUCH_VIBE_STORAGE_KEY 8

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define RESPONSE_TIMEOUT_MS 10000
// Long enough to read the confirmation before returning to the watchface.
#define EXIT_DELAY_MS 1500

// App theme, chosen in the phone app: dark or light. Colour watches use gray
// keys with a red-orange Delete and a blue-green Call; black-and-white watches
// use outlines that invert when highlighted. Until a theme is chosen, colour
// watches start dark and black-and-white watches start light.
typedef enum {
  THEME_DARK = 0,
  THEME_LIGHT = 1,
} ThemeId;

typedef struct {
  GColor background;
  GColor text;
  GColor hint;
  GColor divider;
  GColor highlight;
  GColor highlight_text;
  GColor call;
  GColor warning;
  GColor list_secondary;
  GColor list_good;
  GColor list_bad;
  GColor key;
  GColor key_text;
  GColor key_pressed;
  GColor key_pressed_text;
  GColor key_ring;
  GColor call_key;
  GColor call_key_pressed;
  GColor delete_key;
  GColor delete_key_pressed;
  GColor function_key_text;
} Theme;

static Theme s_theme;
static int32_t s_theme_id = PBL_IF_COLOR_ELSE(THEME_DARK, THEME_LIGHT);
// Touch settings from the Settings page; both are on until changed there.
static bool s_touch_enabled = true;
static bool s_touch_vibe = true;

static void theme_load(int32_t id) {
  const bool light = id == THEME_LIGHT;
#if defined(PBL_COLOR)
  if (light) {
    s_theme = (Theme) {
      .background = GColorWhite, .text = GColorBlack, .hint = GColorDarkGray,
      .divider = GColorLightGray, .highlight = GColorLightGray, .highlight_text = GColorBlack,
      .call = GColorJaegerGreen, .warning = GColorOrange,
      .list_secondary = GColorDarkGray, .list_good = GColorJaegerGreen, .list_bad = GColorRed,
      .key = GColorLightGray, .key_text = GColorBlack,
      .key_pressed = GColorDarkGray, .key_pressed_text = GColorWhite, .key_ring = GColorBlack,
      .call_key = GColorJaegerGreen, .call_key_pressed = GColorIslamicGreen,
      .delete_key = GColorOrange, .delete_key_pressed = GColorRed,
      .function_key_text = GColorWhite,
    };
  } else {
    s_theme = (Theme) {
      .background = GColorBlack, .text = GColorWhite, .hint = GColorLightGray,
      .divider = GColorDarkGray, .highlight = GColorDarkGray, .highlight_text = GColorWhite,
      .call = GColorJaegerGreen, .warning = GColorOrange,
      .list_secondary = GColorLightGray, .list_good = GColorMediumAquamarine,
      .list_bad = GColorSunsetOrange,
      .key = GColorDarkGray, .key_text = GColorWhite,
      .key_pressed = GColorLightGray, .key_pressed_text = GColorBlack, .key_ring = GColorWhite,
      .call_key = GColorJaegerGreen, .call_key_pressed = GColorMalachite,
      .delete_key = GColorOrange, .delete_key_pressed = GColorSunsetOrange,
      .function_key_text = GColorWhite,
    };
  }
#else
  const GColor paper = light ? GColorWhite : GColorBlack;
  const GColor ink = light ? GColorBlack : GColorWhite;
  s_theme = (Theme) {
    .background = paper, .text = ink, .hint = ink, .divider = ink,
    .highlight = ink, .highlight_text = paper, .call = ink, .warning = ink,
    .list_secondary = ink, .list_good = ink, .list_bad = ink,
    .key = paper, .key_text = ink, .key_pressed = ink, .key_pressed_text = paper, .key_ring = ink,
    .call_key = ink, .call_key_pressed = paper, .delete_key = paper, .delete_key_pressed = ink,
    .function_key_text = paper,
  };
#endif
}

// Icons are drawn as vector shapes in a square box so they work on every
// platform, including black-and-white ones, and follow the row's highlight.
#define ICON_SIZE 24
#define ICON_GAP 8
#define ROW_MARGIN 8

typedef struct {
  char id[21];
  char title[TITLE_SIZE];
  char subtitle[SUBTITLE_SIZE];
} Entry;

typedef enum {
  LIST_STATE_LOADING,
  LIST_STATE_READY,
  LIST_STATE_ERROR,
} ListState;

typedef struct {
  Window *window;
  MenuLayer *menu_layer;
  int32_t list;
  char filter[FILTER_SIZE];
  char heading[HEADING_SIZE];
  int32_t offset;    // Position of entries[0] in the full list.
  int32_t total;     // Length of the full list on the phone.
  int32_t received;  // Entries of the current page received so far.
  int32_t select_row_after_load;
  ListState state;
  const char *message;
  Entry entries[PAGE_SIZE];
} ListView;

typedef enum {
  MAIN_ROW_DIALER,
  MAIN_ROW_RECENTS,
  MAIN_ROW_FAVORITES,
  MAIN_ROW_CONTACTS,
  MAIN_ROW_COUNT,
} MainRow;

static Window *s_main_window;
static MenuLayer *s_main_menu_layer;
// Main menu order, chosen on the Settings page in the Pebble phone app and
// kept on the watch so the menu is right even when the phone is away.
static uint8_t s_main_order[MAIN_ROW_COUNT] = {
  MAIN_ROW_DIALER, MAIN_ROW_RECENTS, MAIN_ROW_FAVORITES, MAIN_ROW_CONTACTS,
};
static GPath *s_star_path;

static Window *s_call_window;
static TextLayer *s_call_status_layer;
static TextLayer *s_call_name_layer;
static char s_call_name[TITLE_SIZE];
static Layer *s_call_button_layer;
static GPath *s_call_icon_path;
static GPathInfo s_call_icon_info;
static GPoint s_call_icon_points[24];
static TextLayer *s_call_number_layer;
static char s_call_number[TITLE_SIZE];
static GPath *s_hangup_icon_path;
static GPathInfo s_hangup_icon_info;
static GPoint s_hangup_icon_points[24];
static Entry s_call_entry;
static int32_t s_call_source;
// The COMPANION_VERSION of the phone app that placed the call, or 0 if it is
// from before versions were sent; older apps never answer what they lack.
static int32_t s_call_companion_version;
// The call's audio output and the outputs available, from the phone. The mask
// is 0 until the phone reports it (it needs the watch linked on the phone).
static int32_t s_audio_route;
static int32_t s_audio_routes;
static bool s_audio_muted;
// How the call is going, from the phone (when the Pebble is linked there):
// when it connected (0 until then), whether it is on hold, and until when an
// error message keeps the status line instead of the timer.
static time_t s_call_connected_at;
static bool s_call_on_hold;
static time_t s_call_status_until;
static bool s_call_ticking;
static Layer *s_call_audio_layer;
static Layer *s_call_mute_layer;

typedef enum {
  CALL_CONFIRM,   // Waiting for Select (or a tap on Call) to place the call.
  CALL_SENDING,   // Asked the phone to place the call.
  CALL_ACTIVE,    // The call is going; Down (or a tap on End) hangs up.
  CALL_ENDING,    // Asked the phone to end the call.
  CALL_ENDED,     // Shown briefly before returning to the watchface.
  CALL_BLOCKED,   // Nothing to call, or the call request failed.
} CallState;
static CallState s_call_state;
static Window *s_dial_window;
static Layer *s_dial_layer;
static char s_number[NUMBER_SIZE];
static uint8_t s_key;

// Phone keypad order, then a bottom row of Delete, "+" and Call.
enum {
  KEY_DELETE = 12,
  KEY_PLUS = 13,
  KEY_CALL = 14,
  KEY_COUNT = 15,
};
static const char *s_keys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9",
                               "*", "0", "#", NULL, "+", NULL };

// Layout is worked out once per window so drawing and touch agree on it.
static GRect s_key_rects[KEY_COUNT];
static GRect s_display_rect;
static GFont s_key_font;
static GFont s_number_font;
static GFont s_number_small_font;
static GPath *s_handset_path;
static GPathInfo s_handset_info;
static GPoint s_handset_points[24];
static int16_t s_icon_size;
// Hidden while the keypad is driven by touch; the buttons bring it back.
static bool s_show_focus;
static int s_pressed_key = -1;
static AppTimer *s_press_timer;

// Only one request is in flight at a time. Replies carry the request's token,
// so anything left over from an abandoned request is ignored.
static int32_t s_token;
static ListView *s_waiting_list;
static bool s_waiting_call;
static AppTimer *s_response_timer;
static AppTimer *s_exit_timer;
static uint32_t s_inbox_size;

static const GPathInfo STAR_PATH_INFO = {
  .num_points = 10,
  .points = (GPoint[]) {
    {12, 1}, {15, 8}, {23, 9}, {16, 13}, {18, 21},
    {12, 16}, {6, 21}, {8, 13}, {1, 9}, {9, 8},
  },
};

static void list_window_push(int32_t list, const char *filter);
static bool apply_settings(DictionaryIterator *iterator);

// ---------------------------------------------------------------------------
// Requests

static const char *result_message(int32_t result) {
  switch (result) {
    case RESULT_PERMISSION_REQUIRED:
      return "Allow access on phone";
    case RESULT_NOT_FOUND:
      return "Number unavailable";
    case RESULT_CALL_LOG_PERMISSION:
      return "Allow call history";
    case RESULT_INVALID_NUMBER:
      return "Invalid number";
    case RESULT_HANGUP_PERMISSION:
      return "Allow Phone on phone to end calls";
    case RESULT_HANGUP_UNSUPPORTED:
      return "End the call on the phone";
    default:
      return "Phone error";
  }
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

static void list_show_error(ListView *view, const char *message);
static void call_request_failed(const char *message);

// Moves to a new request token, so any late reply to the current request is
// ignored. Kept across launches so a restarted app never reuses a token.
static void abandon_request(void) {
  s_token = s_token >= INT32_MAX ? 1 : s_token + 1;
  persist_write_int(TOKEN_STORAGE_KEY, s_token);
}

static void fail_request(const char *message) {
  cancel_response_timer();
  ListView *view = s_waiting_list;
  const bool waiting_call = s_waiting_call;
  s_waiting_list = NULL;
  s_waiting_call = false;
  // Ignore any late replies to the request that just failed.
  abandon_request();

  if (view) {
    list_show_error(view, message);
  } else if (waiting_call) {
    call_request_failed(message);
  }
}

static void response_timeout(void *context) {
  s_response_timer = NULL;
  fail_request("No response");
}

static void restart_response_timer(void) {
  cancel_response_timer();
  s_response_timer = app_timer_register(RESPONSE_TIMEOUT_MS, response_timeout, NULL);
}

// Starts a message to the phone, or returns NULL with the reason in *error.
static DictionaryIterator *begin_request(int32_t request, const char **error) {
  DictionaryIterator *iterator;
  const AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result == APP_MSG_BUSY) {
    *error = "Phone busy; try again";
    return NULL;
  }
  if (result != APP_MSG_OK) {
    *error = "Phone unavailable";
    return NULL;
  }

  // A new request replaces any earlier one still waiting for its reply.
  cancel_response_timer();
  s_waiting_list = NULL;
  s_waiting_call = false;
  abandon_request();

  dict_write_int32(iterator, MESSAGE_KEY_REQUEST, request);
  dict_write_int32(iterator, MESSAGE_KEY_TOKEN, s_token);
  return iterator;
}

static bool send_request(const char **error) {
  if (app_message_outbox_send() != APP_MSG_OK) {
    *error = "Send failed";
    return false;
  }
  restart_response_timer();
  return true;
}

// ---------------------------------------------------------------------------
// Lists

static bool list_has_previous(const ListView *view) {
  return view->state == LIST_STATE_READY && view->offset > 0;
}

static bool list_has_more(const ListView *view) {
  return view->state == LIST_STATE_READY && view->offset + view->received < view->total;
}

static bool list_shows_message(const ListView *view) {
  return view->state != LIST_STATE_READY || view->received == 0;
}

static void list_request_page(ListView *view, int32_t offset, int32_t select_row) {
  view->state = LIST_STATE_LOADING;
  view->message = "Loading...";
  view->offset = offset;
  view->received = 0;
  view->select_row_after_load = select_row;
  menu_layer_reload_data(view->menu_layer);
  menu_layer_set_selected_index(view->menu_layer, MenuIndex(0, 0), MenuRowAlignCenter, false);

  const char *error = NULL;
  DictionaryIterator *iterator = begin_request(REQUEST_LIST, &error);
  if (!iterator) {
    list_show_error(view, error);
    return;
  }

  dict_write_int32(iterator, MESSAGE_KEY_LIST, view->list);
  dict_write_int32(iterator, MESSAGE_KEY_OFFSET, offset);
  dict_write_int32(iterator, MESSAGE_KEY_LIMIT, PAGE_SIZE);
  dict_write_int32(iterator, MESSAGE_KEY_CAPACITY, s_inbox_size - INBOX_OVERHEAD);
  if (view->filter[0]) {
    dict_write_cstring(iterator, MESSAGE_KEY_FILTER, view->filter);
  }

  if (!send_request(&error)) {
    list_show_error(view, error);
    return;
  }
  s_waiting_list = view;
}

static void list_show_error(ListView *view, const char *message) {
  view->state = LIST_STATE_ERROR;
  view->message = message;
  view->received = 0;
  menu_layer_reload_data(view->menu_layer);
  menu_layer_set_selected_index(view->menu_layer, MenuIndex(0, 0), MenuRowAlignCenter, false);
}

static void list_finish_page(ListView *view) {
  if (view->received == 0 && view->offset > 0) {
    // The list shrank on the phone since the last page; start again.
    list_request_page(view, 0, 0);
    return;
  }

  view->state = LIST_STATE_READY;
  if (view->total == 0) {
    view->message = view->list == LIST_RECENTS ? "No recent calls"
      : view->list == LIST_FAVORITES ? "No favorites" : "No contacts";
  }
  menu_layer_reload_data(view->menu_layer);

  const uint16_t rows = list_shows_message(view)
    ? 1
    : view->received + list_has_previous(view) + list_has_more(view);
  int32_t row = view->select_row_after_load;
  if (row < 0) {
    // The last entry, which sits above "More" when there is one.
    row = rows - 1 - list_has_more(view);
  }
  if (row < 0 || row >= rows) {
    row = 0;
  }
  menu_layer_set_selected_index(view->menu_layer, MenuIndex(0, row), MenuRowAlignCenter, false);
}

static void copy_field(char *dest, size_t size, const char *start, const char *end) {
  size_t length = end - start;
  if (length >= size) {
    length = size - 1;
    // Never leave half of a UTF-8 character at the end.
    while (length > 0 && (start[length] & 0xC0) == 0x80) {
      length--;
    }
  }
  memcpy(dest, start, length);
  dest[length] = '\0';
}

// ITEMS holds entries separated by ENTRY_SEPARATOR, each made of
// "id FIELD_SEPARATOR title FIELD_SEPARATOR subtitle".
static void list_store_items(ListView *view, int32_t first_index, const char *items) {
  int32_t slot = first_index - view->offset;
  const char *entry = items;
  while (*entry && slot < PAGE_SIZE) {
    const char *entry_end = strchr(entry, ENTRY_SEPARATOR);
    if (!entry_end) {
      entry_end = entry + strlen(entry);
    }

    const char *fields[3] = { entry, entry_end, entry_end };
    const char *field_ends[3] = { entry_end, entry_end, entry_end };
    int field = 0;
    for (const char *c = entry; c < entry_end && field < 2; c++) {
      if (*c == FIELD_SEPARATOR) {
        field_ends[field] = c;
        fields[++field] = c + 1;
      }
    }

    if (slot >= 0) {
      Entry *target = &view->entries[slot];
      copy_field(target->id, sizeof(target->id), fields[0], field_ends[0]);
      copy_field(target->title, sizeof(target->title), fields[1], field_ends[1]);
      copy_field(target->subtitle, sizeof(target->subtitle), fields[2], field_ends[2]);
      if (slot + 1 > view->received) {
        view->received = slot + 1;
      }
    }

    slot++;
    entry = *entry_end ? entry_end + 1 : entry_end;
  }
}

static void list_handle_reply(ListView *view, DictionaryIterator *iterator, int32_t result) {
  if (result != RESULT_OK) {
    fail_request(result_message(result));
    return;
  }

  const Tuple *total_tuple = dict_find(iterator, MESSAGE_KEY_TOTAL);
  const Tuple *offset_tuple = dict_find(iterator, MESSAGE_KEY_OFFSET);
  const Tuple *items_tuple = dict_find(iterator, MESSAGE_KEY_ITEMS);
  if (total_tuple) {
    view->total = total_tuple->value->int32;
  }
  if (offset_tuple && items_tuple && items_tuple->type == TUPLE_CSTRING) {
    list_store_items(view, offset_tuple->value->int32, items_tuple->value->cstring);
  }

  if (!dict_find(iterator, MESSAGE_KEY_FINAL)) {
    // More of this page is on its way; give it a fresh timeout.
    restart_response_timer();
    return;
  }

  cancel_response_timer();
  s_waiting_list = NULL;
  list_finish_page(view);
}

// List rows: the name large and bold, then a dimmer second line whose call
// type or number label gets a touch of colour, and a thin divider between rows.
#define LIST_MARGIN 6
// The separator recent calls put between the call type and the time.
#define RECENT_SEPARATOR " \xc2\xb7 "

// How much of a second line is the coloured part: the call type of a recent
// call ("Missed"), or the label of a number ("Mobile").
static size_t subtitle_lead_length(const ListView *view, const char *subtitle) {
  if (view->list == LIST_RECENTS) {
    const char *separator = strstr(subtitle, RECENT_SEPARATOR);
    return separator ? (size_t)(separator - subtitle) : 0;
  }
  if (view->list == LIST_FAVORITES || view->list == LIST_CONTACTS) {
    for (const char *c = subtitle; *c; c++) {
      if (c > subtitle && c[-1] == ' ' && (*c == '+' || *c == '(' || (*c >= '0' && *c <= '9'))) {
        return c - subtitle - 1;
      }
    }
  }
  return 0;
}

static GColor subtitle_lead_color(const ListView *view, const char *subtitle) {
  if (view->list == LIST_RECENTS &&
      (strncmp(subtitle, "Missed", 6) == 0 || strncmp(subtitle, "Declined", 8) == 0 ||
       strncmp(subtitle, "Blocked", 7) == 0)) {
    return s_theme.list_bad;
  }
  return s_theme.list_good;
}

static void draw_list_cell(GContext *ctx, const Layer *cell_layer, const char *title,
                           GColor title_color, const char *subtitle, size_t lead_length,
                           GColor lead_color) {
  const GRect bounds = layer_get_bounds(cell_layer);
  const bool highlighted = menu_cell_layer_is_highlighted(cell_layer);
  if (highlighted) {
    // Keep the colours readable on the highlight; black-and-white goes all white.
    title_color = s_theme.highlight_text;
    lead_color = PBL_IF_COLOR_ELSE(lead_color, s_theme.highlight_text);
  }
  const GColor secondary = highlighted ? s_theme.highlight_text : s_theme.list_secondary;
  const GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont subtitle_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  const GTextAlignment alignment = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);

#if defined(PBL_ROUND)
  // Rows away from the centre are short and show only the name.
  const bool compact = !highlighted;
  const int16_t margin = 12;
#else
  const bool compact = false;
  const int16_t margin = LIST_MARGIN;
#endif
  const int16_t width = bounds.size.w - 2 * margin;
  const bool two_lines = subtitle && subtitle[0] && !compact;

  // Gothic glyphs sit low in their line box, so lift each line a little.
  const int16_t title_top = two_lines ? bounds.size.h / 2 - 27 : bounds.size.h / 2 - 17;
  graphics_context_set_text_color(ctx, title_color);
  graphics_draw_text(ctx, title, compact ? fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD) : title_font,
                     GRect(margin, compact ? bounds.size.h / 2 - 13 : title_top, width, 30),
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);

  if (two_lines) {
    const int16_t top = bounds.size.h / 2 - 2;
    static char lead[SUBTITLE_SIZE];
    lead_length = MIN(lead_length, sizeof(lead) - 1);
    memcpy(lead, subtitle, lead_length);
    lead[lead_length] = '\0';
    const char *rest = subtitle + lead_length;

    const GRect line = GRect(margin, top, width, 22);
    const int16_t lead_w = lead_length ? graphics_text_layout_get_content_size(
      lead, subtitle_font, line, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w : 0;
    int16_t x = margin;
#if defined(PBL_ROUND)
    const int16_t rest_w = graphics_text_layout_get_content_size(
      rest, subtitle_font, line, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w;
    x = MAX(margin, (bounds.size.w - lead_w - rest_w) / 2);
#endif
    if (lead_length) {
      graphics_context_set_text_color(ctx, lead_color);
      graphics_draw_text(ctx, lead, subtitle_font, GRect(x, top, lead_w + 2, 22),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    graphics_context_set_text_color(ctx, secondary);
    graphics_draw_text(ctx, rest, subtitle_font,
                       GRect(x + lead_w, top, bounds.size.w - margin - x - lead_w, 22),
                       GTextOverflowModeTrailingEllipsis,
                       lead_length ? GTextAlignmentLeft : alignment, NULL);
  }

#if !defined(PBL_ROUND)
  if (!highlighted) {
    graphics_context_set_stroke_color(ctx, s_theme.divider);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(margin, bounds.size.h - 1),
                       GPoint(bounds.size.w - margin - 1, bounds.size.h - 1));
  }
#endif
}

static uint16_t list_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  const ListView *view = context;
  if (list_shows_message(view)) {
    return 1;
  }
  return view->received + list_has_previous(view) + list_has_more(view);
}

static int16_t list_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
#if defined(PBL_ROUND)
  return menu_layer_is_index_selected(menu_layer, cell_index) ? 60 : 36;
#else
  // Larger screens get a little more air between rows.
  return layer_get_bounds(menu_layer_get_layer(menu_layer)).size.h >= 200 ? 56 : 50;
#endif
}

static int16_t list_get_header_height(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  // Round screens have no room for a header above the centred selection.
  return PBL_IF_RECT_ELSE(MENU_CELL_BASIC_HEADER_HEIGHT, 0);
}

static void list_draw_header(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *context) {
  const ListView *view = context;
  menu_cell_basic_header_draw(ctx, cell_layer, view->heading);
}

static void list_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  const ListView *view = context;
  if (list_shows_message(view)) {
    const bool error = view->state == LIST_STATE_ERROR;
    draw_list_cell(ctx, cell_layer, view->message, error ? s_theme.list_bad : s_theme.text,
                   error ? "Select to retry" : NULL, 0, s_theme.list_good);
    return;
  }

  int32_t row = cell_index->row;
  if (list_has_previous(view)) {
    if (row == 0) {
      static char range[24];
      snprintf(range, sizeof(range), "%d-%d",
               (int)(MAX(view->offset - PAGE_SIZE, 0) + 1), (int)view->offset);
      draw_list_cell(ctx, cell_layer, "Previous", s_theme.list_good, range, 0, s_theme.list_good);
      return;
    }
    row--;
  }

  if (row >= view->received) {
    static char range[24];
    const int32_t first = view->offset + view->received + 1;
    const int32_t last = MIN(view->offset + view->received + PAGE_SIZE, view->total);
    snprintf(range, sizeof(range), "%d-%d of %d", (int)first, (int)last, (int)view->total);
    draw_list_cell(ctx, cell_layer, "More", s_theme.list_good, range, 0, s_theme.list_good);
    return;
  }

  const Entry *entry = &view->entries[row];
  draw_list_cell(ctx, cell_layer, entry->title, s_theme.text, entry->subtitle,
                 subtitle_lead_length(view, entry->subtitle),
                 subtitle_lead_color(view, entry->subtitle));
}

static void call_window_push(const Entry *entry, int32_t source);

static void list_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  ListView *view = context;
  if (list_shows_message(view)) {
    if (view->state == LIST_STATE_ERROR) {
      list_request_page(view, view->offset, 0);
    }
    return;
  }

  int32_t row = cell_index->row;
  if (list_has_previous(view)) {
    if (row == 0) {
      // Land on the last entry of the previous page, next to where we were.
      list_request_page(view, MAX(view->offset - PAGE_SIZE, 0), -1);
      return;
    }
    row--;
  }

  if (row >= view->received) {
    // Land on the first entry of the next page, just below "Previous".
    list_request_page(view, view->offset + view->received, 1);
    return;
  }

  const Entry *entry = &view->entries[row];
  if (view->list == LIST_LETTERS) {
    list_window_push(LIST_CONTACTS, entry->title);
  } else {
    call_window_push(entry, view->list);
  }
}

// Lists currently open, so a theme change from the phone can recolour them.
#define MAX_OPEN_LISTS 4
static ListView *s_open_lists[MAX_OPEN_LISTS];

static void list_apply_theme(ListView *view) {
  menu_layer_set_normal_colors(view->menu_layer, s_theme.background, s_theme.text);
  menu_layer_set_highlight_colors(view->menu_layer, s_theme.highlight, s_theme.highlight_text);
  layer_mark_dirty(menu_layer_get_layer(view->menu_layer));
}

static void list_window_load(Window *window) {
  ListView *view = window_get_user_data(window);
  for (int i = 0; i < MAX_OPEN_LISTS; i++) {
    if (!s_open_lists[i]) {
      s_open_lists[i] = view;
      break;
    }
  }
  Layer *root = window_get_root_layer(window);

  view->menu_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(view->menu_layer, view, (MenuLayerCallbacks) {
    .get_num_rows = list_get_num_rows,
    .get_cell_height = list_get_cell_height,
    .get_header_height = list_get_header_height,
    .draw_header = list_draw_header,
    .draw_row = list_draw_row,
    .select_click = list_select,
  });
  list_apply_theme(view);
  menu_layer_set_click_config_onto_window(view->menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(view->menu_layer));

  list_request_page(view, 0, 0);
}

static void list_window_unload(Window *window) {
  ListView *view = window_get_user_data(window);
  for (int i = 0; i < MAX_OPEN_LISTS; i++) {
    if (s_open_lists[i] == view) {
      s_open_lists[i] = NULL;
    }
  }
  if (s_waiting_list == view) {
    cancel_response_timer();
    s_waiting_list = NULL;
    abandon_request();
  }
  menu_layer_destroy(view->menu_layer);
  window_destroy(window);
  free(view);
}

static void list_window_push(int32_t list, const char *filter) {
  ListView *view = calloc(1, sizeof(ListView));
  if (!view) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Out of memory for list");
    return;
  }

  view->list = list;
  if (filter) {
    strncpy(view->filter, filter, sizeof(view->filter) - 1);
  }
  switch (list) {
    case LIST_RECENTS:
      strncpy(view->heading, "Recent calls", sizeof(view->heading) - 1);
      break;
    case LIST_FAVORITES:
      strncpy(view->heading, "Favorites", sizeof(view->heading) - 1);
      break;
    case LIST_LETTERS:
      strncpy(view->heading, "Contacts", sizeof(view->heading) - 1);
      break;
    default:
      snprintf(view->heading, sizeof(view->heading), "Contacts: %s", view->filter);
      break;
  }

  view->window = window_create();
  window_set_user_data(view->window, view);
  window_set_window_handlers(view->window, (WindowHandlers) {
    .load = list_window_load,
    .unload = list_window_unload,
  });
  window_stack_push(view->window, true);
}

// ---------------------------------------------------------------------------
// Touch (Pebble Time 2, Pebble Round 2). Menus use the system's touch
// navigation; the keypad and call screen read raw touch events and work out
// taps and swipes themselves, because tap recognizers never report a tap.

#if defined(_PBL_API_EXISTS_touch_service_subscribe)
#define APP_TOUCH 1
#endif

// A touch that moves less than this is a tap.
#define TAP_SLOP 10
// A rightward swipe at least this long goes back, like the Back button.
#define SWIPE_BACK_MIN 40
#define ABS(x) ((x) < 0 ? -(x) : (x))

typedef void (*TapHandler)(GPoint point);

#if defined(APP_TOUCH)
static Window *s_touch_window;
static TapHandler s_tap_handler;
static GPoint s_touch_start;
static bool s_touch_active;

static void touch_event(const TouchEvent *event, void *context) {
  const GPoint point = GPoint(event->x, event->y);
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_active = !event->non_navigational;
      s_touch_start = point;
      break;
    case TouchEvent_PositionUpdate:
      break;
    case TouchEvent_Liftoff: {
      if (!s_touch_active) {
        break;
      }
      s_touch_active = false;
      const int dx = point.x - s_touch_start.x;
      const int dy = point.y - s_touch_start.y;
      if (ABS(dx) <= TAP_SLOP && ABS(dy) <= TAP_SLOP) {
        if (s_tap_handler) {
          s_tap_handler(s_touch_start);
        }
      } else if (dx >= SWIPE_BACK_MIN && ABS(dy) < dx / 2) {
        window_stack_pop(true);
      }
      break;
    }
  }
}
#endif

static bool touch_available(void) {
#if defined(APP_TOUCH)
  return s_touch_enabled && touch_service_is_enabled();
#else
  return false;
#endif
}

// Menus and lists follow touch only while touch input is switched on.
static void apply_touch_navigation(void) {
#if defined(APP_TOUCH)
  app_touch_navigation_enable(s_touch_enabled);
#endif
}

// A brief tick when a key or button is tapped, if switched on in Settings.
static void touch_feedback(void) {
  if (!s_touch_vibe) {
    return;
  }
  static const uint32_t segments[] = { 30 };
  vibes_enqueue_custom_pattern((VibePattern) {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  });
}

// Sends taps on this window to the handler while it is on screen. The
// system's touch navigation is switched off for the window so a tap is not
// also turned into a button press.
static void touch_attach(Window *window, TapHandler handler) {
#if defined(APP_TOUCH)
  if (!s_touch_enabled) {
    return;
  }
  window_set_touch_bridge_disabled(window, true);
  if (!s_touch_window) {
    touch_service_subscribe(touch_event, NULL);
  }
  s_touch_window = window;
  s_tap_handler = handler;
  s_touch_active = false;
#endif
}

static void touch_detach(Window *window) {
#if defined(APP_TOUCH)
  // Windows can appear before the previous one disappears; only the window
  // that owns the subscription may end it.
  if (s_touch_window != window) {
    return;
  }
  touch_service_unsubscribe();
  s_touch_window = NULL;
  s_tap_handler = NULL;
#endif
}

// Phone handset centred on (0, 0) and 180 units across; scaled to fit a key.
static const GPoint HANDSET_POINTS[] = {
  {-54, -12}, {-38, 13}, {-14, 38}, {12, 54}, {34, 32}, {44, 29}, {62, 34}, {80, 35},
  {90, 45}, {90, 80}, {80, 90}, {47, 87}, {15, 77}, {-14, 61}, {-40, 40}, {-61, 14},
  {-77, -15}, {-87, -47}, {-90, -80}, {-80, -90}, {-45, -90}, {-35, -80}, {-29, -44},
  {-32, -34},
};

// Fills points and info with the handset scaled to size and makes a path.
static GPath *create_handset_path(GPoint *points, GPathInfo *info, int16_t size) {
  for (size_t i = 0; i < ARRAY_LENGTH(HANDSET_POINTS); i++) {
    points[i] = GPoint(HANDSET_POINTS[i].x * size / 180, HANDSET_POINTS[i].y * size / 180);
  }
  *info = (GPathInfo) {
    .num_points = ARRAY_LENGTH(HANDSET_POINTS),
    .points = points,
  };
  return gpath_create(info);
}

// ---------------------------------------------------------------------------
// Calling

static void exit_to_watchface(void *context) {
  s_exit_timer = NULL;
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  window_stack_pop_all(true);
}

static void call_set_status(const char *status, GColor color) {
  if (s_call_status_layer) {
    text_layer_set_text_color(s_call_status_layer, color);
    text_layer_set_text(s_call_status_layer, status);
  }
}

static void call_show_status(const char *status) {
  call_set_status(status, s_theme.hint);
}

// How long an error stays in the status line before the call status returns.
#define CALL_ERROR_SECONDS 3

static void call_show_error(const char *status) {
  call_set_status(status, s_theme.warning);
  s_call_status_until = time(NULL) + CALL_ERROR_SECONDS;
}

// During a call: "Calling", or "Connected 1:23" / "On hold 1:23" once the
// other side answers (the phone reports this when the Pebble is linked).
static void call_refresh_status(void) {
  if (s_call_state != CALL_ACTIVE || time(NULL) < s_call_status_until) {
    return;
  }
  if (!s_call_connected_at) {
    call_set_status("Calling", s_theme.call);
    return;
  }
  static char text[32];
  const int seconds = MAX(0, (int)(time(NULL) - s_call_connected_at));
  const char *label = s_call_on_hold ? "On hold" : "Connected";
  if (seconds >= 3600) {
    snprintf(text, sizeof(text), "%s %d:%02d:%02d", label,
             seconds / 3600, seconds / 60 % 60, seconds % 60);
  } else {
    snprintf(text, sizeof(text), "%s %d:%02d", label, seconds / 60, seconds % 60);
  }
  call_set_status(text, s_call_on_hold ? s_theme.warning : s_theme.call);
}

static void call_tick(struct tm *tick_time, TimeUnits units_changed) {
  call_refresh_status();
}

static void call_stop_ticking(void) {
  if (s_call_ticking) {
    tick_timer_service_unsubscribe();
    s_call_ticking = false;
  }
}

// What the call screen is showing. It starts as a confirmation, and once the
// phone has placed the call it becomes the in-call screen with an End button.
static bool call_button_shown(void) {
  return s_call_state == CALL_ACTIVE || s_call_state == CALL_ENDING ||
         (s_call_state == CALL_CONFIRM && touch_available());
}

static bool call_in_progress(void) {
  return s_call_state == CALL_ACTIVE || s_call_state == CALL_ENDING;
}

// Places the text and the buttons for the current state.
static void call_layout(void) {
  if (!s_call_window) {
    return;
  }
  const GRect bounds = layer_get_bounds(window_get_root_layer(s_call_window));
  const int16_t w = bounds.size.w;
  const int16_t h = bounds.size.h;
  const int16_t inset = PBL_IF_ROUND_ELSE(18, 6);
  const int16_t width = w - 2 * inset;
  const bool button = call_button_shown();
  const bool number = s_call_number[0] != '\0';
  Layer *status = text_layer_get_layer(s_call_status_layer);
  Layer *name = text_layer_get_layer(s_call_name_layer);
  Layer *number_line = text_layer_get_layer(s_call_number_layer);

  // The single button along the bottom: Call on the confirmation, End during
  // the call.
  layer_set_frame(s_call_button_layer,
                  PBL_IF_ROUND_ELSE(GRect(w * 26 / 100, h * 71 / 100, w * 48 / 100, 38),
                                    GRect(w / 5, h - 46, w * 3 / 5, 38)));

  if (call_in_progress()) {
    // Mute and Audio along the top, End (Down) at the bottom, and the call in
    // between: the status, the name and the number, centred. Round screens
    // place the top row a little lower, where the circle is wide enough.
    const int16_t row_h = 26;
    const int16_t row_w = PBL_IF_ROUND_ELSE(w * 70 / 100, w * 90 / 100);
    const int16_t row_x = (w - row_w) / 2;
    const int16_t row_y = PBL_IF_ROUND_ELSE(h * 14 / 100, 6);
    const int16_t gap = 6;
    const GRect audio = GRect(row_x + row_h + gap, row_y, row_w - row_h - gap, row_h);
    layer_set_frame(s_call_mute_layer, GRect(row_x, row_y, row_h, row_h));
    layer_set_frame(s_call_audio_layer, audio);
    layer_set_frame(status, GRect(inset, 0, width, 44));
    const int16_t status_h = MIN(44, text_layer_get_content_size(s_call_status_layer).h + 4);
    const int16_t top = audio.origin.y + audio.size.h + 2;
    const int16_t bottom = layer_get_frame(s_call_button_layer).origin.y;
    const int16_t block = status_h + 30 + (number ? 24 : 0);
    const int16_t y = top + MAX(0, (bottom - top - block) / 2);
    layer_set_frame(status, GRect(inset, y, width, status_h));
    layer_set_frame(name, GRect(inset, y + status_h, width, 30));
    layer_set_frame(number_line, GRect(inset, y + status_h + 28, width, 24));
  } else {
    // Leave room for the button below the name.
    const int16_t middle = h / 2 - (button ? 22 : 0);
    layer_set_frame(status, GRect(inset, middle - 50, width, 44));
    layer_set_frame(name, GRect(inset, middle - 4, width, number ? 30 : button ? 56 : 72));
    layer_set_frame(number_line, GRect(inset, middle + 26, width, 24));
  }
  layer_set_hidden(number_line, !number);
  layer_set_hidden(s_call_button_layer, !button);
  layer_set_hidden(s_call_audio_layer, !call_in_progress());
  layer_set_hidden(s_call_mute_layer, !call_in_progress());
  layer_mark_dirty(s_call_button_layer);
  layer_mark_dirty(s_call_audio_layer);
  layer_mark_dirty(s_call_mute_layer);
}

static void call_ended(void) {
  call_stop_ticking();
  s_call_state = CALL_ENDED;
  call_show_status("Call ended");
  call_layout();
  vibes_short_pulse();
  cancel_exit_timer();
  s_exit_timer = app_timer_register(EXIT_DELAY_MS, exit_to_watchface, NULL);
}

// A call or hang-up request failed or went unanswered.
static void call_request_failed(const char *message) {
  if (s_call_state == CALL_ENDING) {
    // The call may still be going, so End stays available to try again.
    s_call_state = CALL_ACTIVE;
  } else if (s_call_state == CALL_SENDING) {
    // Never retry a call automatically: it may already be dialing.
    s_call_state = CALL_BLOCKED;
  }
  call_show_error(message);
  call_layout();
}

static void call_handle_reply(int32_t result, DictionaryIterator *iterator) {
  if (result != RESULT_OK) {
    fail_request(result_message(result));
    return;
  }
  cancel_response_timer();
  s_waiting_call = false;

  if (s_call_state == CALL_ENDING) {
    call_ended();
    return;
  }

  // The phone has placed the call: show who is being called and offer End.
  // A number typed on the keypad may belong to a contact: show their name in
  // large text, like a call from the lists, with the number beneath it.
  const Tuple *contact = dict_find(iterator, MESSAGE_KEY_CALL_NAME);
  const bool named = s_call_source == -1 && contact && contact->type == TUPLE_CSTRING;
  if (named) {
    strncpy(s_call_name, contact->value->cstring, sizeof(s_call_name) - 1);
    s_call_name[sizeof(s_call_name) - 1] = '\0';
    text_layer_set_font(s_call_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(s_call_name_layer, s_call_name);
  }
  const Tuple *number = dict_find(iterator, MESSAGE_KEY_NUMBER);
  if (number && number->type == TUPLE_CSTRING && (s_call_source != -1 || named)) {
    strncpy(s_call_number, number->value->cstring, sizeof(s_call_number) - 1);
    s_call_number[sizeof(s_call_number) - 1] = '\0';
  }
  const Tuple *version = dict_find(iterator, MESSAGE_KEY_COMPANION_VERSION);
  s_call_companion_version = version ? version->value->int32 : 0;
  s_call_state = CALL_ACTIVE;
  call_refresh_status();
  call_layout();
  vibes_short_pulse();
}

// The phone reports that the call has ended (the other side hung up, or it
// was ended on the phone).
// Call progress from the phone: calling, connected or on hold, and how long
// it has been connected. Counting from now minus those seconds keeps the
// timer right even if the watch's and phone's clocks differ.
enum {
  CALL_STATUS_CALLING = 1,
  CALL_STATUS_CONNECTED = 2,
  CALL_STATUS_ON_HOLD = 3,
};

static void call_status_changed(int32_t status, int32_t seconds) {
  if (!s_call_window || !call_in_progress()) {
    return;
  }
  if (status == CALL_STATUS_CONNECTED || status == CALL_STATUS_ON_HOLD) {
    s_call_connected_at = time(NULL) - MAX(0, seconds);
    s_call_on_hold = status == CALL_STATUS_ON_HOLD;
    if (!s_call_ticking) {
      tick_timer_service_subscribe(SECOND_UNIT, call_tick);
      s_call_ticking = true;
    }
  } else {
    s_call_connected_at = 0;
    s_call_on_hold = false;
    call_stop_ticking();
  }
  call_refresh_status();
}

static void call_phone_ended(void) {
  if (!s_call_window || (s_call_state != CALL_ACTIVE && s_call_state != CALL_ENDING)) {
    return;
  }
  if (s_waiting_call) {
    cancel_response_timer();
    s_waiting_call = false;
  }
  call_ended();
}

// The button below the name: green Call on the confirmation (touch watches),
// red End during a call.
// Room kept at the right of a button for its hint, so text never runs into it.
#define BUTTON_HINT_WIDTH 20

// A small triangle at the right of a button, pointing at the watch button
// (Up or Down) that presses it. Touch watches do not need the hint.
static void draw_button_hint(GContext *ctx, GRect bounds, bool up, GColor color) {
  if (touch_available()) {
    return;
  }
  const int16_t x = bounds.size.w - 12;
  const int16_t middle = bounds.size.h / 2;
  graphics_context_set_fill_color(ctx, color);
  for (int i = 0; i < 5; i++) {
    const int16_t y = up ? middle - 2 + i : middle + 2 - i;
    graphics_fill_rect(ctx, GRect(x - i, y, 2 * i + 1, 1), 0, GCornerNone);
  }
}

static const char *audio_route_name(int32_t route) {
  switch (route) {
    case AUDIO_SPEAKER: return "Speaker";
    case AUDIO_BLUETOOTH: return "Bluetooth";
    case AUDIO_HEADSET: return "Headset";
    case AUDIO_EARPIECE: return "Phone";
    default: return "Audio";
  }
}

// The audio button at the top of the call screen: the current output.
static void call_audio_draw(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_theme.key);
  graphics_fill_rect(ctx, bounds, bounds.size.h / 2, GCornersAll);
  if (PBL_IF_BW_ELSE(true, false)) {
    graphics_context_set_stroke_color(ctx, s_theme.text);
    graphics_draw_round_rect(ctx, bounds, bounds.size.h / 2);
  }
  // Centre the label in the space left of the hint.
  const int16_t hint = touch_available() ? 0 : BUTTON_HINT_WIDTH;
  graphics_context_set_text_color(ctx, s_theme.key_text);
  graphics_draw_text(ctx, audio_route_name(s_audio_routes ? s_audio_route : 0),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, bounds.size.h / 2 - 13, bounds.size.w - hint, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  draw_button_hint(ctx, bounds, true, s_theme.key_text);
}

// The mute button: a microphone, crossed out and red-orange while muted.
static void call_mute_draw(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const GColor fill = s_audio_muted ? PBL_IF_COLOR_ELSE(s_theme.delete_key, s_theme.text)
                                    : s_theme.key;
  const GColor ink = s_audio_muted ? PBL_IF_COLOR_ELSE(s_theme.function_key_text, s_theme.background)
                                   : s_theme.key_text;
  const GPoint c = grect_center_point(&bounds);
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_circle(ctx, c, bounds.size.w / 2 - 1);
  if (PBL_IF_BW_ELSE(!s_audio_muted, false)) {
    graphics_context_set_stroke_color(ctx, s_theme.text);
    graphics_draw_circle(ctx, c, bounds.size.w / 2 - 1);
  }

  // Microphone: a capsule in a cradle, on a stand.
  const int16_t u = bounds.size.w / 12;
  graphics_context_set_fill_color(ctx, ink);
  graphics_context_set_stroke_color(ctx, ink);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_fill_rect(ctx, GRect(c.x - u - 1, c.y - 4 * u, 2 * u + 3, 5 * u), u + 1, GCornersAll);
  graphics_draw_arc(ctx, GRect(c.x - 2 * u - 2, c.y - 3 * u, 4 * u + 5, 5 * u),
                    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
  graphics_draw_line(ctx, GPoint(c.x, c.y + 2 * u), GPoint(c.x, c.y + 3 * u + 1));
  if (s_audio_muted) {
    // The slash gets an outline in the button's colour so it stays clear of
    // the microphone it crosses.
    const GPoint from = GPoint(c.x - 3 * u, c.y - 4 * u);
    const GPoint to = GPoint(c.x + 3 * u, c.y + 3 * u);
    graphics_context_set_stroke_color(ctx, fill);
    graphics_context_set_stroke_width(ctx, 5);
    graphics_draw_line(ctx, from, to);
    graphics_context_set_stroke_color(ctx, ink);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, from, to);
  }
}

static void call_button_draw(Layer *layer, GContext *ctx) {
  const bool end = s_call_state != CALL_CONFIRM;
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, end ? PBL_IF_COLOR_ELSE(s_theme.delete_key, s_theme.call_key)
                                           : s_theme.call_key);
  graphics_fill_rect(ctx, bounds, bounds.size.h / 2, GCornersAll);

  const char *label = end ? "End" : "Call";
  const int16_t icon = 18;
  const int16_t gap = 6;
  const int16_t hint = end && !touch_available() ? BUTTON_HINT_WIDTH : 0;
  // Next to Mute the button is narrower; use the smaller font if needed.
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GSize text = graphics_text_layout_get_content_size(
    label, font, bounds, GTextOverflowModeFill, GTextAlignmentLeft);
  if (icon + gap + text.w + hint + 8 > bounds.size.w) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    text = graphics_text_layout_get_content_size(
      label, font, bounds, GTextOverflowModeFill, GTextAlignmentLeft);
  }
  const int16_t left = (bounds.size.w - hint - icon - gap - text.w) / 2;
  const int16_t middle = bounds.size.h / 2;
  GPath *path = end ? s_hangup_icon_path : s_call_icon_path;
  graphics_context_set_fill_color(ctx, s_theme.function_key_text);
  gpath_move_to(path, GPoint(left + icon / 2, middle));
  gpath_draw_filled(ctx, path);
  graphics_context_set_text_color(ctx, s_theme.function_key_text);
  graphics_draw_text(ctx, label, font,
                     GRect(left + icon + gap, middle - text.h / 2 - text.h / 5, text.w + 4, 30),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (end) {
    draw_button_hint(ctx, bounds, false, s_theme.function_key_text);
  }
}

static void call_submit(ClickRecognizerRef recognizer, void *context);
static void call_hang_up(ClickRecognizerRef recognizer, void *context);
static void call_switch_audio(ClickRecognizerRef recognizer, void *context);
static void call_toggle_mute(ClickRecognizerRef recognizer, void *context);

static void call_tap(GPoint point) {
  if (call_in_progress()) {
    const GRect audio = grect_inset(layer_get_frame(s_call_audio_layer), GEdgeInsets(-6));
    if (grect_contains_point(&audio, &point)) {
      touch_feedback();
      call_switch_audio(NULL, NULL);
      return;
    }
    const GRect mute = grect_inset(layer_get_frame(s_call_mute_layer), GEdgeInsets(-4));
    if (grect_contains_point(&mute, &point)) {
      touch_feedback();
      call_toggle_mute(NULL, NULL);
      return;
    }
  }
  if (!call_button_shown()) {
    return;
  }
  const GRect target = grect_inset(layer_get_frame(s_call_button_layer), GEdgeInsets(-6));
  if (!grect_contains_point(&target, &point)) {
    return;
  }
  if (s_call_state == CALL_CONFIRM) {
    touch_feedback();
    call_submit(NULL, NULL);
  } else if (s_call_state == CALL_ACTIVE) {
    touch_feedback();
    call_hang_up(NULL, NULL);
  }
}

static TextLayer *call_text_layer(Layer *root, const char *font, GColor color) {
  TextLayer *layer = text_layer_create(GRect(0, 0, 0, 0));
  text_layer_set_font(layer, fonts_get_system_font(font));
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, color);
  layer_add_child(root, text_layer_get_layer(layer));
  return layer;
}

static void call_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, s_theme.background);

  s_call_status_layer = call_text_layer(root, FONT_KEY_GOTHIC_18, s_theme.hint);
  s_call_name_layer = call_text_layer(root, s_call_source == -1 ? FONT_KEY_GOTHIC_18_BOLD
                                                                : FONT_KEY_GOTHIC_24_BOLD, s_theme.text);
  text_layer_set_text(s_call_name_layer, s_call_name);
  s_call_number_layer = call_text_layer(root, FONT_KEY_GOTHIC_18, s_theme.hint);
  text_layer_set_text(s_call_number_layer, s_call_number);

  const int16_t w = bounds.size.w;
  const int16_t h = bounds.size.h;
  const GRect button = PBL_IF_ROUND_ELSE(GRect(w * 26 / 100, h * 71 / 100, w * 48 / 100, 38),
                                         GRect(w / 5, h - 46, w * 3 / 5, 38));
  s_call_icon_path = create_handset_path(s_call_icon_points, &s_call_icon_info, 18);
  // The same handset turned on its side is the usual "hang up" symbol.
  s_hangup_icon_path = create_handset_path(s_hangup_icon_points, &s_hangup_icon_info, 18);
  gpath_rotate_to(s_hangup_icon_path, TRIG_MAX_ANGLE * 3 / 8);
  s_call_button_layer = layer_create(button);
  layer_set_update_proc(s_call_button_layer, call_button_draw);
  layer_add_child(root, s_call_button_layer);
  s_call_audio_layer = layer_create(GRect(0, 0, 0, 0));
  layer_set_update_proc(s_call_audio_layer, call_audio_draw);
  layer_add_child(root, s_call_audio_layer);
  s_call_mute_layer = layer_create(GRect(0, 0, 0, 0));
  layer_set_update_proc(s_call_mute_layer, call_mute_draw);
  layer_add_child(root, s_call_mute_layer);

  call_layout();
}

static void call_window_appear(Window *window) {
  touch_attach(window, call_tap);
}

static void call_window_disappear(Window *window) {
  touch_detach(window);
}

static void call_window_unload(Window *window) {
  if (s_waiting_call) {
    cancel_response_timer();
    s_waiting_call = false;
    abandon_request();
  }
  cancel_exit_timer();
  call_stop_ticking();
  text_layer_destroy(s_call_status_layer);
  text_layer_destroy(s_call_name_layer);
  text_layer_destroy(s_call_number_layer);
  layer_destroy(s_call_button_layer);
  layer_destroy(s_call_audio_layer);
  layer_destroy(s_call_mute_layer);
  s_call_audio_layer = NULL;
  s_call_mute_layer = NULL;
  gpath_destroy(s_call_icon_path);
  gpath_destroy(s_hangup_icon_path);
  s_call_status_layer = NULL;
  s_call_name_layer = NULL;
  s_call_number_layer = NULL;
  s_call_button_layer = NULL;
  s_call_icon_path = NULL;
  s_hangup_icon_path = NULL;
  window_destroy(window);
  s_call_window = NULL;
}

static void call_submit(ClickRecognizerRef recognizer, void *context) {
  if (s_call_state != CALL_CONFIRM) {
    return;
  }
  const char *error = NULL;
  DictionaryIterator *iterator = begin_request(s_call_source == -1 ? REQUEST_DIAL : REQUEST_CALL, &error);
  if (!iterator) {
    call_show_error(error);
    return;
  }
  if (s_call_source == -1) {
    dict_write_cstring(iterator, MESSAGE_KEY_NUMBER, s_call_name);
  } else {
    dict_write_cstring(iterator, MESSAGE_KEY_ITEM_ID, s_call_entry.id);
    dict_write_int32(iterator, MESSAGE_KEY_LIST, s_call_source);
  }
  if (!send_request(&error)) {
    call_show_error(error);
    return;
  }
  s_call_state = CALL_SENDING;
  s_waiting_call = true;
  call_show_status("Sending call...");
  call_layout();
}

// Down, or a tap on End: asks the phone to end the call.
static void call_hang_up(ClickRecognizerRef recognizer, void *context) {
  if (s_call_state != CALL_ACTIVE) {
    return;
  }
  if (s_call_companion_version < COMPANION_CAN_HANG_UP) {
    // An older phone app would never answer; say so now instead of timing out.
    call_show_error("Update Phone Dialer on your phone");
    vibes_short_pulse();
    return;
  }
  const char *error = NULL;
  DictionaryIterator *iterator = begin_request(REQUEST_HANGUP, &error);
  if (!iterator || !send_request(&error)) {
    call_show_error(error);
    return;
  }
  s_call_state = CALL_ENDING;
  s_waiting_call = true;
  call_show_status("Ending call...");
  layer_mark_dirty(s_call_button_layer);
}

// The next output in the order Phone (or Headset), Speaker, Bluetooth,
// skipping any the phone does not have right now.
static int32_t next_audio_route(void) {
  const int32_t handset = (s_audio_routes & AUDIO_HEADSET) ? AUDIO_HEADSET : AUDIO_EARPIECE;
  const int32_t order[] = { handset, AUDIO_SPEAKER, AUDIO_BLUETOOTH };
  const int count = ARRAY_LENGTH(order);
  int current = 0;
  for (int i = 0; i < count; i++) {
    if (order[i] == s_audio_route ||
        (order[i] == handset && (s_audio_route == AUDIO_EARPIECE || s_audio_route == AUDIO_HEADSET))) {
      current = i;
    }
  }
  for (int step = 1; step <= count; step++) {
    const int32_t route = order[(current + step) % count];
    if (s_audio_routes & route) {
      return route;
    }
  }
  return AUDIO_SPEAKER;
}

// Up, or a tap on the audio button: moves the call to the next output. The
// phone reports the new output back, which updates the button.
static void call_switch_audio(ClickRecognizerRef recognizer, void *context) {
  if (s_call_state != CALL_ACTIVE) {
    return;
  }
  if (s_call_companion_version < COMPANION_CAN_SWITCH_AUDIO) {
    call_show_error("Update Phone Dialer on your phone");
    call_layout();
    vibes_short_pulse();
    return;
  }
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    vibes_short_pulse();
    return;
  }
  dict_write_int32(iterator, MESSAGE_KEY_REQUEST, REQUEST_AUDIO);
  dict_write_int32(iterator, MESSAGE_KEY_AUDIO_ROUTE,
                   s_audio_routes ? next_audio_route() : AUDIO_SPEAKER);
  app_message_outbox_send();
}

// Select, or a tap on the microphone at the top: mutes or unmutes the call. The phone
// reports the new state back, which updates the button.
static void call_toggle_mute(ClickRecognizerRef recognizer, void *context) {
  if (s_call_state != CALL_ACTIVE) {
    return;
  }
  if (s_call_companion_version < COMPANION_CAN_MUTE) {
    call_show_error("Update Phone Dialer on your phone");
    call_layout();
    vibes_short_pulse();
    return;
  }
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    vibes_short_pulse();
    return;
  }
  dict_write_int32(iterator, MESSAGE_KEY_REQUEST, REQUEST_MUTE);
  dict_write_int32(iterator, MESSAGE_KEY_AUDIO_MUTED, !s_audio_muted);
  app_message_outbox_send();
}

// Why the phone has no audio outputs to offer, in answer to Up or Select.
enum {
  AUDIO_NOT_LINKED = 1,
  AUDIO_NEEDS_ANDROID_12 = 2,
};

// The phone reports the call's audio output, when the call starts, when it
// changes, and in answer to Up. No outputs means Android does not let the
// phone app switch them: the watch is not linked to it as a companion watch,
// or the phone is older than Android 12.
static void call_audio_changed(int32_t route, int32_t routes, bool muted, int32_t asked) {
  if (!s_call_window || !call_in_progress()) {
    return;
  }
  s_audio_route = route;
  s_audio_routes = routes;
  s_audio_muted = routes && muted;
  if (!routes && asked) {
    call_show_error(asked == AUDIO_NEEDS_ANDROID_12 ? "Needs Android 12 on phone"
                                                    : "Link Pebble in phone app");
    call_layout();
    vibes_short_pulse();
  } else if (s_call_state == CALL_ACTIVE) {
    call_refresh_status();
    call_layout();
  }
  layer_mark_dirty(s_call_audio_layer);
  layer_mark_dirty(s_call_mute_layer);
}

// Select places the call on the confirmation, and mutes during the call.
static void call_select(ClickRecognizerRef recognizer, void *context) {
  if (call_in_progress()) {
    call_toggle_mute(recognizer, context);
  } else {
    call_submit(recognizer, context);
  }
}

static void call_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, call_switch_audio);
  window_single_click_subscribe(BUTTON_ID_SELECT, call_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, call_hang_up);
}

static void call_window_push(const Entry *entry, int32_t source) {
  s_call_entry = *entry;
  s_call_source = source;
  s_call_state = CALL_CONFIRM;
  s_call_companion_version = 0;
  s_audio_route = 0;
  s_audio_routes = 0;
  s_audio_muted = false;
  s_call_connected_at = 0;
  s_call_on_hold = false;
  s_call_status_until = 0;
  s_call_number[0] = '\0';
  strncpy(s_call_name, entry->title, sizeof(s_call_name) - 1);
  s_call_name[sizeof(s_call_name) - 1] = '\0';

  s_call_window = window_create();
  window_set_window_handlers(s_call_window, (WindowHandlers) {
    .load = call_window_load,
    .appear = call_window_appear,
    .disappear = call_window_disappear,
    .unload = call_window_unload,
  });
  window_set_click_config_provider(s_call_window, call_click_config);
  window_stack_push(s_call_window, true);
  if (source != -1 && (!entry->id[0] || strcmp(entry->id, "0") == 0)) {
    s_call_state = CALL_BLOCKED;
    call_show_error("Number unavailable");
    call_layout();
  } else {
    call_show_status(touch_available() ? "Tap Call or press Select" : "Select to call");
  }
}

// ---------------------------------------------------------------------------
// Number keypad. Up/Down move the highlight, Select presses the highlighted
// key, and on touch watches every key can also be tapped directly.

#define PRESS_FLASH_MS 150

#if defined(PBL_ROUND)
static int16_t isqrt(int32_t value) {
  int32_t root = 0;
  while ((root + 1) * (root + 1) <= value) {
    root++;
  }
  return root;
}
#endif

static void dial_layout(GRect bounds) {
  const int16_t w = bounds.size.w;
  const int16_t h = bounds.size.h;
#if defined(PBL_ROUND)
  // Each row is as wide as the circle allows at its outer edge, so the
  // middle rows get the widest keys and nothing is clipped.
  const int16_t gap = 3;
  const int16_t radius = w / 2;
  const int16_t grid_top = h * 27 / 100;
  const int16_t grid_bottom = h * 90 / 100;
  const int16_t key_h = (grid_bottom - grid_top - 4 * gap) / 5;
  for (int row = 0; row < 5; row++) {
    const int16_t top = grid_top + row * (key_h + gap);
    const int16_t far = MAX(h / 2 - top, top + key_h - h / 2);
    const int16_t row_w = MIN(2 * (isqrt(radius * radius - far * far) - gap), w * 78 / 100);
    const int16_t key_w = (row_w - 2 * gap) / 3;
    const int16_t left = (w - (3 * key_w + 2 * gap)) / 2;
    for (int col = 0; col < 3; col++) {
      s_key_rects[row * 3 + col] = GRect(left + col * (key_w + gap), top, key_w, key_h);
    }
  }
  const int16_t display_w = w * 64 / 100;
  s_display_rect = GRect((w - display_w) / 2, h * 9 / 100, display_w,
                         grid_top - h * 9 / 100 - gap);
#else
  const int16_t gap = w >= 180 ? 4 : 3;
  const int16_t grid_w = w - 2 * gap;
  const int16_t grid_top = h * 20 / 100;
  const int16_t key_w = (grid_w - 2 * gap) / 3;
  const int16_t key_h = (h - gap - grid_top - 4 * gap) / 5;
  for (int i = 0; i < KEY_COUNT; i++) {
    s_key_rects[i] = GRect(gap + (i % 3) * (key_w + gap),
                           grid_top + (i / 3) * (key_h + gap), key_w, key_h);
  }
  s_display_rect = GRect(gap, 0, grid_w, grid_top - gap);
#endif

  s_key_font = fonts_get_system_font(key_h >= 30 ? FONT_KEY_GOTHIC_28_BOLD
                                     : key_h >= 24 ? FONT_KEY_GOTHIC_24_BOLD
                                     : FONT_KEY_GOTHIC_18_BOLD);
  s_number_font = fonts_get_system_font(s_display_rect.size.h >= 36 ? FONT_KEY_GOTHIC_28_BOLD
                                        : FONT_KEY_GOTHIC_24_BOLD);
  s_number_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_icon_size = key_h * 7 / 10;
}

static GPoint rect_center(GRect rect) {
  return GPoint(rect.origin.x + rect.size.w / 2, rect.origin.y + rect.size.h / 2);
}

// Backspace: a tag pointing left with a cross in it.
static void draw_delete_icon(GContext *ctx, GPoint center, int16_t size) {
  const int16_t half_w = size * 6 / 10;
  const int16_t half_h = size * 4 / 10;
  const int16_t notch = half_h;
  const GPoint left = GPoint(center.x - half_w, center.y);
  const GPoint top_left = GPoint(center.x - half_w + notch, center.y - half_h);
  const GPoint top_right = GPoint(center.x + half_w, center.y - half_h);
  const GPoint bottom_right = GPoint(center.x + half_w, center.y + half_h);
  const GPoint bottom_left = GPoint(center.x - half_w + notch, center.y + half_h);
  graphics_draw_line(ctx, left, top_left);
  graphics_draw_line(ctx, top_left, top_right);
  graphics_draw_line(ctx, top_right, bottom_right);
  graphics_draw_line(ctx, bottom_right, bottom_left);
  graphics_draw_line(ctx, bottom_left, left);

  const int16_t cross = half_h / 2;
  const int16_t cross_x = center.x + notch / 2;
  graphics_draw_line(ctx, GPoint(cross_x - cross, center.y - cross),
                     GPoint(cross_x + cross, center.y + cross));
  graphics_draw_line(ctx, GPoint(cross_x + cross, center.y - cross),
                     GPoint(cross_x - cross, center.y + cross));
}

static void draw_key(GContext *ctx, int key) {
  const GRect rect = s_key_rects[key];
  const bool pressed = key == s_pressed_key;
  const bool focused = s_show_focus && key == s_key;
  const uint16_t radius = rect.size.h / 4;

#if defined(PBL_COLOR)
  GColor fill;
  GColor ink = s_theme.function_key_text;
  if (key == KEY_CALL) {
    fill = pressed ? s_theme.call_key_pressed : s_theme.call_key;
  } else if (key == KEY_DELETE) {
    fill = pressed ? s_theme.delete_key_pressed : s_theme.delete_key;
  } else {
    fill = pressed ? s_theme.key_pressed : s_theme.key;
    ink = pressed ? s_theme.key_pressed_text : s_theme.key_text;
  }

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, radius, GCornersAll);
  if (focused) {
    graphics_context_set_stroke_color(ctx, s_theme.key_ring);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, grect_inset(rect, GEdgeInsets(1)), radius);
  }
#else
  // The Call key is solid and inverts to an outline; the others do the opposite.
  const bool inverted = (focused || pressed) != (key == KEY_CALL);
  const GColor fill = inverted ? s_theme.text : s_theme.background;
  const GColor ink = inverted ? s_theme.background : s_theme.text;

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, radius, GCornersAll);
  graphics_context_set_stroke_color(ctx, s_theme.text);
  // A heavier outline keeps the highlight visible on the Call key as well.
  graphics_context_set_stroke_width(ctx, focused ? 3 : 1);
  graphics_draw_round_rect(ctx, focused ? grect_inset(rect, GEdgeInsets(1)) : rect, radius);
#endif

  const GPoint center = rect_center(rect);
  if (key == KEY_CALL) {
    graphics_context_set_fill_color(ctx, ink);
    gpath_move_to(s_handset_path, center);
    gpath_draw_filled(ctx, s_handset_path);
  } else if (key == KEY_DELETE) {
    graphics_context_set_stroke_color(ctx, ink);
    graphics_context_set_stroke_width(ctx, 2);
    draw_delete_icon(ctx, center, s_icon_size);
  } else {
    // Gothic glyphs sit low in their line box, so lift the text to centre it.
    const GSize text = graphics_text_layout_get_content_size(
      s_keys[key], s_key_font, rect, GTextOverflowModeFill, GTextAlignmentCenter);
    const int16_t lift = text.h / 4;
    graphics_context_set_text_color(ctx, ink);
    graphics_draw_text(ctx, s_keys[key], s_key_font,
                       GRect(rect.origin.x, center.y - text.h / 2 - lift, rect.size.w, text.h + lift),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void draw_number(GContext *ctx) {
  const GRect area = s_display_rect;
  if (!s_number[0]) {
    graphics_context_set_text_color(ctx, s_theme.hint);
    const GSize text = graphics_text_layout_get_content_size(
      "Enter number", s_number_small_font, area, GTextOverflowModeFill, GTextAlignmentCenter);
    graphics_draw_text(ctx, "Enter number", s_number_small_font,
                       GRect(area.origin.x, area.origin.y + (area.size.h - text.h) / 2 - 3,
                             area.size.w, text.h + 4),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    return;
  }

  // Use the large font while the number fits; then the small one; then show
  // the end of the number, which is the part being typed.
  GFont font = s_number_font;
  const char *text = s_number;
  static char tail[NUMBER_SIZE + 4];
  const GRect wide = GRect(0, 0, 1000, area.size.h);
  if (graphics_text_layout_get_content_size(text, font, wide, GTextOverflowModeFill,
                                            GTextAlignmentLeft).w > area.size.w) {
    font = s_number_small_font;
    for (size_t start = 0; s_number[start]; start++) {
      snprintf(tail, sizeof(tail), start ? "...%s" : "%s", s_number + start);
      if (graphics_text_layout_get_content_size(tail, font, wide, GTextOverflowModeFill,
                                                GTextAlignmentLeft).w <= area.size.w) {
        break;
      }
    }
    text = tail;
  }

  const GSize size = graphics_text_layout_get_content_size(
    text, font, area, GTextOverflowModeFill, GTextAlignmentCenter);
  graphics_context_set_text_color(ctx, s_theme.text);
  graphics_draw_text(ctx, text, font,
                     GRect(area.origin.x, area.origin.y + (area.size.h - size.h) / 2 - size.h / 5,
                           area.size.w, size.h + size.h / 5),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void dial_draw(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, s_theme.background);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  draw_number(ctx);
  graphics_context_set_stroke_color(ctx, s_theme.divider);
  graphics_context_set_stroke_width(ctx, 1);
  const int16_t divider_y = s_display_rect.origin.y + s_display_rect.size.h;
  graphics_draw_line(ctx, GPoint(s_display_rect.origin.x + 8, divider_y),
                     GPoint(s_display_rect.origin.x + s_display_rect.size.w - 9, divider_y));

  for (int i = 0; i < KEY_COUNT; i++) {
    draw_key(ctx, i);
  }
}

static void dial_press(int key) {
  size_t length = strlen(s_number);
  if (key == KEY_DELETE) {
    if (length) s_number[length - 1] = '\0';
  } else if (key == KEY_CALL) {
    bool has_digit = false;
    for (size_t i = 0; i < length; i++) {
      if (s_number[i] >= '0' && s_number[i] <= '9') has_digit = true;
    }
    if (has_digit) {
      Entry entry = {0};
      strncpy(entry.title, s_number, sizeof(entry.title) - 1);
      call_window_push(&entry, -1);
    } else {
      vibes_short_pulse();
    }
  } else if (length < NUMBER_SIZE - 1 && (key != KEY_PLUS || length == 0)) {
    s_number[length] = s_keys[key][0];
    s_number[length + 1] = '\0';
  } else {
    vibes_short_pulse();
  }
  layer_mark_dirty(s_dial_layer);
}

static void dial_up(ClickRecognizerRef recognizer, void *context) {
  if (s_show_focus) {
    s_key = (s_key + KEY_COUNT - 1) % KEY_COUNT;
  }
  s_show_focus = true;
  layer_mark_dirty(s_dial_layer);
}

static void dial_down(ClickRecognizerRef recognizer, void *context) {
  if (s_show_focus) {
    s_key = (s_key + 1) % KEY_COUNT;
  }
  s_show_focus = true;
  layer_mark_dirty(s_dial_layer);
}

static void dial_select(ClickRecognizerRef recognizer, void *context) {
  if (!s_show_focus) {
    // After using touch, the first press only shows where the highlight is.
    s_show_focus = true;
    layer_mark_dirty(s_dial_layer);
    return;
  }
  dial_press(s_key);
}

static void dial_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, dial_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, dial_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, dial_select);
}

static void dial_clear_press(void *context) {
  s_press_timer = NULL;
  s_pressed_key = -1;
  if (s_dial_layer) {
    layer_mark_dirty(s_dial_layer);
  }
}

// The key under a point, counting the gaps between keys so there are no dead spots.
static int dial_key_at(GPoint point) {
  for (int i = 0; i < KEY_COUNT; i++) {
    const GRect target = grect_inset(s_key_rects[i], GEdgeInsets(-2));
    if (grect_contains_point(&target, &point)) {
      return i;
    }
  }
  return -1;
}

static void dial_tap(GPoint point) {
  const int key = dial_key_at(point);
  if (key < 0) {
    return;
  }

  touch_feedback();
  s_key = key;
  s_show_focus = false;
  s_pressed_key = key;
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
  }
  s_press_timer = app_timer_register(PRESS_FLASH_MS, dial_clear_press, NULL);
  dial_press(key);
}

static void dial_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, s_theme.background);
  dial_layout(bounds);
  s_handset_path = create_handset_path(s_handset_points, &s_handset_info, s_icon_size);

  s_dial_layer = layer_create(bounds);
  layer_set_update_proc(s_dial_layer, dial_draw);
  layer_add_child(root, s_dial_layer);

  // With touch, the button highlight stays hidden until a button is used.
  s_show_focus = !touch_available();
}

static void dial_window_appear(Window *window) {
  touch_attach(window, dial_tap);
}

static void dial_window_disappear(Window *window) {
  touch_detach(window);
}

static void dial_window_unload(Window *window) {
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
    s_press_timer = NULL;
  }
  s_pressed_key = -1;
  layer_destroy(s_dial_layer);
  s_dial_layer = NULL;
  gpath_destroy(s_handset_path);
  s_handset_path = NULL;
  window_destroy(window);
  s_dial_window = NULL;
}

static void dial_window_push(void) {
  s_dial_window = window_create();
  window_set_click_config_provider(s_dial_window, dial_click_config);
  window_set_window_handlers(s_dial_window, (WindowHandlers) {
    .load = dial_window_load,
    .appear = dial_window_appear,
    .disappear = dial_window_disappear,
    .unload = dial_window_unload,
  });
  window_stack_push(s_dial_window, true);
}

// ---------------------------------------------------------------------------
// AppMessage

static void inbox_received(DictionaryIterator *iterator, void *context) {
  // Settings from the Settings page can arrive at any time.
  if (apply_settings(iterator)) {
    return;
  }
  // So can the phone's notice that the call we placed has ended, and how it
  // is going until then.
  if (dict_find(iterator, MESSAGE_KEY_CALL_STATE)) {
    call_phone_ended();
    return;
  }
  const Tuple *call_status = dict_find(iterator, MESSAGE_KEY_CALL_STATUS);
  if (call_status) {
    const Tuple *seconds = dict_find(iterator, MESSAGE_KEY_CALL_SECONDS);
    call_status_changed(call_status->value->int32, seconds ? seconds->value->int32 : 0);
    return;
  }
  // And audio output updates during that call.
  const Tuple *routes = dict_find(iterator, MESSAGE_KEY_AUDIO_ROUTES);
  if (routes) {
    const Tuple *route = dict_find(iterator, MESSAGE_KEY_AUDIO_ROUTE);
    const Tuple *asked = dict_find(iterator, MESSAGE_KEY_AUDIO_ASKED);
    const Tuple *muted = dict_find(iterator, MESSAGE_KEY_AUDIO_MUTED);
    call_audio_changed(route ? route->value->int32 : 0, routes->value->int32,
                       muted && muted->value->int32, asked ? asked->value->int32 : 0);
    return;
  }

  const Tuple *token_tuple = dict_find(iterator, MESSAGE_KEY_TOKEN);
  if (!token_tuple || token_tuple->value->int32 != s_token) {
    return;
  }

  const Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT);
  const int32_t result = result_tuple ? result_tuple->value->int32 : RESULT_FAILED;

  if (s_waiting_call) {
    call_handle_reply(result, iterator);
  } else if (s_waiting_list) {
    list_handle_reply(s_waiting_list, iterator, result);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  // A dropped page would leave a gap, so fail and let the user retry.
  if (s_waiting_list || s_waiting_call) {
    fail_request("Phone reply lost");
  }
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  if (s_waiting_list || s_waiting_call) {
    fail_request("Phone unavailable");
  }
}

// ---------------------------------------------------------------------------
// Main menu

static const char *main_row_title(MainRow row) {
  switch (row) {
    case MAIN_ROW_DIALER: return "Dialer";
    case MAIN_ROW_RECENTS: return "Recent calls";
    case MAIN_ROW_FAVORITES: return "Favorites";
    default: return "Contacts";
  }
}

static void draw_star_icon(GContext *ctx, GPoint origin) {
  gpath_move_to(s_star_path, origin);
  gpath_draw_filled(ctx, s_star_path);
}

static void draw_person_icon(GContext *ctx, GPoint origin) {
  graphics_fill_circle(ctx, GPoint(origin.x + 12, origin.y + 7), 5);
  graphics_fill_rect(ctx, GRect(origin.x + 3, origin.y + 14, 18, 9), 8, GCornersTop);
}

static uint16_t main_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return MAIN_ROW_COUNT;
}

// Settings come from the Settings page in the Pebble phone app (Clay, see
// src/pkjs/config.js). They are kept on the watch, so they still apply when
// the phone is away.

static void load_main_order(void) {
  uint8_t stored[MAIN_ROW_COUNT];
  if (persist_read_data(MENU_ORDER_STORAGE_KEY, stored, sizeof(stored)) != sizeof(stored)) {
    return;
  }
  // Only accept a complete arrangement of the known rows.
  bool seen[MAIN_ROW_COUNT] = { false };
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    if (stored[i] >= MAIN_ROW_COUNT || seen[stored[i]]) {
      return;
    }
    seen[stored[i]] = true;
  }
  memcpy(s_main_order, stored, sizeof(s_main_order));
}

// Builds the order from the page's four choices. A row chosen twice keeps its
// first position, and rows left out fill the remaining places in their usual
// order, so the menu always shows every row once.
static void set_main_order(const int32_t *choices) {
  uint8_t order[MAIN_ROW_COUNT];
  bool used[MAIN_ROW_COUNT] = { false };
  int count = 0;
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    const int32_t row = choices[i];
    if (row >= 0 && row < MAIN_ROW_COUNT && !used[row]) {
      used[row] = true;
      order[count++] = row;
    }
  }
  for (int row = 0; row < MAIN_ROW_COUNT; row++) {
    if (!used[row]) {
      order[count++] = row;
    }
  }
  memcpy(s_main_order, order, sizeof(s_main_order));
  persist_write_data(MENU_ORDER_STORAGE_KEY, s_main_order, sizeof(s_main_order));
}

static void load_settings(void) {
  load_main_order();
  if (persist_exists(THEME_STORAGE_KEY)) {
    const int32_t id = persist_read_int(THEME_STORAGE_KEY);
    if (id == THEME_DARK || id == THEME_LIGHT) {
      s_theme_id = id;
    }
  }
  if (persist_exists(TOUCH_STORAGE_KEY)) {
    s_touch_enabled = persist_read_bool(TOUCH_STORAGE_KEY);
  }
  if (persist_exists(TOUCH_VIBE_STORAGE_KEY)) {
    s_touch_vibe = persist_read_bool(TOUCH_VIBE_STORAGE_KEY);
  }
  theme_load(s_theme_id);
}

static void main_apply_theme(void);

// Recolours every screen that is open, after the theme changes.
static void refresh_theme(void) {
  main_apply_theme();
  for (int i = 0; i < MAX_OPEN_LISTS; i++) {
    if (s_open_lists[i]) {
      list_apply_theme(s_open_lists[i]);
    }
  }
  if (s_dial_window) {
    window_set_background_color(s_dial_window, s_theme.background);
    layer_mark_dirty(s_dial_layer);
  }
  if (s_call_window) {
    window_set_background_color(s_call_window, s_theme.background);
    text_layer_set_text_color(s_call_name_layer, s_theme.text);
    text_layer_set_text_color(s_call_status_layer, s_theme.hint);
    text_layer_set_text_color(s_call_number_layer, s_theme.hint);
    layer_mark_dirty(s_call_button_layer);
  }
}

// Clay sends selects as text and toggles as numbers.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

// Applies the values saved on the Settings page. Returns false if the message
// held no settings (it is then a reply to a list or call request).
static bool apply_settings(DictionaryIterator *iterator) {
  bool found = false;
  const Tuple *tuple;

  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    found = true;
    const int32_t id = tuple_int(tuple);
    if ((id == THEME_DARK || id == THEME_LIGHT) && id != s_theme_id) {
      s_theme_id = id;
      persist_write_int(THEME_STORAGE_KEY, s_theme_id);
      theme_load(s_theme_id);
      refresh_theme();
    }
  }

  const uint32_t menu_keys[MAIN_ROW_COUNT] = {
    MESSAGE_KEY_MENU_1, MESSAGE_KEY_MENU_2, MESSAGE_KEY_MENU_3, MESSAGE_KEY_MENU_4,
  };
  int32_t choices[MAIN_ROW_COUNT];
  bool menu_found = false;
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    tuple = dict_find(iterator, menu_keys[i]);
    choices[i] = tuple ? tuple_int(tuple) : s_main_order[i];
    menu_found = menu_found || tuple;
  }
  if (menu_found) {
    found = true;
    set_main_order(choices);
    if (s_main_menu_layer) {
      menu_layer_reload_data(s_main_menu_layer);
    }
  }

  if ((tuple = dict_find(iterator, MESSAGE_KEY_TOUCH_ENABLED))) {
    found = true;
    s_touch_enabled = tuple_int(tuple) != 0;
    persist_write_bool(TOUCH_STORAGE_KEY, s_touch_enabled);
    apply_touch_navigation();
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TOUCH_VIBE))) {
    found = true;
    s_touch_vibe = tuple_int(tuple) != 0;
    persist_write_bool(TOUCH_VIBE_STORAGE_KEY, s_touch_vibe);
  }
  return found;
}

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  const MainRow row = s_main_order[cell_index->row];
  const char *title = main_row_title(row);
  const GRect bounds = layer_get_bounds(cell_layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

#if defined(PBL_ROUND)
  // Round screens centre the icon and title together, like system menus.
  const GSize title_size = graphics_text_layout_get_content_size(
    title, font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  const int16_t left = (bounds.size.w - (ICON_SIZE + ICON_GAP + title_size.w)) / 2;
#else
  const int16_t left = ROW_MARGIN;
#endif
  const int16_t middle = bounds.size.h / 2;

  // Match the colours set in main_window_load so icons invert with the text.
  // On colour watches the Dialer and Favorites icons keep the keypad's accents.
  const GColor foreground = menu_cell_layer_is_highlighted(cell_layer)
    ? s_theme.highlight_text
    : s_theme.text;
  const GColor icon_color = PBL_IF_COLOR_ELSE(
    row == MAIN_ROW_DIALER ? s_theme.call : row == MAIN_ROW_FAVORITES ? s_theme.warning : foreground,
    foreground);
  graphics_context_set_fill_color(ctx, icon_color);
  const GPoint icon_origin = GPoint(left, middle - ICON_SIZE / 2);
  graphics_context_set_stroke_color(ctx, icon_color);
  if (row == MAIN_ROW_DIALER) {
    for (int i = 0; i < 9; i++) {
      graphics_fill_circle(ctx, GPoint(icon_origin.x + 5 + (i % 3) * 7,
          icon_origin.y + 5 + (i / 3) * 7), 2);
    }
  } else if (row == MAIN_ROW_RECENTS) {
    GPoint center = GPoint(icon_origin.x + 12, icon_origin.y + 12);
    graphics_draw_circle(ctx, center, 10);
    graphics_draw_line(ctx, center, GPoint(center.x, center.y - 7));
    graphics_draw_line(ctx, center, GPoint(center.x + 5, center.y + 3));
  } else if (row == MAIN_ROW_FAVORITES) {
    draw_star_icon(ctx, icon_origin);
  } else {
    draw_person_icon(ctx, icon_origin);
  }

  // Gothic glyphs sit low in their line box, so lift the text to centre it.
  const int16_t title_left = left + ICON_SIZE + ICON_GAP;
  graphics_context_set_text_color(ctx, foreground);
  graphics_draw_text(ctx, title, font,
                     GRect(title_left, middle - 15, bounds.size.w - title_left, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void main_select(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (s_main_order[cell_index->row]) {
    case MAIN_ROW_DIALER: dial_window_push(); break;
    case MAIN_ROW_RECENTS: list_window_push(LIST_RECENTS, NULL); break;
    case MAIN_ROW_FAVORITES: list_window_push(LIST_FAVORITES, NULL); break;
    default: list_window_push(LIST_LETTERS, NULL); break;
  }
}

static void main_apply_theme(void) {
  if (!s_main_menu_layer) {
    return;
  }
  menu_layer_set_normal_colors(s_main_menu_layer, s_theme.background, s_theme.text);
  menu_layer_set_highlight_colors(s_main_menu_layer, s_theme.highlight, s_theme.highlight_text);
  layer_mark_dirty(menu_layer_get_layer(s_main_menu_layer));
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_main_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = main_get_num_rows,
    .draw_row = main_draw_row,
    .select_click = main_select,
  });
  main_apply_theme();
  menu_layer_set_click_config_onto_window(s_main_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_main_menu_layer));
  s_star_path = gpath_create(&STAR_PATH_INFO);
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu_layer);
  s_main_menu_layer = NULL;
  gpath_destroy(s_star_path);
}

static void init(void) {
  load_settings();
  // Third-party apps get no touch navigation unless they ask; with it, the
  // menus scroll and select by touch.
  apply_touch_navigation();
  s_token = persist_read_int(TOKEN_STORAGE_KEY);
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  s_inbox_size = MIN(app_message_inbox_size_maximum(), (uint32_t)MAX_INBOX_SIZE);
  app_message_open(s_inbox_size, OUTBOX_SIZE);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  cancel_response_timer();
  cancel_exit_timer();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
