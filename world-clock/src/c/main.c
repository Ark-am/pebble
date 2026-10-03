#include <pebble.h>

// A digital world clock. The watch's own time is shown large at the top, with
// the date beside it, and the times in two other cities are listed below.
// The phone works out each city's offset from UTC (see src/pkjs) and the watch
// keeps the last offsets it was given, so the times stay right without the
// phone.

#define PERSIST_KEY_SETTINGS 1

#define CITY_COUNT 2
#define LABEL_LENGTH 16

#if PBL_DISPLAY_WIDTH >= 200
  #define DATE_FONT FONT_KEY_GOTHIC_18_BOLD
  #define CITY_FONT FONT_KEY_GOTHIC_24_BOLD
  #define CITY_TIME_FONT FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
  #define DETAIL_FONT FONT_KEY_GOTHIC_18
  // Visible heights of each font's text and the blank space the font leaves
  // above it.
  #define DATE_HEIGHT 13
  #define DATE_PAD 5
  #define CITY_HEIGHT 17
  #define CITY_PAD 7
  #define CITY_TIME_HEIGHT 19
  #define CITY_TIME_PAD 6
  #define DETAIL_HEIGHT 13
  #define DETAIL_PAD 5
  #define TIME_HEIGHT 31
  #define TIME_PAD 10
  #define GAP 10
#else
  #define DATE_FONT FONT_KEY_GOTHIC_14_BOLD
  #define CITY_FONT FONT_KEY_GOTHIC_18_BOLD
  #define CITY_TIME_FONT FONT_KEY_LECO_20_BOLD_NUMBERS
  #define DETAIL_FONT FONT_KEY_GOTHIC_14
  #define DATE_HEIGHT 10
  #define DATE_PAD 4
  #define CITY_HEIGHT 13
  #define CITY_PAD 5
  #define CITY_TIME_HEIGHT 14
  #define CITY_TIME_PAD 4
  #define DETAIL_HEIGHT 10
  #define DETAIL_PAD 4
  #define TIME_HEIGHT 27
  #define TIME_PAD 9
  #define GAP 7
#endif

// Space at the sides of the screen. Round screens need more, as the corners
// are cut away.
#define SIDE_MARGIN PBL_IF_ROUND_ELSE(22, 6)
#define LINE_GAP 3

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 2

// Values match the TIME_FORMAT options in src/pkjs/config.js.
typedef enum {
  TIME_FORMAT_WATCH,
  TIME_FORMAT_12H,
  TIME_FORMAT_24H,
} TimeFormat;

typedef struct {
  uint8_t version;
  // Black-and-white watches only; colour watches use background_argb.
  bool dark_theme;
  uint8_t accent_argb;
  char labels[CITY_COUNT][LABEL_LENGTH];
  // Minutes each city is ahead of UTC.
  int16_t offsets[CITY_COUNT];
  // Added in version 2.
  uint8_t time_format;
  uint8_t background_argb;
  // Colour of the times; automatic (black or white, whichever suits the
  // background) unless one is chosen.
  bool time_color_auto;
  uint8_t time_argb;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 1: return offsetof(Settings, time_format);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

typedef struct {
  GColor background;
  GColor foreground;
  GColor time;
  GColor detail;
  GColor accent;
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static GFont s_time_font;
static GFont s_date_font;
static GFont s_city_font;
static GFont s_city_time_font;
static GFont s_detail_font;

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .dark_theme = false,
    .accent_argb = GColorTiffanyBlueARGB8,
    .labels = { "London", "Tokyo" },
    .offsets = { 60, 540 },
    .time_format = TIME_FORMAT_WATCH,
    .background_argb = GColorWhiteARGB8,
    .time_color_auto = true,
    .time_argb = GColorBlackARGB8,
  };
}

#if defined(PBL_COLOR)
// Whether a colour is light enough to need dark text on it.
static bool is_light(GColor color) {
  // Each channel is 0 to 3; weighted roughly by how bright it looks.
  return color.r * 3 + color.g * 6 + color.b > 15;
}
#endif

