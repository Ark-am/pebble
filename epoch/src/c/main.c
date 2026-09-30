#include <pebble.h>

// A classic analog watchface: a minute track that follows the edge of the
// screen, hour numbers and indices, and tapered hands. The weather and health
// data sit above the centre, the date and a battery ring below it; tap the
// watch to cycle the health metric. The background, the style of the hour
// numbers and the other options are configurable from the phone (see src/pkjs).

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_METRIC 2
#define PERSIST_KEY_TEMPERATURE 3
#define PERSIST_KEY_CONDITION 4

#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define MODERN_FONT FONT_KEY_GOTHIC_28_BOLD
  #define DATE_FONT FONT_KEY_GOTHIC_24_BOLD
  // Visible glyph heights and the blank space fonts leave above them.
  #define MODERN_HEIGHT 20
  #define MODERN_PAD 9
  #define DATE_HEIGHT 17
  #define DATE_PAD 7
  #define LABEL_FONT FONT_KEY_GOTHIC_18
  #define LABEL_HEIGHT 13
  #define LABEL_PAD 5
  #define TEXT_BOX_WIDTH 80
  #define CLASSIC_HEIGHT 20
  #define CLASSIC_STROKE 3
  #define MINUTE_TICK_LENGTH 6
  #define HOUR_TICK_LENGTH 9
  #define HOUR_HAND_WIDTH 4
  #define MINUTE_HAND_WIDTH 3
  #define LEAF_RADIUS 7
  #define LEAF_OUTLINE 2
  #define RING_RADIUS 15
#else
  #define MODERN_FONT FONT_KEY_GOTHIC_24_BOLD
  #define DATE_FONT FONT_KEY_GOTHIC_18_BOLD
  #define MODERN_HEIGHT 17
  #define MODERN_PAD 7
  #define DATE_HEIGHT 13
  #define DATE_PAD 5
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define LABEL_HEIGHT 10
  #define LABEL_PAD 4
  #define TEXT_BOX_WIDTH 56
  #define CLASSIC_HEIGHT 15
  #define CLASSIC_STROKE 2
  #define MINUTE_TICK_LENGTH 4
  #define HOUR_TICK_LENGTH 7
  #define HOUR_HAND_WIDTH 3
  #define MINUTE_HAND_WIDTH 2
  #define LEAF_RADIUS 4
  #define LEAF_OUTLINE 1
  #define RING_RADIUS 11
#endif

#define EDGE_INSET 2
#define NUMERAL_GAP 6
#define LINE_GAP 2
#define DIAL_NAME "Pebble"
#define RING_SEGMENTS 10

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 2
typedef struct {
  uint8_t version;
  bool dark;
  bool modern_numerals;
  bool disconnect_vibe;
} Settings;

static Settings s_settings;

