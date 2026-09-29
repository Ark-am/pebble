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
static GPath *s_speaker_cone;
static AppTimer *s_response_timer;
static AppTimer *s_exit_timer;
static int32_t s_pending_mode = -1;

#define RESPONSE_TIMEOUT_MS 10000
// Long enough to read the confirmation before returning to the watchface.
#define EXIT_DELAY_MS 1000

// Icons are drawn as vector shapes in a square box so they work on every
// platform, including black-and-white ones, and follow the row's highlight.
#define ICON_SIZE 24
#define ICON_GAP 8
#define ROW_MARGIN 8

static const GPathInfo SPEAKER_CONE_INFO = {
  .num_points = 4,
  .points = (GPoint[]) { {7, 8}, {13, 3}, {13, 21}, {7, 16} },
};

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

static void draw_speaker(GContext *ctx, GPoint origin) {
  graphics_fill_rect(ctx, GRect(origin.x + 2, origin.y + 8, 5, 8), 0, GCornerNone);
  gpath_move_to(s_speaker_cone, origin);
  gpath_draw_filled(ctx, s_speaker_cone);
}

static void draw_normal_icon(GContext *ctx, GPoint origin) {
  draw_speaker(ctx, origin);
  // Two sound waves centred on the speaker's mouth.
  const int32_t start = DEG_TO_TRIGANGLE(50);
  const int32_t end = DEG_TO_TRIGANGLE(130);
  graphics_draw_arc(ctx, GRect(origin.x + 9, origin.y + 7, 10, 10),
                    GOvalScaleModeFitCircle, start, end);
  graphics_draw_arc(ctx, GRect(origin.x + 5, origin.y + 3, 18, 18),
                    GOvalScaleModeFitCircle, start, end);
}

static void draw_silent_icon(GContext *ctx, GPoint origin) {
  draw_speaker(ctx, origin);
  graphics_draw_line(ctx, GPoint(origin.x + 16, origin.y + 8),
                     GPoint(origin.x + 22, origin.y + 16));
  graphics_draw_line(ctx, GPoint(origin.x + 22, origin.y + 8),
                     GPoint(origin.x + 16, origin.y + 16));
}

static void draw_vibrate_icon(GContext *ctx, GPoint origin) {
  graphics_draw_round_rect(ctx, GRect(origin.x + 8, origin.y + 3, 8, 18), 2);

  // Zigzags on both sides of the phone, mirrored around the icon's centre.
  graphics_context_set_stroke_width(ctx, 1);
  static const GPoint zigzag[] = { {4, 6}, {1, 9}, {4, 12}, {1, 15}, {4, 18} };
  for (size_t i = 1; i < ARRAY_LENGTH(zigzag); i++) {
    const GPoint a = zigzag[i - 1];
    const GPoint b = zigzag[i];
    graphics_draw_line(ctx, GPoint(origin.x + a.x, origin.y + a.y),
                       GPoint(origin.x + b.x, origin.y + b.y));
    graphics_draw_line(ctx, GPoint(origin.x + ICON_SIZE - 1 - a.x, origin.y + a.y),
                       GPoint(origin.x + ICON_SIZE - 1 - b.x, origin.y + b.y));
  }
}

static void draw_mode_icon(GContext *ctx, int32_t mode, GPoint origin) {
  switch (mode) {
    case MODE_SILENT:
      draw_silent_icon(ctx, origin);
      break;
    case MODE_VIBRATE:
      draw_vibrate_icon(ctx, origin);
      break;
    case MODE_NORMAL:
      draw_normal_icon(ctx, origin);
      break;
  }
}

static void menu_draw_row(GContext *ctx,
                          const Layer *cell_layer,
                          MenuIndex *cell_index,
                          void *context) {
  const int32_t mode = MODE_MENU_ORDER[cell_index->row];
  const char *title = mode_name(mode);
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

  // Match the colours set in window_load so icons invert with the text.
  const GColor foreground = menu_cell_layer_is_highlighted(cell_layer)
    ? GColorWhite
    : GColorBlack;
  graphics_context_set_fill_color(ctx, foreground);
  graphics_context_set_stroke_color(ctx, foreground);
  graphics_context_set_stroke_width(ctx, 2);
  draw_mode_icon(ctx, mode, GPoint(left, middle - ICON_SIZE / 2));

  // Gothic glyphs sit low in their line box, so lift the text to centre it.
  const int16_t title_left = left + ICON_SIZE + ICON_GAP;
  graphics_context_set_text_color(ctx, foreground);
  graphics_draw_text(ctx, title, font,
                     GRect(title_left, middle - 15, bounds.size.w - title_left, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
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
  menu_layer_set_normal_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer, GColorBlack, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
  s_speaker_cone = gpath_create(&SPEAKER_CONE_INFO);

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
  text_layer_set_text(s_status_layer, "www.ark-am.com");
  layer_add_child(root, text_layer_get_layer(s_status_layer));
#if defined(PBL_ROUND)
  text_layer_enable_screen_text_flow_and_paging(s_status_layer, 4);
#endif
}

static void window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
  menu_layer_destroy(s_menu_layer);
  gpath_destroy(s_speaker_cone);
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