static void apply_palette(void) {
#if defined(PBL_COLOR)
  s_palette.background = (GColor) { .argb = s_settings.background_argb };
  const bool light = is_light(s_palette.background);
  s_palette.foreground = light ? GColorBlack : GColorWhite;
  s_palette.detail = light ? GColorDarkGray : GColorLightGray;
  s_palette.accent = (GColor) { .argb = s_settings.accent_argb };
  s_palette.time = s_settings.time_color_auto
    ? s_palette.foreground
    : (GColor) { .argb = s_settings.time_argb };
#else
  const bool dark = s_settings.dark_theme;
  s_palette.background = dark ? GColorBlack : GColorWhite;
  s_palette.foreground = dark ? GColorWhite : GColorBlack;
  s_palette.detail = s_palette.foreground;
  s_palette.accent = s_palette.foreground;
  s_palette.time = s_palette.foreground;
#endif
}

// ---------------------------------------------------------------------------
// Time

// Whether to show 24-hour times: as chosen, or following the watch.
static bool use_24h(void) {
  switch (s_settings.time_format) {
    case TIME_FORMAT_12H: return false;
    case TIME_FORMAT_24H: return true;
    default: return clock_is_24h_style();
  }
}

// Minutes the watch's own time zone is ahead of UTC, worked out by comparing
// its local time with UTC.
static int32_t local_offset_minutes(time_t now) {
  struct tm local = *localtime(&now);
  struct tm utc = *gmtime(&now);
  int32_t days = local.tm_yday - utc.tm_yday;
  if (local.tm_year != utc.tm_year) {
    days = local.tm_year > utc.tm_year ? 1 : -1;
  }
  return days * 24 * 60 + (local.tm_hour - utc.tm_hour) * 60 + (local.tm_min - utc.tm_min);
}

static void format_time(const struct tm *t, char *buffer, size_t size) {
  if (use_24h()) {
    strftime(buffer, size, "%H:%M", t);
  } else {
    const int hour = t->tm_hour % 12;
    snprintf(buffer, size, "%d:%02d", hour ? hour : 12, t->tm_min);
  }
}

