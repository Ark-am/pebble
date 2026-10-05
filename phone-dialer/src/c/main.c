#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_REQUEST and friends.
// The Android companion owns the contact data; the watch only ever holds one
// page of a list at a time so it fits in the smallest app heap.

enum {
  REQUEST_LIST = 1,
  REQUEST_CALL = 2,
  REQUEST_DIAL = 3,
  // Not a reply-driven request: shares the main menu order with the phone.
  REQUEST_SYNC_MENU = 4,
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
// Key 3 held the old on-watch reorder hint flag; keep it unused.
#define MENU_STAMP_STORAGE_KEY 4
// The phone's companion may still be starting when the watch app opens.
#define MENU_SYNC_DELAY_MS 1000
#define MENU_SYNC_RETRY_MS 3000
#define MENU_SYNC_ATTEMPTS 3

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define RESPONSE_TIMEOUT_MS 10000
// Long enough to read the confirmation before returning to the watchface.
#define EXIT_DELAY_MS 1500

// App theme. Colour watches: white on black with dark gray highlights, a
// red-orange for problems and a blue-green for calling, like the keypad.
// Black-and-white watches: black on white with inverted highlights.
#define THEME_BACKGROUND PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite)
#define THEME_TEXT PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)
#define THEME_HINT PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack)
#define THEME_DIVIDER PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define THEME_HIGHLIGHT PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define THEME_HIGHLIGHT_TEXT GColorWhite
#define THEME_CALL PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack)
#define THEME_WARNING PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack)

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
// Main menu order, chosen in the phone app's settings and kept on the watch
// so the menu is right even when the phone is away.
static uint8_t s_main_order[MAIN_ROW_COUNT] = {
  MAIN_ROW_DIALER, MAIN_ROW_RECENTS, MAIN_ROW_FAVORITES, MAIN_ROW_CONTACTS,
};
// When the order last changed, so the watch and the phone keep the newer one.
static int32_t s_main_order_stamp;
static bool s_menu_sync_in_flight;
static int s_menu_sync_attempts;
static AppTimer *s_menu_sync_timer;
static GPath *s_star_path;

static Window *s_call_window;
static TextLayer *s_call_status_layer;
static TextLayer *s_call_name_layer;
static char s_call_name[TITLE_SIZE];
static Entry s_call_entry;
static int32_t s_call_source;
static bool s_call_submitted;
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
static void apply_phone_main_order(const char *text, int32_t stamp);
static void schedule_menu_sync(uint32_t delay_ms);

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
static void call_show_error(const char *status);

static void fail_request(const char *message) {
  cancel_response_timer();
  ListView *view = s_waiting_list;
  const bool waiting_call = s_waiting_call;
  s_waiting_list = NULL;
  s_waiting_call = false;
  // Ignore any late replies to the request that just failed.
  s_token = s_token >= INT32_MAX ? 1 : s_token + 1;
  persist_write_int(TOKEN_STORAGE_KEY, s_token);

  if (view) {
    list_show_error(view, message);
  } else if (waiting_call) {
    call_show_error(message);
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
  s_token = s_token >= INT32_MAX ? 1 : s_token + 1;
  persist_write_int(TOKEN_STORAGE_KEY, s_token);

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

static uint16_t list_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  const ListView *view = context;
  if (list_shows_message(view)) {
    return 1;
  }
  return view->received + list_has_previous(view) + list_has_more(view);
}

static int16_t list_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
#if defined(PBL_ROUND)
  return menu_layer_is_index_selected(menu_layer, cell_index)
    ? MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT
    : MENU_CELL_ROUND_UNFOCUSED_SHORT_CELL_HEIGHT;
#else
  return 44;
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
    const char *subtitle = view->state == LIST_STATE_ERROR ? "Select to retry" : NULL;
    menu_cell_basic_draw(ctx, cell_layer, view->message, subtitle, NULL);
    return;
  }

