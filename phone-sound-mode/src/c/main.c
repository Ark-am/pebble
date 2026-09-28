#include <pebble.h>

// Message keys come from package.json as MESSAGE_KEY_COMMAND and friends.

enum {
  MODE_SILENT = 0,
  MODE_VIBRATE = 1,
  MODE_NORMAL = 2,
};

static const int32_t MODE_MENU_ORDER[] = {
  MODE_VIBRATE,
  MODE_NORMAL,
  MODE_SILENT,
};

enum {
  RESULT_OK = 0,
  RESULT_FAILED = 1,
  RESULT_PERMISSION_REQUIRED = 2,
};

static Window *s_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_status_layer;
static AppTimer *s_response_timer;
static AppTimer *s_exit_timer;
static int32_t s_pending_mode = -1;

#define RESPONSE_TIMEOUT_MS 10000
// Long enough to read the confirmation before returning to the watchface.
#define EXIT_DELAY_MS 1000

static const char *mode_name(int32_t mode) {
  switch (mode) {
    case MODE_SILENT:
      return "Silent";
    case MODE_VIBRATE:
      return "Vibrate";
    case MODE_NORMAL:
      return "Normal";
    default:
      return "Unknown";
  }
}

static void set_status(const char *text) {
  text_layer_set_text(s_status_layer, text);
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
  s_pending_mode = -1;
  set_status("No response; select again");
}

static void send_mode(int32_t mode) {
  DictionaryIterator *iterator;
  const AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result == APP_MSG_BUSY) {
    // The previous command is still in flight; keep waiting for its reply.
    set_status("Still sending...");
    return;
  }

  cancel_response_timer();
  cancel_exit_timer();
  s_pending_mode = -1;

  if (result != APP_MSG_OK) {
    set_status("Phone unavailable");
    return;
  }

  dict_write_int32(iterator, MESSAGE_KEY_COMMAND, mode);
  s_pending_mode = mode;
  set_status("Sending...");

  if (app_message_outbox_send() != APP_MSG_OK) {
    s_pending_mode = -1;
    set_status("Send failed");
    return;
  }

  s_response_timer = app_timer_register(
    RESPONSE_TIMEOUT_MS,
    response_timeout,
    NULL
  );
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer,
                                  uint16_t section_index,
                                  void *context) {
  return ARRAY_LENGTH(MODE_MENU_ORDER);
}

static void menu_draw_row(GContext *ctx,
                          const Layer *cell_layer,
                          MenuIndex *cell_index,
                          void *context) {
  const int32_t mode = MODE_MENU_ORDER[cell_index->row];
  menu_cell_basic_draw(ctx, cell_layer, mode_name(mode), NULL, NULL);
}

static void menu_select(MenuLayer *menu_layer,
                        MenuIndex *cell_index,
                        void *context) {
  send_mode(MODE_MENU_ORDER[cell_index->row]);
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  cancel_response_timer();

  const Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT);
  const Tuple *mode_tuple = dict_find(iterator, MESSAGE_KEY_CURRENT_MODE);

  if (!result_tuple) {
    set_status("Invalid phone response");
    return;
  }

  const int32_t result = result_tuple->value->int32;
  if (result == RESULT_PERMISSION_REQUIRED) {
    set_status("Grant access on phone");
    return;
  }

  if (result != RESULT_OK) {
    set_status("Could not change mode");
    return;
  }

  const int32_t mode = mode_tuple ? mode_tuple->value->int32 : s_pending_mode;
  static char status[24];
  snprintf(status, sizeof(status), "%s enabled", mode_name(mode));
  set_status(status);
  vibes_short_pulse();
  s_pending_mode = -1;

  // Only leave after the phone confirms, so failures stay visible for a retry.
  cancel_exit_timer();
  s_exit_timer = app_timer_register(EXIT_DELAY_MS, exit_to_watchface, NULL);
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  cancel_response_timer();
  s_pending_mode = -1;
  set_status("Phone response lost");
}

static void outbox_failed(DictionaryIterator *iterator,
                          AppMessageResult reason,
                          void *context) {
  cancel_response_timer();
  s_pending_mode = -1;
  set_status("Phone unavailable");
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  // Round screens clip the bottom corners, so give the status two lines and
  // let the text flow within the visible part of the display.
  const int16_t status_height = PBL_IF_ROUND_ELSE(40, 28);

  s_menu_layer = menu_layer_create(GRect(
    0,
    0,
    bounds.size.w,
    bounds.size.h - status_height
  ));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  s_status_layer = text_layer_create(GRect(
    0,
    bounds.size.h - status_height,
    bounds.size.w,
    status_height
  ));
  text_layer_set_background_color(s_status_layer, GColorBlack);
  text_layer_set_text_color(s_status_layer, GColorWhite);
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_text(s_status_layer, "Choose phone mode");
  layer_add_child(root, text_layer_get_layer(s_status_layer));
#if defined(PBL_ROUND)
  text_layer_enable_screen_text_flow_and_paging(s_status_layer, 4);
#endif
}

static void window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
  menu_layer_destroy(s_menu_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(128, 64);
}

static void deinit(void) {
  cancel_response_timer();
  cancel_exit_timer();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