static void uppercase(char *text) {
  for (char *c = text; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
}

// How a city's time relates to the watch's: "Today" or, when it is already
// another day there, that day's name, and how many hours ahead or behind,
// e.g. "Today +5h", "Sun +13h" or "Fri -5h30".
static void format_detail(const struct tm *here, const struct tm *there, int32_t difference,
                          char *buffer, size_t size) {
  char day[8] = "Today";
  if (there->tm_yday != here->tm_yday || there->tm_year != here->tm_year) {
    strftime(day, sizeof(day), "%a", there);
  }

  const int32_t magnitude = difference < 0 ? -difference : difference;
  const char sign = difference < 0 ? '-' : '+';
  if (difference == 0) {
    snprintf(buffer, size, "%s, same time", day);
  } else if (magnitude % 60) {
    snprintf(buffer, size, "%s %c%dh%02d", day, sign, (int)(magnitude / 60),
             (int)(magnitude % 60));
  } else {
    snprintf(buffer, size, "%s %c%dh", day, sign, (int)(magnitude / 60));
  }
}

// ---------------------------------------------------------------------------
// Drawing

static int16_t text_width(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 300, 100),
                                               GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// Draws text so its visible top is at a given height.
static void draw_text(GContext *ctx, const char *text, GFont font, GColor color,
                      GRect box, int16_t pad, GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font,
                     GRect(box.origin.x, box.origin.y - pad, box.size.w, box.size.h + pad * 2),
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

// The watch's own time, large, with the day and date in a column beside it.
static void draw_local_time(GContext *ctx, GRect area, int16_t top, const struct tm *t) {
  char time_text[8];
  format_time(t, time_text, sizeof(time_text));
  char day[8];
  strftime(day, sizeof(day), "%a", t);
  uppercase(day);
  char date[12];
  char month[4];
  strftime(month, sizeof(month), "%b", t);
  uppercase(month);
  snprintf(date, sizeof(date), "%s %d", month, t->tm_mday);
  const char *meridiem = use_24h() ? NULL : t->tm_hour < 12 ? "AM" : "PM";

  const int16_t time_w = text_width(time_text, s_time_font);
  int16_t column_w = text_width(day, s_date_font);
  const int16_t date_w = text_width(date, s_date_font);
  column_w = date_w > column_w ? date_w : column_w;
  const int16_t gap = GAP;
  const int16_t left = area.origin.x + (area.size.w - time_w - gap - column_w) / 2;

  draw_text(ctx, time_text, s_time_font, s_palette.time,
            GRect(left, top, time_w + 2, TIME_HEIGHT), TIME_PAD, GTextAlignmentLeft);

  // The column's lines are centred on the time.
  const int lines = meridiem ? 3 : 2;
  const int16_t column_h = lines * DATE_HEIGHT + (lines - 1) * LINE_GAP;
  int16_t y = top + (TIME_HEIGHT - column_h) / 2;
  const int16_t x = left + time_w + gap;
  draw_text(ctx, day, s_date_font, s_palette.accent,
            GRect(x, y, column_w + 2, DATE_HEIGHT), DATE_PAD, GTextAlignmentLeft);
  y += DATE_HEIGHT + LINE_GAP;
  draw_text(ctx, date, s_date_font, s_palette.foreground,
            GRect(x, y, column_w + 2, DATE_HEIGHT), DATE_PAD, GTextAlignmentLeft);
  if (meridiem) {
    y += DATE_HEIGHT + LINE_GAP;
    draw_text(ctx, meridiem, s_date_font, s_palette.detail,
              GRect(x, y, column_w + 2, DATE_HEIGHT), DATE_PAD, GTextAlignmentLeft);
  }
}

// One city: its name with the day and difference below, and its time on the
// right.
static void draw_city(GContext *ctx, GRect area, int16_t top, int city, time_t now,
                      const struct tm *here, int32_t here_offset) {
  const int32_t offset = s_settings.offsets[city];
  const time_t shifted = now + offset * 60;
  const struct tm there = *gmtime(&shifted);

  char time_text[8];
  format_time(&there, time_text, sizeof(time_text));
  char detail[24];
  format_detail(here, &there, offset - here_offset, detail, sizeof(detail));

  const int16_t row_h = CITY_HEIGHT + LINE_GAP + DETAIL_HEIGHT;
  const int16_t time_w = text_width(time_text, s_city_time_font);
  const int16_t time_x = area.origin.x + area.size.w - time_w;
  const int16_t time_top = use_24h()
    ? top + (row_h - CITY_TIME_HEIGHT) / 2
    : top;
  draw_text(ctx, time_text, s_city_time_font, s_palette.time,
            GRect(time_x - 2, time_top, time_w + 4, CITY_TIME_HEIGHT), CITY_TIME_PAD,
            GTextAlignmentRight);
  if (!use_24h()) {
    draw_text(ctx, there.tm_hour < 12 ? "AM" : "PM", s_detail_font, s_palette.detail,
              GRect(time_x - 10, top + row_h - DETAIL_HEIGHT, time_w + 10, DETAIL_HEIGHT),
              DETAIL_PAD, GTextAlignmentRight);
  }

  const int16_t name_w = time_x - area.origin.x - GAP / 2;
  draw_text(ctx, s_settings.labels[city], s_city_font, s_palette.accent,
            GRect(area.origin.x, top, name_w, CITY_HEIGHT), CITY_PAD, GTextAlignmentLeft);
  draw_text(ctx, detail, s_detail_font, s_palette.detail,
            GRect(area.origin.x, top + CITY_HEIGHT + LINE_GAP, name_w, DETAIL_HEIGHT),
            DETAIL_PAD, GTextAlignmentLeft);
}

static void draw_divider(GContext *ctx, GRect area, int16_t y, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(area.origin.x, y), GPoint(area.origin.x + area.size.w - 1, y));
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // The unobstructed area shrinks when a Timeline Quick View is showing.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GRect area = grect_inset(bounds, GEdgeInsets(0, SIDE_MARGIN));
  const time_t now = time(NULL);
  const struct tm here = *localtime(&now);
  const int32_t here_offset = local_offset_minutes(now);

  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // Everything is stacked and centred vertically: the local time, a divider,
  // then each city with a fainter divider between them.
  const int16_t row_h = CITY_HEIGHT + LINE_GAP + DETAIL_HEIGHT;
  const int16_t total = TIME_HEIGHT + GAP * 2 + CITY_COUNT * row_h + (CITY_COUNT - 1) * GAP * 2;
  int16_t y = bounds.origin.y + (bounds.size.h - total) / 2;

  draw_local_time(ctx, area, y, &here);
  y += TIME_HEIGHT + GAP;
  draw_divider(ctx, area, y, s_palette.accent);
  y += GAP;
  for (int city = 0; city < CITY_COUNT; city++) {
    if (city > 0) {
      y += GAP;
      draw_divider(ctx, grect_inset(area, GEdgeInsets(0, area.size.w / 4)), y, s_palette.detail);
      y += GAP;
    }
    draw_city(ctx, area, y, city, now, &here, here_offset);
    y += row_h;
  }
}

// ---------------------------------------------------------------------------
// Events

static void request_offsets(void) {
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_OFFSETS, 1);
  app_message_outbox_send();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
  // Daylight saving changes on the hour, so check the offsets then.
  if (tick_time->tm_min == 0) {
    request_offsets();
  }
}