  int32_t row = cell_index->row;
  if (list_has_previous(view)) {
    if (row == 0) {
      static char range[24];
      snprintf(range, sizeof(range), "%d-%d",
               (int)(MAX(view->offset - PAGE_SIZE, 0) + 1), (int)view->offset);
      menu_cell_basic_draw(ctx, cell_layer, "Previous", range, NULL);
      return;
    }
    row--;
  }

  if (row >= view->received) {
    static char range[24];
    const int32_t first = view->offset + view->received + 1;
    const int32_t last = MIN(view->offset + view->received + PAGE_SIZE, view->total);
    snprintf(range, sizeof(range), "%d-%d of %d", (int)first, (int)last, (int)view->total);
    menu_cell_basic_draw(ctx, cell_layer, "More", range, NULL);
    return;
  }

  const Entry *entry = &view->entries[row];
  menu_cell_basic_draw(ctx, cell_layer, entry->title, entry->subtitle, NULL);
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

static void list_window_load(Window *window) {
  ListView *view = window_get_user_data(window);
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
  menu_layer_set_normal_colors(view->menu_layer, THEME_BACKGROUND, THEME_TEXT);
  menu_layer_set_highlight_colors(view->menu_layer, THEME_HIGHLIGHT, THEME_HIGHLIGHT_TEXT);
  menu_layer_set_click_config_onto_window(view->menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(view->menu_layer));

  list_request_page(view, 0, 0);
}

static void list_window_unload(Window *window) {
  ListView *view = window_get_user_data(window);
  if (s_waiting_list == view) {
    cancel_response_timer();
    s_waiting_list = NULL;
    s_token = s_token >= INT32_MAX ? 1 : s_token + 1;
    persist_write_int(TOKEN_STORAGE_KEY, s_token);
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
  call_set_status(status, THEME_HINT);
}

static void call_show_error(const char *status) {
  call_set_status(status, THEME_WARNING);
}

static void call_handle_reply(int32_t result) {
  if (result != RESULT_OK) {
    fail_request(result_message(result));
    return;
  }

  cancel_response_timer();
  s_waiting_call = false;
  call_set_status("Sent to phone", THEME_CALL);
  vibes_short_pulse();

  // Hand over to the phone's call screen once the call is placed.
  cancel_exit_timer();
  s_exit_timer = app_timer_register(EXIT_DELAY_MS, exit_to_watchface, NULL);
}

static void call_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, THEME_BACKGROUND);

  const int16_t inset = PBL_IF_ROUND_ELSE(18, 6);
  const int16_t middle = bounds.size.h / 2;

  s_call_status_layer = text_layer_create(GRect(inset, middle - 50, bounds.size.w - 2 * inset, 44));
  text_layer_set_font(s_call_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_call_status_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_call_status_layer, GColorClear);
  text_layer_set_text_color(s_call_status_layer, THEME_HINT);
  layer_add_child(root, text_layer_get_layer(s_call_status_layer));

  s_call_name_layer = text_layer_create(GRect(inset, middle - 4, bounds.size.w - 2 * inset, 72));
  text_layer_set_font(s_call_name_layer, fonts_get_system_font(
      s_call_source == -1 ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_call_name_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_call_name_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_call_name_layer, GColorClear);
  text_layer_set_text_color(s_call_name_layer, THEME_TEXT);
  text_layer_set_text(s_call_name_layer, s_call_name);
  layer_add_child(root, text_layer_get_layer(s_call_name_layer));
}

static void call_window_unload(Window *window) {
  if (s_waiting_call) {
    cancel_response_timer();
    s_waiting_call = false;
    s_token = s_token >= INT32_MAX ? 1 : s_token + 1;
    persist_write_int(TOKEN_STORAGE_KEY, s_token);
  }
  cancel_exit_timer();
  text_layer_destroy(s_call_status_layer);
  text_layer_destroy(s_call_name_layer);
  s_call_status_layer = NULL;
  s_call_name_layer = NULL;
  window_destroy(window);
  s_call_window = NULL;
}

static void call_submit(ClickRecognizerRef recognizer, void *context) {
  if (s_call_submitted) {
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
  // Never automatically retry a call after a lost reply: it may already be dialing.
  s_call_submitted = true;
  s_waiting_call = true;
  call_show_status("Sending call...");
}

static void call_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, call_submit);
}

