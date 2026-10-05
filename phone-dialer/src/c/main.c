#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_REQUEST and friends.
// The Android companion owns the contact data; the watch only ever holds one
// page of a list at a time so it fits in the smallest app heap.

enum {
  REQUEST_LIST = 1,
  REQUEST_CALL = 2,
};

enum {
  LIST_FAVORITES = 0,
  LIST_LETTERS = 1,
  LIST_CONTACTS = 2,
};

enum {
  RESULT_OK = 0,
  RESULT_FAILED = 1,
  RESULT_PERMISSION_REQUIRED = 2,
  RESULT_NOT_FOUND = 3,
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
#define OUTBOX_SIZE 96

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define RESPONSE_TIMEOUT_MS 10000
// Long enough to read the confirmation before returning to the watchface.
#define EXIT_DELAY_MS 1500

// Icons are drawn as vector shapes in a square box so they work on every
// platform, including black-and-white ones, and follow the row's highlight.
#define ICON_SIZE 24
#define ICON_GAP 8
#define ROW_MARGIN 8

typedef struct {
  int32_t id;
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
  MAIN_ROW_FAVORITES,
  MAIN_ROW_CONTACTS,
  MAIN_ROW_COUNT,
} MainRow;

static Window *s_main_window;
static MenuLayer *s_main_menu_layer;
static GPath *s_star_path;

static Window *s_call_window;
static TextLayer *s_call_status_layer;
static TextLayer *s_call_name_layer;
static char s_call_name[TITLE_SIZE];

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

// ---------------------------------------------------------------------------
// Requests

static const char *result_message(int32_t result) {
  switch (result) {
    case RESULT_PERMISSION_REQUIRED:
      return "Allow access on phone";
    case RESULT_NOT_FOUND:
      return "Number not found";
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
static void call_show_status(const char *status);

static void fail_request(const char *message) {
  cancel_response_timer();
  ListView *view = s_waiting_list;
  const bool waiting_call = s_waiting_call;
  s_waiting_list = NULL;
  s_waiting_call = false;
  // Ignore any late replies to the request that just failed.
  s_token++;

  if (view) {
    list_show_error(view, message);
  } else if (waiting_call) {
    call_show_status(message);
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
  s_token++;

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
    view->message = view->list == LIST_FAVORITES ? "No favorites" : "No contacts";
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

static int32_t parse_int(const char *start, const char *end) {
  int32_t value = 0;
  for (const char *c = start; c < end && *c >= '0' && *c <= '9'; c++) {
    value = value * 10 + (*c - '0');
  }
  return value;
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
      target->id = parse_int(fields[0], field_ends[0]);
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

static void call_window_push(const Entry *entry);

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
    call_window_push(entry);
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
  menu_layer_set_normal_colors(view->menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(view->menu_layer, GColorBlack, GColorWhite);
  menu_layer_set_click_config_onto_window(view->menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(view->menu_layer));

  list_request_page(view, 0, 0);
}

static void list_window_unload(Window *window) {
  ListView *view = window_get_user_data(window);
  if (s_waiting_list == view) {
    cancel_response_timer();
    s_waiting_list = NULL;
    s_token++;
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

static void call_show_status(const char *status) {
  if (s_call_status_layer) {
    text_layer_set_text(s_call_status_layer, status);
  }
}

static void call_handle_reply(int32_t result) {
  if (result != RESULT_OK) {
    fail_request(result_message(result));
    return;
  }

  cancel_response_timer();
  s_waiting_call = false;
  call_show_status("Calling");
  vibes_short_pulse();

  // Hand over to the phone's call screen once the call is placed.
  cancel_exit_timer();
  s_exit_timer = app_timer_register(EXIT_DELAY_MS, exit_to_watchface, NULL);
}

static void call_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  const int16_t inset = PBL_IF_ROUND_ELSE(18, 6);
  const int16_t middle = bounds.size.h / 2;

  s_call_status_layer = text_layer_create(GRect(inset, middle - 44, bounds.size.w - 2 * inset, 24));
  text_layer_set_font(s_call_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_call_status_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_call_status_layer, GColorClear);
  text_layer_set_text_color(s_call_status_layer, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_call_status_layer));

  s_call_name_layer = text_layer_create(GRect(inset, middle - 20, bounds.size.w - 2 * inset, 64));
  text_layer_set_font(s_call_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_call_name_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_call_name_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_call_name_layer, GColorClear);
  text_layer_set_text_color(s_call_name_layer, GColorBlack);
  text_layer_set_text(s_call_name_layer, s_call_name);
  layer_add_child(root, text_layer_get_layer(s_call_name_layer));
}

static void call_window_unload(Window *window) {
  if (s_waiting_call) {
    cancel_response_timer();
    s_waiting_call = false;
    s_token++;
  }
  cancel_exit_timer();
  text_layer_destroy(s_call_status_layer);
  text_layer_destroy(s_call_name_layer);
  s_call_status_layer = NULL;
  s_call_name_layer = NULL;
  window_destroy(window);
  s_call_window = NULL;
}

static void call_window_push(const Entry *entry) {
  strncpy(s_call_name, entry->title, sizeof(s_call_name) - 1);
  s_call_name[sizeof(s_call_name) - 1] = '\0';

  s_call_window = window_create();
  window_set_window_handlers(s_call_window, (WindowHandlers) {
    .load = call_window_load,
    .unload = call_window_unload,
  });
  window_stack_push(s_call_window, true);
  call_show_status("Dialing...");

  const char *error = NULL;
  DictionaryIterator *iterator = begin_request(REQUEST_CALL, &error);
  if (!iterator) {
    call_show_status(error);
    return;
  }
  dict_write_int32(iterator, MESSAGE_KEY_ITEM_ID, entry->id);
  if (!send_request(&error)) {
    call_show_status(error);
    return;
  }
  s_waiting_call = true;
}

// ---------------------------------------------------------------------------
// AppMessage

static void inbox_received(DictionaryIterator *iterator, void *context) {
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

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  if (s_waiting_list || s_waiting_call) {
    fail_request("Phone unavailable");
  }
}

// ---------------------------------------------------------------------------
// Main menu

static const char *main_row_title(MainRow row) {
  return row == MAIN_ROW_FAVORITES ? "Favorites" : "Contacts";
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

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  const MainRow row = cell_index->row;
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
  const GColor foreground = menu_cell_layer_is_highlighted(cell_layer)
    ? GColorWhite
    : GColorBlack;
  graphics_context_set_fill_color(ctx, foreground);
  const GPoint icon_origin = GPoint(left, middle - ICON_SIZE / 2);
  if (row == MAIN_ROW_FAVORITES) {
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
  list_window_push(cell_index->row == MAIN_ROW_FAVORITES ? LIST_FAVORITES : LIST_LETTERS, NULL);
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  s_main_menu_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = main_get_num_rows,
    .draw_row = main_draw_row,
    .select_click = main_select,
  });
  menu_layer_set_normal_colors(s_main_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_main_menu_layer, GColorBlack, GColorWhite);
  menu_layer_set_click_config_onto_window(s_main_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_main_menu_layer));
  s_star_path = gpath_create(&STAR_PATH_INFO);
}

static void main_window_unload(Window *window) {
  menu_layer_destroy(s_main_menu_layer);
  gpath_destroy(s_star_path);
}

static void init(void) {
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