// Clay sends dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  static const uint32_t *label_keys[CITY_COUNT] = {
    &MESSAGE_KEY_CITY_1_LABEL, &MESSAGE_KEY_CITY_2_LABEL,
  };
  static const uint32_t *offset_keys[CITY_COUNT] = {
    &MESSAGE_KEY_CITY_1_OFFSET, &MESSAGE_KEY_CITY_2_OFFSET,
  };
  bool changed = false;
  const Tuple *tuple;

  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.dark_theme = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT))) {
    const int32_t format = tuple_int(tuple);
    s_settings.time_format = format >= TIME_FORMAT_WATCH && format <= TIME_FORMAT_24H
      ? format : TIME_FORMAT_WATCH;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BACKGROUND_COLOR))) {
    s_settings.background_argb = GColorFromHEX(tuple_int(tuple)).argb;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_COLOR))) {
    // -1 means automatic.
    const int32_t color = tuple_int(tuple);
    s_settings.time_color_auto = color < 0;
    if (color >= 0) {
      s_settings.time_argb = GColorFromHEX(color).argb;
    }
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_ACCENT_COLOR))) {
    s_settings.accent_argb = GColorFromHEX(tuple_int(tuple)).argb;
    changed = true;
  }
  for (int city = 0; city < CITY_COUNT; city++) {
    if ((tuple = dict_find(iterator, *label_keys[city])) && tuple->type == TUPLE_CSTRING) {
      strncpy(s_settings.labels[city], tuple->value->cstring, LABEL_LENGTH - 1);
      s_settings.labels[city][LABEL_LENGTH - 1] = '\0';
      changed = true;
    }
    if ((tuple = dict_find(iterator, *offset_keys[city]))) {
      s_settings.offsets[city] = tuple_int(tuple);
      changed = true;
    }
  }

  if (changed) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    window_set_background_color(s_window, s_palette.background);
    layer_mark_dirty(s_canvas);
  }
}

#if !defined(PBL_PLATFORM_APLITE)
static void unobstructed_change(AnimationProgress progress, void *context) {
  layer_mark_dirty(s_canvas);
}
#endif

// ---------------------------------------------------------------------------
// Lifecycle

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void load_settings(void) {
  settings_set_defaults(&s_settings);
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings saved;
    const int size = persist_read_data(PERSIST_KEY_SETTINGS, &saved, sizeof(saved));
    if (size == settings_size(saved.version)) {
      memcpy(&s_settings, &saved, size);
      s_settings.version = SETTINGS_VERSION;
#if defined(PBL_COLOR)
      // Before version 2, colour watches had a light or dark background.
      if (saved.version == 1 && saved.dark_theme) {
        s_settings.background_argb = GColorBlackARGB8;
      }
#endif
    }
  }
  apply_palette();
}

static void init(void) {
#if PBL_DISPLAY_WIDTH >= 200
  s_time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
#else
  s_time_font = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
#endif
  s_date_font = fonts_get_system_font(DATE_FONT);
  s_city_font = fonts_get_system_font(CITY_FONT);
  s_city_time_font = fonts_get_system_font(CITY_TIME_FONT);
  s_detail_font = fonts_get_system_font(DETAIL_FONT);
  load_settings();

  s_window = window_create();
  window_set_background_color(s_window, s_palette.background);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 32);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