typedef struct {
  GColor background;
  GColor foreground;
  GColor leaf;
  GColor ring;
  GColor connected;
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static GFont s_modern_font;
static GFont s_date_font;
static GFont s_label_font;

static BatteryChargeState s_battery;
static bool s_bluetooth_connected;
static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[16];

// ---------------------------------------------------------------------------
// Health metrics

#if defined(PBL_HEALTH)
typedef enum {
  METRIC_STEPS,
  METRIC_DISTANCE,
  METRIC_CALORIES,
  METRIC_ACTIVE,
  METRIC_SLEEP,
  METRIC_HEART_RATE,
  METRIC_COUNT,
} Metric;

static Metric s_metric;

static bool metric_available(Metric metric) {
  const time_t now = time(NULL);
  HealthMetric health_metric;
  time_t start = time_start_of_today();
  switch (metric) {
    case METRIC_STEPS: health_metric = HealthMetricStepCount; break;
    case METRIC_DISTANCE: health_metric = HealthMetricWalkedDistanceMeters; break;
    case METRIC_CALORIES: health_metric = HealthMetricActiveKCalories; break;
    case METRIC_ACTIVE: health_metric = HealthMetricActiveSeconds; break;
    case METRIC_SLEEP: health_metric = HealthMetricSleepSeconds; break;
    case METRIC_HEART_RATE:
      // Only watches with a heart-rate sensor report this.
      health_metric = HealthMetricHeartRateBPM;
      start = now;
      break;
    default:
      return false;
  }
  return health_service_metric_accessible(health_metric, start, now)
    & HealthServiceAccessibilityMaskAvailable;
}

// Writes a whole number with thousands separators, e.g. 12,345.
static void format_thousands(char *buffer, size_t size, int32_t value) {
  if (value >= 1000) {
    snprintf(buffer, size, "%d,%03d", (int)(value / 1000), (int)(value % 1000));
  } else {
    snprintf(buffer, size, "%d", (int)value);
  }
}

// Fills in the value and label for a metric. Pebble's printf has no floating
// point, so decimals are built from integers.
static void format_metric(Metric metric, char *value, size_t size, const char **label) {
  switch (metric) {
    case METRIC_STEPS:
      format_thousands(value, size, health_service_sum_today(HealthMetricStepCount));
      *label = "STEPS";
      break;
    case METRIC_DISTANCE: {
      const int32_t meters = health_service_sum_today(HealthMetricWalkedDistanceMeters);
      const bool imperial = health_service_get_measurement_system_for_display(
        HealthMetricWalkedDistanceMeters) == MeasurementSystemImperial;
      // Tenths of a kilometre or mile.
      const int32_t tenths = imperial ? meters * 10 / 1609 : meters / 100;
      snprintf(value, size, "%d.%d", (int)(tenths / 10), (int)(tenths % 10));
      *label = imperial ? "MILES" : "KM";
      break;
    }
    case METRIC_CALORIES:
      format_thousands(value, size, health_service_sum_today(HealthMetricActiveKCalories));
      *label = "KCAL";
      break;
    case METRIC_ACTIVE:
      snprintf(value, size, "%dm", (int)(health_service_sum_today(HealthMetricActiveSeconds) / 60));
      *label = "ACTIVE";
      break;
    case METRIC_SLEEP: {
      const int32_t minutes = health_service_sum_today(HealthMetricSleepSeconds) / 60;
      snprintf(value, size, "%dh %02dm", (int)(minutes / 60), (int)(minutes % 60));
      *label = "SLEEP";
      break;
    }
    case METRIC_HEART_RATE:
      snprintf(value, size, "%d",
               (int)health_service_peek_current_value(HealthMetricHeartRateBPM));
      *label = "BPM";
      break;
    default:
      value[0] = '\0';
      *label = "";
      break;
  }
}

// Steps are always shown if nothing else is available.
static Metric next_available_metric(Metric from) {
  for (int i = 1; i <= METRIC_COUNT; i++) {
    const Metric candidate = (from + i) % METRIC_COUNT;
    if (metric_available(candidate)) {
      return candidate;
    }
  }
  return METRIC_STEPS;
}

static void health_handler(HealthEventType event, void *context) {
  if (event != HealthEventSleepUpdate || s_metric == METRIC_SLEEP) {
    layer_mark_dirty(s_canvas);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  s_metric = next_available_metric(s_metric);
  persist_write_int(PERSIST_KEY_METRIC, s_metric);
  layer_mark_dirty(s_canvas);
}
#endif

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .dark = false,
    .modern_numerals = false,
    .disconnect_vibe = true,
  };
}

static void apply_palette(void) {
  const bool dark = s_settings.dark;
  s_palette.background = dark ? GColorBlack : GColorWhite;
  s_palette.foreground = dark ? GColorWhite : GColorBlack;
#if defined(PBL_COLOR)
  s_palette.leaf = dark ? GColorDarkGray : GColorLightGray;
  s_palette.ring = GColorMintGreen;
  s_palette.connected = GColorPictonBlue;
#else
  s_palette.leaf = s_palette.background;
  s_palette.ring = s_palette.foreground;
  s_palette.connected = s_palette.foreground;
#endif
}

// ---------------------------------------------------------------------------
// Geometry

static GPoint ray_point(GPoint centre, int32_t angle, int32_t distance) {
  return GPoint(
    centre.x + sin_lookup(angle) * distance / TRIG_MAX_RATIO,
    centre.y - cos_lookup(angle) * distance / TRIG_MAX_RATIO
  );
}

// Distance from the centre to the screen edge along an angle. Round screens
// use a circle; rectangular ones follow the display's edge so the minute
// track fills the whole face.
static int32_t edge_distance(GRect bounds, int32_t angle) {
  const int32_t half_w = bounds.size.w / 2 - EDGE_INSET;
  const int32_t half_h = bounds.size.h / 2 - EDGE_INSET;
#if defined(PBL_ROUND)
  (void)angle;
  return half_w < half_h ? half_w : half_h;
#else
  const int32_t sin_abs = abs(sin_lookup(angle));
  const int32_t cos_abs = abs(cos_lookup(angle));
  const int32_t to_side = sin_abs ? half_w * TRIG_MAX_RATIO / sin_abs : INT32_MAX;
  const int32_t to_top = cos_abs ? half_h * TRIG_MAX_RATIO / cos_abs : INT32_MAX;
  return to_side < to_top ? to_side : to_top;
#endif
}