static void call_window_push(const Entry *entry, int32_t source) {
  s_call_entry = *entry;
  s_call_source = source;
  s_call_submitted = false;
  strncpy(s_call_name, entry->title, sizeof(s_call_name) - 1);
  s_call_name[sizeof(s_call_name) - 1] = '\0';

  s_call_window = window_create();
  window_set_window_handlers(s_call_window, (WindowHandlers) {
    .load = call_window_load,
    .unload = call_window_unload,
  });
  window_set_click_config_provider(s_call_window, call_click_config);
  window_stack_push(s_call_window, true);
  if (source != -1 && (!entry->id[0] || strcmp(entry->id, "0") == 0)) {
    s_call_submitted = true;
    call_show_error("Number unavailable");
  } else {
    call_show_status("Select to call");
  }
}

// ---------------------------------------------------------------------------
// Number keypad. Up/Down move the highlight, Select presses the highlighted
// key, and on touch watches every key can also be tapped directly.

#if defined(_PBL_API_EXISTS_tap_recognizer_create)
#define DIAL_TOUCH 1
#endif

// Colour watches: gray keys on black, like a calculator, with a red-orange
// Delete key, a blue-green Call key and a white ring for the highlight.
// Black-and-white watches: outlined keys that invert when highlighted or
// pressed, and a solid Call key.
#define DIAL_BACKGROUND THEME_BACKGROUND
#define DIAL_TEXT THEME_TEXT
#define DIAL_HINT THEME_HINT
#define DIAL_DIVIDER THEME_DIVIDER

#define PRESS_FLASH_MS 150

// Phone handset centred on (0, 0) and 180 units across; scaled to fit a key.
static const GPoint HANDSET_POINTS[] = {
  {-54, -12}, {-38, 13}, {-14, 38}, {12, 54}, {34, 32}, {44, 29}, {62, 34}, {80, 35},
  {90, 45}, {90, 80}, {80, 90}, {47, 87}, {15, 77}, {-14, 61}, {-40, 40}, {-61, 14},
  {-77, -15}, {-87, -47}, {-90, -80}, {-80, -90}, {-45, -90}, {-35, -80}, {-29, -44},
  {-32, -34},
};

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

static void dial_create_handset(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(HANDSET_POINTS); i++) {
    s_handset_points[i] = GPoint(HANDSET_POINTS[i].x * s_icon_size / 180,
                                 HANDSET_POINTS[i].y * s_icon_size / 180);
  }
  s_handset_info = (GPathInfo) {
    .num_points = ARRAY_LENGTH(HANDSET_POINTS),
    .points = s_handset_points,
  };
  s_handset_path = gpath_create(&s_handset_info);
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
  GColor ink = GColorWhite;
  if (key == KEY_CALL) {
    fill = pressed ? GColorMalachite : GColorJaegerGreen;
  } else if (key == KEY_DELETE) {
    fill = pressed ? GColorSunsetOrange : GColorOrange;
  } else {
    fill = pressed ? GColorLightGray : GColorDarkGray;
    ink = pressed ? GColorBlack : GColorWhite;
  }

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, radius, GCornersAll);
  if (focused) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, grect_inset(rect, GEdgeInsets(1)), radius);
  }
