// Minimalist: a quiet watch face for the Pebble 2 Duo, analog or digital.
//
// Analog draws twelve hour marks around the edge of the screen and two hands
// in one of five shapes. Digital is laid out like Retro F-91W's LCD: a top row
// with the day and the month and date, and the time below across the width,
// with PM to its right in 12-hour time. Both styles can hide the date. Both show a
// name, PEBBLE unless the wearer changes or clears it, and all the text comes
// in one of four font styles (src/c/font_styles.h). The settings come from the
// phone (src/pkjs/config.js) and are kept on the watch.

#include <pebble.h>

#include "font_styles.h"

// ---------------------------------------------------------------- Settings

enum { STYLE_ANALOG = 0, STYLE_DIGITAL = 1 };
enum { FORMAT_WATCH = 0, FORMAT_12H = 1, FORMAT_24H = 2 };
enum { HANDS_BATON, HANDS_DAUPHINE, HANDS_SKELETON, HANDS_LOLLIPOP, HANDS_LEAF, HANDS_COUNT };

#define PERSIST_KEY_SETTINGS 1
#define SETTINGS_VERSION 4
// Version 1 ended before the name, version 2 before the numbers, and version 3
// before the font and hand styles.
#define SETTINGS_V1_SIZE offsetof(Settings, brand)
#define SETTINGS_V2_SIZE offsetof(Settings, numbers)
#define SETTINGS_V3_SIZE offsetof(Settings, font_style)
#define BRAND_LENGTH 16

// Saved with persist_write_data, so only append fields and bump the version.
typedef struct {
  uint8_t version;
  uint8_t style;
  bool dark;
  bool show_date;
  uint8_t time_format;
  bool leading_zero;
  char brand[BRAND_LENGTH + 1];  // Empty hides it.
  bool numbers;  // Numerals in place of the analog marks at 12, 3, 6 and 9.
  uint8_t font_style;
  uint8_t hand_style;
} Settings;

static Settings s_settings = {
  .version = SETTINGS_VERSION,
  .style = STYLE_ANALOG,
  .dark = true,
  .show_date = true,
  .time_format = FORMAT_WATCH,
  .leading_zero = true,
  .brand = "PEBBLE",
  .numbers = false,
  .font_style = FONT_STYLE_CONDENSED,
  .hand_style = HANDS_BATON,
};

// ---------------------------------------------------------------- Layout

// Hour marks sit this far in from the screen edge.
#define MARK_INSET 5
#define MARK_LONG 12   // 12, 3, 6 and 9
// Or, in their place, numerals with their outer edge at MARK_INSET.
#define MARK_SHORT 7
#define MARK_LONG_WIDTH 3
#define MARK_SHORT_WIDTH 1

#define HOUR_HAND_LENGTH 36
#define MINUTE_HAND_LENGTH 54
#define HUB_RADIUS 4
// Baton: lines this wide.
#define HOUR_BATON_WIDTH 5
#define MINUTE_BATON_WIDTH 3
// Dauphine, skeleton and leaf: half the width at the widest point.
#define HOUR_HALF_WIDTH 5
#define MINUTE_HALF_WIDTH 4
#define SKELETON_HOUR_HALF_WIDTH 3
#define SKELETON_MINUTE_HALF_WIDTH 2
// How far the shapes reach back past the centre.
#define HAND_TAIL 5
// Lollipop: a line this wide, ending in a ring of this radius at the tip.
#define HOUR_LOLLIPOP_WIDTH 3
#define MINUTE_LOLLIPOP_WIDTH 1
#define HOUR_RING_RADIUS 5
#define MINUTE_RING_RADIUS 4
#define RING_THICKNESS 2

// The bottom of the name's capitals sits this far above the centre on the
// analog dial, and the top of the date's this far below it.
#define ANALOG_BRAND_OFFSET 31
#define ANALOG_DATE_OFFSET 31

// The name: BRAND_GAP above the digital top row, and BRAND_MARGIN in from the
// screen's sides.
#define BRAND_GAP 8
#define BRAND_MARGIN 8

// Digital, after Retro F-91W's LCD: the top row, ROW_GAP above the time, has
// the day at the left and the month and date at the right. In 12-hour time,
// PM sits PM_GAP to the right of the time, on its baseline, and the time is
// set smaller to leave room for it (ROLE_TIME_12H). The sizes and offsets of
// the text for each font style are in src/c/font_styles.h, and PM_GAP matches
// tools/make_fonts.py.
#define ROW_GAP 10
#define PM_GAP 3
// Moves the date left so its last digit lines up with the time's; measured in
// the emulator, and the same for all four font styles. In 12-hour time the
// date lines up with PM, which needs no nudge.
#define DATE_NUDGE 2