// ---------------------------------------------------------------------------
// Classic numerals
//
// The system fonts cannot be rotated, so the classic numerals are drawn as
// strokes. Each glyph is a list of x, y points on a 16 x 32 grid; PEN_UP
// starts a new stroke and GLYPH_END finishes the glyph.

#define GLYPH_WIDTH 16
#define GLYPH_HEIGHT 32
#define GLYPH_GAP 3
#define PEN_UP -1
#define GLYPH_END -2

static const int8_t GLYPH_0[] = {
  8, 0, 12, 2, 14, 8, 15, 16, 14, 24, 12, 30, 8, 32, 4, 30, 2, 24, 1, 16, 2, 8, 4, 2, 8, 0,
  GLYPH_END,
};
static const int8_t GLYPH_1[] = {
  4, 5, 9, 0, 9, 32, PEN_UP, 4, 32, 14, 32,
  GLYPH_END,
};
static const int8_t GLYPH_2[] = {
  1, 7, 3, 2, 7, 0, 11, 1, 14, 5, 14, 10, 11, 16, 1, 32, 15, 32, 15, 28,
  GLYPH_END,
};
static const int8_t GLYPH_4[] = {
  12, 32, 12, 0, 0, 22, 16, 22, PEN_UP, 8, 32, 16, 32,
  GLYPH_END,
};
static const int8_t GLYPH_6[] = {
  13, 3, 10, 0, 6, 1, 3, 5, 1, 12, 1, 20, 2, 27, 5, 31, 8, 32, 11, 31, 14, 27, 15, 22,
  14, 17, 11, 14, 8, 13, 5, 14, 2, 17, 1, 21,
  GLYPH_END,
};
static const int8_t GLYPH_8[] = {
  8, 14, 4, 12, 2, 8, 2, 4, 4, 1, 8, 0, 12, 1, 14, 4, 14, 8, 12, 12, 8, 14,
  3, 16, 1, 20, 1, 26, 3, 30, 8, 32, 13, 30, 15, 26, 15, 20, 13, 16, 8, 14,
  GLYPH_END,
};

// Only the digits of 12, 2, 4, 6, 8 and 10 are needed.
static const int8_t *const GLYPHS[10] = {
  GLYPH_0, GLYPH_1, GLYPH_2, NULL, GLYPH_4, NULL, GLYPH_6, NULL, GLYPH_8, NULL,
};

// Draws a number centred on a point, turned clockwise by an angle.
static void draw_classic_numeral(GContext *ctx, const char *text, GPoint at, int32_t rotation) {
  const int length = strlen(text);
  const int32_t total_width = length * GLYPH_WIDTH + (length - 1) * GLYPH_GAP;
  const int32_t sin = sin_lookup(rotation);
  const int32_t cos = cos_lookup(rotation);
  const int32_t scale = GLYPH_HEIGHT * TRIG_MAX_RATIO;

  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, CLASSIC_STROKE);
  for (int i = 0; i < length; i++) {
    const int8_t *p = GLYPHS[text[i] - '0'];
    if (!p) {
      continue;
    }
    const int32_t left = i * (GLYPH_WIDTH + GLYPH_GAP) - total_width / 2;
    bool pen_down = false;
    GPoint last = at;
    while (*p != GLYPH_END) {
      if (*p == PEN_UP) {
        pen_down = false;
        p++;
        continue;
      }
      // The point relative to the number's centre, scaled to pixels.
      const int32_t x = (left + p[0]) * CLASSIC_HEIGHT;
      const int32_t y = (p[1] - GLYPH_HEIGHT / 2) * CLASSIC_HEIGHT;
      const GPoint point = GPoint(at.x + (x * cos - y * sin) / scale,
                                  at.y + (x * sin + y * cos) / scale);
      if (pen_down) {
        graphics_draw_line(ctx, last, point);
      }
      last = point;
      pen_down = true;
      p += 2;
    }
  }
}

// ---------------------------------------------------------------------------
// Drawing