#else
  // The Call key is solid and inverts to an outline; the others do the opposite.
  const bool inverted = (focused || pressed) != (key == KEY_CALL);
  const GColor fill = inverted ? GColorBlack : GColorWhite;
  const GColor ink = inverted ? GColorWhite : GColorBlack;

  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, rect, radius, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
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
    graphics_context_set_text_color(ctx, DIAL_HINT);
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
  graphics_context_set_text_color(ctx, DIAL_TEXT);
  graphics_draw_text(ctx, text, font,
                     GRect(area.origin.x, area.origin.y + (area.size.h - size.h) / 2 - size.h / 5,
                           area.size.w, size.h + size.h / 5),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void dial_draw(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, DIAL_BACKGROUND);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  draw_number(ctx);
  graphics_context_set_stroke_color(ctx, DIAL_DIVIDER);
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

#if defined(DIAL_TOUCH)
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

static void dial_tap(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed) {
    return;
  }
  const int key = dial_key_at(tap_recognizer_get_tap_point(recognizer));
  if (key < 0) {
    return;
  }

  s_key = key;
  s_show_focus = false;
  s_pressed_key = key;
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
  }
  s_press_timer = app_timer_register(PRESS_FLASH_MS, dial_clear_press, NULL);
  dial_press(key);
}
#endif

static void dial_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, DIAL_BACKGROUND);
  dial_layout(bounds);
  dial_create_handset();

  s_dial_layer = layer_create(bounds);
  layer_set_update_proc(s_dial_layer, dial_draw);
  layer_add_child(root, s_dial_layer);

  s_show_focus = true;
#if defined(DIAL_TOUCH)
  if (touch_service_is_enabled()) {
    // Taps go to the keys instead of being turned into button presses, and
    // the button highlight stays hidden until a button is used.
    s_show_focus = false;
    window_set_touch_bridge_disabled(window, true);
    window_attach_recognizer(window, tap_recognizer_create(dial_tap, NULL));
  }
#endif
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
    .unload = dial_window_unload,
  });
  window_stack_push(s_dial_window, true);
}

// ---------------------------------------------------------------------------
// AppMessage

static void inbox_received(DictionaryIterator *iterator, void *context) {
  // Menu order updates can arrive at any time and are not tied to a request.
  const Tuple *order_tuple = dict_find(iterator, MESSAGE_KEY_MENU_ORDER);
  if (order_tuple && order_tuple->type == TUPLE_CSTRING) {
    const Tuple *stamp_tuple = dict_find(iterator, MESSAGE_KEY_MENU_STAMP);
    apply_phone_main_order(order_tuple->value->cstring, stamp_tuple ? stamp_tuple->value->int32 : 0);
    return;
  }

  const Tuple *token_tuple = dict_find(iterator, MESSAGE_KEY_TOKEN);
  if (!token_tuple || token_tuple->value->int32 != s_token) {
    return;
  }

  const Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT);
  const int32_t result = result_tuple ? result_tuple->value->int32 : RESULT_FAILED;

  if (s_waiting_call) {
    call_handle_reply(result);
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

static void outbox_sent(DictionaryIterator *iterator, void *context) {
  s_menu_sync_in_flight = false;
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  if (s_menu_sync_in_flight) {
    s_menu_sync_in_flight = false;
    schedule_menu_sync(MENU_SYNC_RETRY_MS);
    return;
  }
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

// The order travels to and from the phone as digits, one per row: "0123".
static bool parse_main_order(const char *text, uint8_t *order) {
  bool seen[MAIN_ROW_COUNT] = { false };
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    const int row = text[i] - '0';
    if (row < 0 || row >= MAIN_ROW_COUNT || seen[row]) {
      return false;
    }
    seen[row] = true;
    order[i] = row;
  }
  return text[MAIN_ROW_COUNT] == '\0';
}

static void load_main_order(void) {
  uint8_t stored[MAIN_ROW_COUNT];
  char text[MAIN_ROW_COUNT + 1];
  if (persist_read_data(MENU_ORDER_STORAGE_KEY, stored, sizeof(stored)) != sizeof(stored)) {
    return;
  }
  // Only accept a complete arrangement of the known rows.
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    text[i] = '0' + MIN(stored[i], 9);
  }
  text[MAIN_ROW_COUNT] = '\0';
  if (parse_main_order(text, s_main_order)) {
    s_main_order_stamp = persist_read_int(MENU_STAMP_STORAGE_KEY);
  }
}