// ---------------------------------------------------------------- State

static Window *s_window;
static Layer *s_canvas;
static struct tm s_now;

// The fonts in use, one per piece of text (ROLE_* in font_styles.h), with where
// their visible text starts in the text box and how tall it is.
typedef struct {
  GFont font;
  int16_t top;
  int16_t height;
  bool owned;  // Loaded for this role, so unloaded with it.
} Text;
static Text s_text[ROLE_COUNT];
// Width of the cell each digit, and the colon, of the time is drawn in. Every
// digit gets the widest digit's width, so the colon never moves. One each for
// the 24-hour time and the smaller 12-hour time.
static int16_t s_digit_w[2];
static int16_t s_colon_w[2];
static int16_t s_colon_box[2];  // The colon's own width, which may be wider.
static int16_t s_pm_w;

static const char *const DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *const MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static GColor foreground(void) {
  return s_settings.dark ? GColorWhite : GColorBlack;
}

static GColor background(void) {
  return s_settings.dark ? GColorBlack : GColorWhite;
}

// ---------------------------------------------------------------- Drawing

// The point at `distance` from `centre` along `angle` (TRIG_MAX_ANGLE is a
// full turn, 0 is 12 o'clock).
static GPoint along(GPoint centre, int32_t angle, int32_t distance) {
  return GPoint(centre.x + sin_lookup(angle) * distance / TRIG_MAX_RATIO,
                centre.y - cos_lookup(angle) * distance / TRIG_MAX_RATIO);
}

// How far a ray from the centre at `angle` travels before reaching the edge
// of `bounds` inset by MARK_INSET, so the marks follow the rectangle.
static int32_t distance_to_edge(GRect bounds, int32_t angle) {
  const int32_t half_w = bounds.size.w / 2 - MARK_INSET;
  const int32_t half_h = bounds.size.h / 2 - MARK_INSET;
  const int32_t dx = abs(sin_lookup(angle));
  const int32_t dy = abs(cos_lookup(angle));
  int32_t distance = INT32_MAX;
  if (dx > 0) {
    distance = half_w * TRIG_MAX_RATIO / dx;
  }
  if (dy > 0) {
    const int32_t vertical = half_h * TRIG_MAX_RATIO / dy;
    if (vertical < distance) {
      distance = vertical;
    }
  }
  return distance;
}

// Draws `text` in the font for `role` with its visible text starting at `top`.
static void draw_text_at(GContext *ctx, const char *text, int role, int16_t x, int16_t top,
                         int16_t w, GTextAlignment alignment) {
  const Text *t = &s_text[role];
  graphics_draw_text(ctx, text, t->font, GRect(x, top - t->top, w, t->top + t->height + 20),
                     GTextOverflowModeFill, alignment, NULL);
}