static void draw_ticks(GContext *ctx, GRect bounds, GPoint centre) {
  for (int i = 0; i < 60; i++) {
    const bool hour = i % 5 == 0;
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    const int32_t outer = edge_distance(bounds, angle);
    const int32_t length = hour ? HOUR_TICK_LENGTH : MINUTE_TICK_LENGTH;
    graphics_context_set_stroke_color(ctx, s_palette.foreground);
    graphics_context_set_stroke_width(ctx, hour ? 3 : 1);
    graphics_draw_line(ctx, ray_point(centre, angle, outer - length),
                       ray_point(centre, angle, outer));
  }
}

static bool hour_has_numeral(int hour) {
  return s_settings.modern_numerals ? hour % 3 == 0 : hour % 2 == 0;
}

static void draw_modern_numeral(GContext *ctx, const char *text, GPoint at) {
  graphics_context_set_text_color(ctx, s_palette.foreground);
  graphics_draw_text(ctx, text, s_modern_font,
                     GRect(at.x - 30, at.y - MODERN_HEIGHT / 2 - MODERN_PAD, 60,
                           MODERN_HEIGHT + MODERN_PAD * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Hour numbers sit just inside the ticks, with a long index at every other
// hour.
static void draw_hours(GContext *ctx, GRect bounds, GPoint centre) {
  for (int hour = 0; hour < 12; hour++) {
    const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
    const int32_t edge = edge_distance(bounds, angle);
    if (!hour_has_numeral(hour)) {
      graphics_context_set_stroke_color(ctx, s_palette.foreground);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_line(ctx, ray_point(centre, angle, edge * 64 / 100),
                         ray_point(centre, angle, edge * 82 / 100));
      continue;
    }

    char text[3];
    snprintf(text, sizeof(text), "%d", hour == 0 ? 12 : hour);
    if (s_settings.modern_numerals) {
      // Numbers at 3 and 9 reach the ticks with their width, not their height.
      int32_t reach = MODERN_HEIGHT / 2;
      if (hour % 6 != 0) {
        reach = graphics_text_layout_get_content_size(
          text, s_modern_font, GRect(0, 0, 60, MODERN_HEIGHT + MODERN_PAD * 2),
          GTextOverflowModeFill, GTextAlignmentCenter).w / 2;
      }
      draw_modern_numeral(ctx, text,
                          ray_point(centre, angle, edge - HOUR_TICK_LENGTH - NUMERAL_GAP - reach));
    } else {
      // The tops of the numbers face outward, except on the lower half of the
      // dial, where that would turn them upside down.
      const bool lower = hour > 3 && hour < 9;
      const int32_t rotation = lower ? angle - TRIG_MAX_ANGLE / 2 : angle;
      const GPoint at = ray_point(centre, angle,
                                  edge - HOUR_TICK_LENGTH - NUMERAL_GAP - CLASSIC_HEIGHT / 2);
      draw_classic_numeral(ctx, text, at, rotation);
    }
  }
}

static void draw_text_line(GContext *ctx, const char *text, GFont font, GPoint centre,
                           int16_t visible_top, int16_t pad, int16_t height) {
  graphics_context_set_text_color(ctx, s_palette.foreground);
  graphics_draw_text(ctx, text, font,
                     GRect(centre.x - TEXT_BOX_WIDTH / 2, visible_top - pad, TEXT_BOX_WIDTH,
                           height + pad * 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// A bold value above a small label, centred on a point.
static void draw_complication(GContext *ctx, GPoint at, const char *value, const char *label) {
  const int16_t top = at.y - (DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  draw_text_line(ctx, value, s_date_font, at, top, DATE_PAD, DATE_HEIGHT);
  draw_text_line(ctx, label, s_label_font, at, top + DATE_HEIGHT + LINE_GAP,
                 LABEL_PAD, LABEL_HEIGHT);
}

static void draw_weather(GContext *ctx, GPoint at) {
  char temperature[16];
  if (s_temperature == NO_TEMPERATURE) {
    snprintf(temperature, sizeof(temperature), "--\xc2\xb0");
  } else {
    snprintf(temperature, sizeof(temperature), "%d\xc2\xb0", (int)s_temperature);
  }
  draw_complication(ctx, at, temperature, s_condition[0] ? s_condition : "WEATHER");
}

#if defined(PBL_HEALTH)
static void draw_health(GContext *ctx, GPoint at) {
  char value[16];
  const char *label;
  format_metric(s_metric, value, sizeof(value), &label);
  draw_complication(ctx, at, value, label);
}
#endif

// The day of the week above the day of the month.
static void draw_date(GContext *ctx, GPoint at, const struct tm *t) {
  char day[4];
  strftime(day, sizeof(day), "%a", t);
  for (char *c = day; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
  char date[4];
  snprintf(date, sizeof(date), "%d", t->tm_mday);

  const int16_t top = at.y - (DATE_HEIGHT * 2 + LINE_GAP) / 2;
  draw_text_line(ctx, day, s_date_font, at, top, DATE_PAD, DATE_HEIGHT);
  draw_text_line(ctx, date, s_date_font, at, top + DATE_HEIGHT + LINE_GAP, DATE_PAD, DATE_HEIGHT);
}

// A ring of ten segments shows the battery level; the dot inside it is filled
// while the phone is connected, and shows a crescent moon during Quiet Time.
static void draw_battery_ring(GContext *ctx, GPoint at) {
  const int16_t inner = RING_RADIUS * 65 / 100;
  const int filled = (s_battery.charge_percent + 5) / 10;

  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < RING_SEGMENTS; i++) {
    const int32_t start = TRIG_MAX_ANGLE * i / RING_SEGMENTS;
    const int32_t end = TRIG_MAX_ANGLE * (i + 1) / RING_SEGMENTS;
    const int32_t middle = (start + end) / 2;
    GPoint points[] = {
      ray_point(at, start, RING_RADIUS),
      ray_point(at, middle, RING_RADIUS),
      ray_point(at, end, RING_RADIUS),
      ray_point(at, end, inner),
      ray_point(at, middle, inner),
      ray_point(at, start, inner),
    };
    GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
    GPath *segment = gpath_create(&info);
    if (i < filled) {
      graphics_context_set_fill_color(ctx, s_palette.ring);
      gpath_draw_filled(ctx, segment);
    }
    gpath_draw_outline(ctx, segment);
    gpath_destroy(segment);
  }

  graphics_context_set_fill_color(ctx, s_bluetooth_connected
                                  ? s_palette.connected : s_palette.background);
  graphics_fill_circle(ctx, at, inner - 2);
  graphics_draw_circle(ctx, at, inner - 2);

  if (quiet_time_is_active()) {
    const int16_t moon = inner - 4;
    graphics_context_set_fill_color(ctx, s_bluetooth_connected
                                    ? s_palette.background : s_palette.foreground);
    graphics_fill_circle(ctx, at, moon);
    graphics_context_set_fill_color(ctx, s_bluetooth_connected
                                    ? s_palette.connected : s_palette.background);
    graphics_fill_circle(ctx, GPoint(at.x + moon / 2, at.y - moon / 3), moon * 8 / 10);
  }
}

// A thin line out to the tip, from a leaf-shaped base around the centre.
static void draw_hand(GContext *ctx, GPoint centre, int32_t angle, int16_t length,
                      int16_t width) {
  const int16_t leaf = length * 40 / 100;
  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, ray_point(centre, angle, leaf - 2), ray_point(centre, angle, length));

  // Drawn pointing at 12, then turned into place.
  const int16_t r = LEAF_RADIUS;
  GPoint points[] = {
    { 0, r },
    { -r * 7 / 10, r * 7 / 10 },
    { -r, -leaf / 8 },
    { -r * 6 / 10, -leaf / 2 },
    { -(width / 2), -leaf },
    { width / 2, -leaf },
    { r * 6 / 10, -leaf / 2 },
    { r, -leaf / 8 },
    { r * 7 / 10, r * 7 / 10 },
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  gpath_rotate_to(path, angle);
  gpath_move_to(path, centre);
  graphics_context_set_fill_color(ctx, s_palette.leaf);
  gpath_draw_filled(ctx, path);
  graphics_context_set_stroke_width(ctx, LEAF_OUTLINE);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // The unobstructed area shrinks when a Timeline Quick View is showing.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GPoint centre = grect_center_point(&bounds);
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  draw_ticks(ctx, bounds, centre);
  draw_hours(ctx, bounds, centre);

  const int32_t reach_x = edge_distance(bounds, TRIG_MAX_ANGLE / 4);
  const int32_t reach_y = edge_distance(bounds, 0);
  const int16_t row = reach_y * 29 / 100;

  // The name sits halfway between the 12 and the complications below it.
  const int16_t numeral_height = s_settings.modern_numerals ? MODERN_HEIGHT : CLASSIC_HEIGHT;
  const int16_t below_numeral = reach_y - HOUR_TICK_LENGTH - NUMERAL_GAP - numeral_height;
  const int16_t above_row = row + (DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  const int16_t name_y = centre.y - (below_numeral + above_row) / 2;
  draw_text_line(ctx, DIAL_NAME, s_label_font, GPoint(centre.x, name_y),
                 name_y - LABEL_HEIGHT / 2, LABEL_PAD, LABEL_HEIGHT);

  const int16_t side = reach_x * 32 / 100;
  draw_weather(ctx, GPoint(centre.x - side, centre.y - row));
#if defined(PBL_HEALTH)
  draw_health(ctx, GPoint(centre.x + side, centre.y - row));
#endif
  draw_date(ctx, GPoint(centre.x - reach_x * 30 / 100, centre.y + row), t);
  draw_battery_ring(ctx, GPoint(centre.x + reach_x * 25 / 100, centre.y + row));

  const int16_t minute_length = (reach_x < reach_y ? reach_x : reach_y) - HOUR_TICK_LENGTH - 4;
  const int16_t hour_length = minute_length * 65 / 100;
  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;
  draw_hand(ctx, centre, hour_angle, hour_length, HOUR_HAND_WIDTH);
  draw_hand(ctx, centre, minute_angle, minute_length, MINUTE_HAND_WIDTH);
}

// ---------------------------------------------------------------------------
// Events

static void request_weather(void) {
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
  if (tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery = state;
  layer_mark_dirty(s_canvas);
}

static void bluetooth_handler(bool connected) {
  if (s_bluetooth_connected && !connected && s_settings.disconnect_vibe
      && !quiet_time_is_active()) {
    vibes_double_pulse();
  }
  s_bluetooth_connected = connected;
  layer_mark_dirty(s_canvas);
}

// Clay sends toggles as numbers and dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static bool read_settings(DictionaryIterator *iterator) {
  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.dark = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NUMERALS))) {
    s_settings.modern_numerals = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DISCONNECT_VIBE))) {
    s_settings.disconnect_vibe = tuple_int(tuple) != 0;
    changed = true;
  }
  return changed;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  const Tuple *temperature = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  const Tuple *condition = dict_find(iterator, MESSAGE_KEY_CONDITION);
  if (temperature) {
    s_temperature = temperature->value->int32;
    persist_write_int(PERSIST_KEY_TEMPERATURE, s_temperature);
  }
  if (condition) {
    strncpy(s_condition, condition->value->cstring, sizeof(s_condition) - 1);
    s_condition[sizeof(s_condition) - 1] = '\0';
    persist_write_string(PERSIST_KEY_CONDITION, s_condition);
  }

  if (read_settings(iterator)) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    window_set_background_color(s_window, s_palette.background);
  }
  layer_mark_dirty(s_canvas);
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

static void load_saved_state(void) {
  settings_set_defaults(&s_settings);
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings saved;
    const int size = persist_read_data(PERSIST_KEY_SETTINGS, &saved, sizeof(saved));
    if (size == (int)sizeof(saved) && saved.version == SETTINGS_VERSION) {
      s_settings = saved;
    }
  }
  apply_palette();

  if (persist_exists(PERSIST_KEY_TEMPERATURE)) {
    s_temperature = persist_read_int(PERSIST_KEY_TEMPERATURE);
  }
  if (persist_exists(PERSIST_KEY_CONDITION)) {
    persist_read_string(PERSIST_KEY_CONDITION, s_condition, sizeof(s_condition));
  }
#if defined(PBL_HEALTH)
  s_metric = persist_exists(PERSIST_KEY_METRIC)
    ? (Metric)persist_read_int(PERSIST_KEY_METRIC) % METRIC_COUNT
    : METRIC_STEPS;
  if (!metric_available(s_metric)) {
    s_metric = METRIC_STEPS;
  }
#endif
}

static void init(void) {
  s_modern_font = fonts_get_system_font(MODERN_FONT);
  s_date_font = fonts_get_system_font(DATE_FONT);
  s_label_font = fonts_get_system_font(LABEL_FONT);
  load_saved_state();
  s_battery = battery_state_service_peek();
  s_bluetooth_connected = connection_service_peek_pebble_app_connection();

  s_window = window_create();
  window_set_background_color(s_window, s_palette.background);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_handler,
  });
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
  accel_tap_service_subscribe(tap_handler);
#endif
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  // Settings from the phone arrive as one message with every option.
  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 32);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
  accel_tap_service_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