static void save_main_order(void) {
  persist_write_data(MENU_ORDER_STORAGE_KEY, s_main_order, sizeof(s_main_order));
  persist_write_int(MENU_STAMP_STORAGE_KEY, s_main_order_stamp);
}

// Tells the phone the watch's order. The phone keeps it if it is newer and
// answers with its own if that is newer. If the phone is busy or away, the
// next launch tries again, so nothing is lost.
static void sync_main_order(void);

static void menu_sync_timer_fired(void *context) {
  s_menu_sync_timer = NULL;
  sync_main_order();
}

static void schedule_menu_sync(uint32_t delay_ms) {
  if (s_menu_sync_timer) {
    app_timer_cancel(s_menu_sync_timer);
  }
  s_menu_sync_timer = app_timer_register(delay_ms, menu_sync_timer_fired, NULL);
}

static void sync_main_order(void) {
  DictionaryIterator *iterator;
  if (s_menu_sync_attempts >= MENU_SYNC_ATTEMPTS) {
    return;
  }
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    // Busy with a list or call; try again shortly.
    s_menu_sync_attempts++;
    schedule_menu_sync(MENU_SYNC_RETRY_MS);
    return;
  }
  char text[MAIN_ROW_COUNT + 1];
  for (int i = 0; i < MAIN_ROW_COUNT; i++) {
    text[i] = '0' + s_main_order[i];
  }
  text[MAIN_ROW_COUNT] = '\0';
  dict_write_int32(iterator, MESSAGE_KEY_REQUEST, REQUEST_SYNC_MENU);
  dict_write_cstring(iterator, MESSAGE_KEY_MENU_ORDER, text);
  dict_write_int32(iterator, MESSAGE_KEY_MENU_STAMP, s_main_order_stamp);
  s_menu_sync_attempts++;
  s_menu_sync_in_flight = app_message_outbox_send() == APP_MSG_OK;
}

// An order from the phone, chosen in the companion app's settings.
static void apply_phone_main_order(const char *text, int32_t stamp) {
  uint8_t order[MAIN_ROW_COUNT];
  if (stamp <= s_main_order_stamp || !parse_main_order(text, order)) {
    return;
  }
  memcpy(s_main_order, order, sizeof(s_main_order));
  s_main_order_stamp = stamp;
  save_main_order();
  if (s_main_menu_layer) {
    menu_layer_reload_data(s_main_menu_layer);
  }
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
    ? THEME_HIGHLIGHT_TEXT
    : THEME_TEXT;
  const GColor icon_color = PBL_IF_COLOR_ELSE(
    row == MAIN_ROW_DIALER ? THEME_CALL : row == MAIN_ROW_FAVORITES ? THEME_WARNING : foreground,
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

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_main_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = main_get_num_rows,
    .draw_row = main_draw_row,
    .select_click = main_select,
  });
  menu_layer_set_normal_colors(s_main_menu_layer, THEME_BACKGROUND, THEME_TEXT);
  menu_layer_set_highlight_colors(s_main_menu_layer, THEME_HIGHLIGHT, THEME_HIGHLIGHT_TEXT);
  menu_layer_set_click_config_onto_window(s_main_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_main_menu_layer));
  s_star_path = gpath_create(&STAR_PATH_INFO);
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu_layer);
  gpath_destroy(s_star_path);
}

static void init(void) {
  s_token = persist_read_int(TOKEN_STORAGE_KEY);
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_register_outbox_sent(outbox_sent);
  s_inbox_size = MIN(app_message_inbox_size_maximum(), (uint32_t)MAX_INBOX_SIZE);
  app_message_open(s_inbox_size, OUTBOX_SIZE);
  load_main_order();
  schedule_menu_sync(MENU_SYNC_DELAY_MS);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  if (s_menu_sync_timer) {
    app_timer_cancel(s_menu_sync_timer);
  }
  cancel_response_timer();
  cancel_exit_timer();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