// The name, centred on one line with its capitals starting at `top`; a name
// too long for the screen ends in an ellipsis.
static void draw_brand(GContext *ctx, GRect bounds, int16_t top) {
  const Text *t = &s_text[ROLE_SMALL];
  graphics_context_set_text_color(ctx, foreground());
  graphics_draw_text(ctx, s_settings.brand, t->font,
                     GRect(BRAND_MARGIN, top - t->top, bounds.size.w - BRAND_MARGIN * 2,
                           t->top + t->height + 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Fills a hand's outline, given pointing at 12 with the centre at (0, 0), or
// with `hollow` draws just the outline over the background.
static void draw_shape(GContext *ctx, GPoint *points, uint32_t count, GPoint centre,
                       int32_t angle, bool hollow) {
  GPath path = {
    .num_points = count,
    .points = points,
    .rotation = angle,
    .offset = centre,
  };
  graphics_context_set_fill_color(ctx, hollow ? background() : foreground());
  gpath_draw_filled(ctx, &path);
  if (hollow) {
    graphics_context_set_stroke_color(ctx, foreground());
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_outline(ctx, &path);
  }
}

static void draw_hand(GContext *ctx, GPoint centre, int32_t angle, bool minute) {
  const int16_t length = minute ? MINUTE_HAND_LENGTH : HOUR_HAND_LENGTH;
  const int16_t half = minute ? MINUTE_HALF_WIDTH : HOUR_HALF_WIDTH;
  graphics_context_set_stroke_color(ctx, foreground());
  switch (s_settings.hand_style) {
    case HANDS_DAUPHINE: {
      // Widest at the centre, tapering to a point.
      GPoint points[] = {{-half, 0}, {0, -length}, {half, 0}, {0, HAND_TAIL}};
      draw_shape(ctx, points, ARRAY_LENGTH(points), centre, angle, false);
      break;
    }
    case HANDS_SKELETON: {
      // An open outline with a pointed tip.
      const int16_t w = minute ? SKELETON_MINUTE_HALF_WIDTH : SKELETON_HOUR_HALF_WIDTH;
      GPoint points[] = {{-w, HAND_TAIL}, {-w, -length + w * 2}, {0, -length},
                         {w, -length + w * 2}, {w, HAND_TAIL}};
      draw_shape(ctx, points, ARRAY_LENGTH(points), centre, angle, true);
      break;
    }
    case HANDS_LOLLIPOP: {
      // A thin line ending in a ring at the tip.
      const int16_t radius = minute ? MINUTE_RING_RADIUS : HOUR_RING_RADIUS;
      const GPoint ring = along(centre, angle, length - radius);
      graphics_context_set_stroke_width(ctx, minute ? MINUTE_LOLLIPOP_WIDTH : HOUR_LOLLIPOP_WIDTH);
      graphics_draw_line(ctx, centre, along(centre, angle, length - radius * 2));
      graphics_context_set_fill_color(ctx, foreground());
      graphics_fill_circle(ctx, ring, radius);
      graphics_context_set_fill_color(ctx, background());
      graphics_fill_circle(ctx, ring, radius - RING_THICKNESS);
      break;
    }
    case HANDS_LEAF: {
      // Widest halfway along, pointed at both ends.
      GPoint points[] = {{0, HAND_TAIL}, {-half, -length / 2}, {0, -length}, {half, -length / 2}};
      draw_shape(ctx, points, ARRAY_LENGTH(points), centre, angle, false);
      break;
    }
    default:
      // Baton: a plain line.
      graphics_context_set_stroke_width(ctx, minute ? MINUTE_BATON_WIDTH : HOUR_BATON_WIDTH);
      graphics_draw_line(ctx, centre, along(centre, angle, length));
      break;
  }
}

static void draw_analog(GContext *ctx, GRect bounds) {
  const GPoint centre = grect_center_point(&bounds);
  graphics_context_set_stroke_color(ctx, foreground());

  for (int hour = 0; hour < 12; hour++) {
    const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
    const bool cardinal = hour % 3 == 0;
    if (cardinal && s_settings.numbers) {
      continue;
    }
    const int32_t outer = distance_to_edge(bounds, angle);
    const int32_t length = cardinal ? MARK_LONG : MARK_SHORT;
    graphics_context_set_stroke_width(ctx, cardinal ? MARK_LONG_WIDTH : MARK_SHORT_WIDTH);
    graphics_draw_line(ctx, along(centre, angle, outer - length), along(centre, angle, outer));
  }

  graphics_context_set_text_color(ctx, foreground());
  if (s_settings.numbers) {
    const int16_t w = bounds.size.w;
    const int16_t h = bounds.size.h;
    const int16_t height = s_text[ROLE_NUMERALS].height;
    const int16_t middle = centre.y - height / 2;
    draw_text_at(ctx, "12", ROLE_NUMERALS, 0, MARK_INSET, w, GTextAlignmentCenter);
    draw_text_at(ctx, "3", ROLE_NUMERALS, 0, middle, w - MARK_INSET, GTextAlignmentRight);
    draw_text_at(ctx, "6", ROLE_NUMERALS, 0, h - MARK_INSET - height, w, GTextAlignmentCenter);
    draw_text_at(ctx, "9", ROLE_NUMERALS, MARK_INSET, middle, w, GTextAlignmentLeft);
  }

  if (s_settings.brand[0]) {
    draw_brand(ctx, bounds, centre.y - ANALOG_BRAND_OFFSET - s_text[ROLE_SMALL].height);
  }
  if (s_settings.show_date) {
    char date[8];
    snprintf(date, sizeof(date), "%s %d", DAYS[s_now.tm_wday], s_now.tm_mday);
    draw_text_at(ctx, date, ROLE_DATE, 0, centre.y + ANALOG_DATE_OFFSET, bounds.size.w,
                 GTextAlignmentCenter);
  }

  const int32_t minute_angle = TRIG_MAX_ANGLE * s_now.tm_min / 60;
  const int32_t hour_angle =
      TRIG_MAX_ANGLE * ((s_now.tm_hour % 12) * 60 + s_now.tm_min) / (12 * 60);
  draw_hand(ctx, centre, hour_angle, false);
  draw_hand(ctx, centre, minute_angle, true);

  graphics_context_set_fill_color(ctx, foreground());
  graphics_fill_circle(ctx, centre, HUB_RADIUS);
  graphics_context_set_fill_color(ctx, background());
  graphics_fill_circle(ctx, centre, 1);
}

static bool use_24h(void) {
  switch (s_settings.time_format) {
    case FORMAT_12H: return false;
    case FORMAT_24H: return true;
    default: return clock_is_24h_style();
  }
}

static void draw_digital(GContext *ctx, GRect bounds) {
  const bool twenty_four = use_24h();
  int hour = s_now.tm_hour;
  if (!twenty_four) {
    hour = hour % 12 ? hour % 12 : 12;
  }
  char digits[5];
  snprintf(digits, sizeof(digits), "%02d%02d", hour, s_now.tm_min);
  // Without the leading zero the first place is left blank, as on an LCD.
  const bool blank_first = !s_settings.leading_zero && digits[0] == '0';

  // The time, with PM's place to its right in 12-hour time (kept in the
  // morning too, so the time stays put at noon), sets the edges the top row
  // lines up with.
  const int t = twenty_four ? 0 : 1;
  const int time_role = twenty_four ? ROLE_TIME : ROLE_TIME_12H;
  const int16_t time_w = s_digit_w[t] * 4 + s_colon_w[t];
  const int16_t width = time_w + (twenty_four ? 0 : PM_GAP + s_pm_w);
  const int16_t left = (bounds.size.w - width) / 2;

  // From the top: the name, the top row and the time, centred together.
  const bool brand = s_settings.brand[0] != '\0';
  const int16_t brand_h = s_text[ROLE_SMALL].height;
  const int16_t pm_h = s_text[ROLE_SMALL].height;
  const int16_t time_h = s_text[time_role].height;
  const int16_t row_h = s_settings.show_date ? s_text[ROLE_ROW].height : 0;
  const int16_t block = (brand ? brand_h + BRAND_GAP : 0) + (row_h ? row_h + ROW_GAP : 0) + time_h;
  const int16_t top = (bounds.size.h - block) / 2;
  const int16_t row_top = top + (brand ? brand_h + BRAND_GAP : 0);
  const int16_t time_top = row_top + (row_h ? row_h + ROW_GAP : 0);

  if (brand) {
    draw_brand(ctx, bounds, top);
  }

  graphics_context_set_text_color(ctx, foreground());

  // The top row: the day at the left, and the month and date at the right.
  if (s_settings.show_date) {
    char date[8];
    snprintf(date, sizeof(date), "%s %d", MONTHS[s_now.tm_mon], s_now.tm_mday);
    draw_text_at(ctx, DAYS[s_now.tm_wday], ROLE_ROW, left, row_top, width, GTextAlignmentLeft);
    draw_text_at(ctx, date, ROLE_ROW, left, row_top, width - (twenty_four ? DATE_NUDGE : 0),
                 GTextAlignmentRight);
  }

  // PM, to the right of the time with its bottom on the time's.
  if (!twenty_four && s_now.tm_hour >= 12) {
    draw_text_at(ctx, "PM", ROLE_SMALL, left + time_w + PM_GAP, time_top + time_h - pm_h,
                 s_pm_w + PM_GAP, GTextAlignmentLeft);
  }

  // The time, one cell per character.
  int16_t x = left;
  for (int i = 0; i < 4; i++) {
    if (i != 0 || !blank_first) {
      const char digit[2] = {digits[i], '\0'};
      draw_text_at(ctx, digit, time_role, x, time_top, s_digit_w[t], GTextAlignmentCenter);
    }
    x += s_digit_w[t];
    if (i == 1) {
      // Centred on its cell, in a box as wide as the colon itself.
      draw_text_at(ctx, ":", time_role, x + (s_colon_w[t] - s_colon_box[t]) / 2, time_top,
                   s_colon_box[t], GTextAlignmentCenter);
      x += s_colon_w[t];
    }
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, background());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, false);

  if (s_settings.style == STYLE_DIGITAL) {
    draw_digital(ctx, bounds);
  } else {
    draw_analog(ctx, bounds);
  }
}

// ---------------------------------------------------------------- Fonts

static void unload_fonts(void) {
  for (int role = 0; role < ROLE_COUNT; role++) {
    if (s_text[role].owned) {
      fonts_unload_custom_font(s_text[role].font);
    }
    s_text[role].owned = false;
  }
}

static int16_t text_width(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 200, 100),
                                               GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// Loads the fonts of the chosen style, sharing one between pieces of text
// that use the same font.
static void load_fonts(void) {
  unload_fonts();
  const FontStyle *style = &FONT_STYLES[s_settings.font_style];
  for (int role = 0; role < ROLE_COUNT; role++) {
    const FontSpec *spec = &style->roles[role];
    Text *t = &s_text[role];
    t->top = spec->top;
    t->height = spec->height;
    t->owned = false;
    if (!spec->resource) {
      t->font = fonts_get_system_font(spec->system);
      continue;
    }
    int shared = -1;
    for (int earlier = 0; earlier < role; earlier++) {
      if (style->roles[earlier].resource == spec->resource) {
        shared = earlier;
        break;
      }
    }
    if (shared >= 0) {
      t->font = s_text[shared].font;
    } else {
      t->font = fonts_load_custom_font(resource_get_handle(spec->resource));
      t->owned = true;
    }
  }

  for (int t = 0; t < 2; t++) {
    const GFont time_font = s_text[t ? ROLE_TIME_12H : ROLE_TIME].font;
    s_digit_w[t] = 0;
    for (char d = '0'; d <= '9'; d++) {
      const char digit[2] = {d, '\0'};
      const int16_t w = text_width(digit, time_font);
      if (w > s_digit_w[t]) {
        s_digit_w[t] = w;
      }
    }
    s_colon_w[t] = style->colon_w[t];
    s_colon_box[t] = text_width(":", time_font);
    if (s_colon_box[t] < s_colon_w[t]) {
      s_colon_box[t] = s_colon_w[t];
    }
  }
  s_pm_w = text_width("PM", s_text[ROLE_SMALL].font);
}

// ---------------------------------------------------------------- Events

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_canvas);
}

static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_STYLE))) {
    s_settings.style = tuple_int(tuple) == STYLE_DIGITAL ? STYLE_DIGITAL : STYLE_ANALOG;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.dark = tuple_int(tuple) == 0;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_DATE))) {
    s_settings.show_date = tuple_int(tuple) != 0;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT))) {
    const int32_t format = tuple_int(tuple);
    s_settings.time_format = (format >= FORMAT_WATCH && format <= FORMAT_24H) ? format : FORMAT_WATCH;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_LEADING_ZERO))) {
    s_settings.leading_zero = tuple_int(tuple) != 0;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NUMBERS))) {
    s_settings.numbers = tuple_int(tuple) != 0;
  }
  const uint8_t font_style = s_settings.font_style;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_FONT_STYLE))) {
    const int32_t style = tuple_int(tuple);
    s_settings.font_style = (style >= 0 && style < FONT_STYLE_COUNT) ? style : FONT_STYLE_CONDENSED;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_HAND_STYLE))) {
    const int32_t style = tuple_int(tuple);
    s_settings.hand_style = (style >= 0 && style < HANDS_COUNT) ? style : HANDS_BATON;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BRAND)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_settings.brand, tuple->value->cstring, BRAND_LENGTH);
    s_settings.brand[BRAND_LENGTH] = '\0';
  }
  persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
  if (s_settings.font_style != font_style) {
    load_fonts();
  }
  layer_mark_dirty(s_canvas);
}

static void load_settings(void) {
  if (!persist_exists(PERSIST_KEY_SETTINGS)) {
    return;
  }
  Settings saved;
  const int size = persist_read_data(PERSIST_KEY_SETTINGS, &saved, sizeof(saved));
  // Older versions keep their choices, with the defaults for what they lack.
  if ((saved.version == SETTINGS_VERSION && size == (int)sizeof(saved)) ||
      (saved.version == 3 && size == (int)SETTINGS_V3_SIZE) ||
      (saved.version == 2 && size == (int)SETTINGS_V2_SIZE) ||
      (saved.version == 1 && size == (int)SETTINGS_V1_SIZE)) {
    memcpy(&s_settings, &saved, size);
    s_settings.version = SETTINGS_VERSION;
    s_settings.brand[BRAND_LENGTH] = '\0';
    if (s_settings.font_style >= FONT_STYLE_COUNT) {
      s_settings.font_style = FONT_STYLE_CONDENSED;
    }
    if (s_settings.hand_style >= HANDS_COUNT) {
      s_settings.hand_style = HANDS_BATON;
    }
  }
}

// ---------------------------------------------------------------- Lifecycle

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  load_settings();

  load_fonts();

  const time_t now = time(NULL);
  s_now = *localtime(&now);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 16);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  unload_fonts();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
