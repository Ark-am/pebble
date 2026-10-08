#include <pebble.h>

#include "font_styles.h"

// Drawing text in a bundled font needs more stack than the app has to spare
// if every helper is folded into the drawing function, so the larger helpers
// are kept separate; each one's stack is then only in use while it runs.
#define NOINLINE __attribute__((noinline))

// A classic analog watchface: a minute track that follows the edge of the
// screen, hour numbers and indices, and tapered hands. The weather (with an
// icon for the conditions) and health data sit above the centre, the date and
// a battery ring below it; tap the watch to cycle the health metric. The
// background, the style of the hour numbers and the other options are
// configurable from the phone (see src/pkjs).

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_METRIC 2
#define PERSIST_KEY_TEMPERATURE 3
#define PERSIST_KEY_CONDITION 4
#define PERSIST_KEY_DAYTIME 5

#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define DATE_FONT FONT_KEY_GOTHIC_24_BOLD
  // Visible glyph heights and the blank space fonts leave above them.
  #define DATE_HEIGHT 17
  #define DATE_PAD 7
  #define LABEL_FONT FONT_KEY_GOTHIC_18
  #define LABEL_HEIGHT 13
  #define LABEL_PAD 5
  #define TEXT_BOX_WIDTH 80
  #define NAME_BOX_WIDTH 120
  #define CLASSIC_HEIGHT 20
  #define CLASSIC_STROKE 3
  #define MINUTE_TICK_LENGTH 6
  #define HOUR_TICK_LENGTH 9
  #define HOUR_HAND_WIDTH 4
  #define MINUTE_HAND_WIDTH 3
  #define LEAF_RADIUS 5
  #define LEAF_OUTLINE 2
  #define CORNER_RADIUS 22
  #define RING_RADIUS 15
  // Weather icons are designed on a 24 x 16 grid and drawn at this many
  // quarters of a pixel per unit.
  #define WEATHER_ICON_SCALE 4
  #define WEATHER_ICON_STROKE 2
#else
  #define DATE_FONT FONT_KEY_GOTHIC_18_BOLD
  #define DATE_HEIGHT 13
  #define DATE_PAD 5
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define LABEL_HEIGHT 10
  #define LABEL_PAD 4
  #define TEXT_BOX_WIDTH 56
  #define NAME_BOX_WIDTH 90
  #define CLASSIC_HEIGHT 15
  #define CLASSIC_STROKE 2
  #define MINUTE_TICK_LENGTH 4
  #define HOUR_TICK_LENGTH 7
  #define HOUR_HAND_WIDTH 3
  #define MINUTE_HAND_WIDTH 2
  #define LEAF_RADIUS 3
  #define LEAF_OUTLINE 1
  #define CORNER_RADIUS 16
  #define RING_RADIUS 11
  #define WEATHER_ICON_SCALE 3
  #define WEATHER_ICON_STROKE 1
#endif

#define EDGE_INSET 2
#define NUMERAL_GAP 6
#define LINE_GAP 2
#define DEFAULT_DIAL_NAME "Pebble"
#define RING_SEGMENTS 10
// How far along each hand its grey leaf reaches, in percent.
#define LEAF_PERCENT 70
// The dial-coloured border that separates the minute hand from the hour hand.
#define HAND_GAP 1

// How far an item may move to keep clear of the hands: around the centre in
// steps of 3 degrees, up to 60 degrees either way (or all the way round when
// fewer than four items leave space free), and outward in steps of a sixth of
// its distance. A step outward counts as three steps around. Plus the space to
// leave around it.
#define AVOID_STEP (TRIG_MAX_ANGLE / 120)
#define AVOID_STEPS 20
#define AVOID_STEPS_ROUND 60
#define AVOID_PUSHES 3
#define AVOID_PUSH_COST 3
#define AVOID_GAP 2

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 9
typedef struct {
  uint8_t version;
  bool dark;
  bool modern_numerals;
  bool disconnect_vibe;
  // Added in version 3. Empty to show no name.
  char dial_name[32];
  // Added in version 4.
  bool second_hand;
  // Added in version 5: which complications are shown.
  bool show_weather;
  bool show_health;
  bool show_date;
  bool show_battery;
  // Added in version 6.
  bool avoid_hands;
  // Added in version 7. 0 (clear) means the same colour as the text.
  uint8_t second_hand_argb;
  // Added in version 8: font styles for the modern hour numbers (no longer
  // used; every hour number now has the classic style) and for the
  // information (see font_styles.h).
  uint8_t number_font;
  uint8_t info_font;
  // Added in version 9: the HandStyle.
  uint8_t hand_style;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 2: return offsetof(Settings, dial_name);
    case 3: return offsetof(Settings, second_hand);
    case 4: return offsetof(Settings, show_weather);
    case 5: return offsetof(Settings, avoid_hands);
    case 6: return offsetof(Settings, second_hand_argb);
    case 7: return offsetof(Settings, number_font);
    case 8: return offsetof(Settings, hand_style);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

typedef struct {
  GColor background;
  GColor foreground;
  GColor leaf;
  GColor ring;
  GColor ring_empty;
  GColor connected;
  GColor disconnected;
  GColor sun;
  GColor rain;
  GColor second_hand;
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static StyledFont s_date_font;
static StyledFont s_label_font;

#if defined(PBL_COLOR)
// Smooth images of the hour numbers, already turned to follow the dial and
// made by tools/make_numerals.py, by hour (12 first); 0 where no number is
// drawn.
#if PBL_DISPLAY_WIDTH >= 200
static const uint32_t NUMERAL_BLACK[12] = {
  RESOURCE_ID_NUMERAL_LARGE_BLACK_12, 0, RESOURCE_ID_NUMERAL_LARGE_BLACK_2,
  RESOURCE_ID_NUMERAL_LARGE_BLACK_3, RESOURCE_ID_NUMERAL_LARGE_BLACK_4, 0,
  RESOURCE_ID_NUMERAL_LARGE_BLACK_6, 0, RESOURCE_ID_NUMERAL_LARGE_BLACK_8,
  RESOURCE_ID_NUMERAL_LARGE_BLACK_9, RESOURCE_ID_NUMERAL_LARGE_BLACK_10, 0,
};
static const uint32_t NUMERAL_WHITE[12] = {
  RESOURCE_ID_NUMERAL_LARGE_WHITE_12, 0, RESOURCE_ID_NUMERAL_LARGE_WHITE_2,
  RESOURCE_ID_NUMERAL_LARGE_WHITE_3, RESOURCE_ID_NUMERAL_LARGE_WHITE_4, 0,
  RESOURCE_ID_NUMERAL_LARGE_WHITE_6, 0, RESOURCE_ID_NUMERAL_LARGE_WHITE_8,
  RESOURCE_ID_NUMERAL_LARGE_WHITE_9, RESOURCE_ID_NUMERAL_LARGE_WHITE_10, 0,
};
#else
static const uint32_t NUMERAL_BLACK[12] = {
  RESOURCE_ID_NUMERAL_SMALL_BLACK_12, 0, RESOURCE_ID_NUMERAL_SMALL_BLACK_2,
  RESOURCE_ID_NUMERAL_SMALL_BLACK_3, RESOURCE_ID_NUMERAL_SMALL_BLACK_4, 0,
  RESOURCE_ID_NUMERAL_SMALL_BLACK_6, 0, RESOURCE_ID_NUMERAL_SMALL_BLACK_8,
  RESOURCE_ID_NUMERAL_SMALL_BLACK_9, RESOURCE_ID_NUMERAL_SMALL_BLACK_10, 0,
};
static const uint32_t NUMERAL_WHITE[12] = {
  RESOURCE_ID_NUMERAL_SMALL_WHITE_12, 0, RESOURCE_ID_NUMERAL_SMALL_WHITE_2,
  RESOURCE_ID_NUMERAL_SMALL_WHITE_3, RESOURCE_ID_NUMERAL_SMALL_WHITE_4, 0,
  RESOURCE_ID_NUMERAL_SMALL_WHITE_6, 0, RESOURCE_ID_NUMERAL_SMALL_WHITE_8,
  RESOURCE_ID_NUMERAL_SMALL_WHITE_9, RESOURCE_ID_NUMERAL_SMALL_WHITE_10, 0,
};
#endif

static GBitmap *s_numerals[12];

static void unload_numerals(void) {
  for (int i = 0; i < 12; i++) {
    if (s_numerals[i]) {
      gbitmap_destroy(s_numerals[i]);
      s_numerals[i] = NULL;
    }
  }
}
#endif

static BatteryChargeState s_battery;
static bool s_bluetooth_connected;
static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[16];
static bool s_daytime = true;

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
  if (s_settings.show_health && (event != HealthEventSleepUpdate || s_metric == METRIC_SLEEP)) {
    layer_mark_dirty(s_canvas);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_settings.show_health) {
    return;
  }
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
    .dial_name = DEFAULT_DIAL_NAME,
    .second_hand = false,
    .show_weather = true,
    .show_health = true,
    .show_date = true,
    .show_battery = true,
    .avoid_hands = false,
    .second_hand_argb = GColorRedARGB8,
  };
}

static bool hour_has_numeral(int hour) {
  return s_settings.modern_numerals ? hour % 3 == 0 : hour % 2 == 0;
}

static void apply_palette(void) {
  const bool dark = s_settings.dark;
  s_palette.background = dark ? GColorBlack : GColorWhite;
  s_palette.foreground = dark ? GColorWhite : GColorBlack;
#if defined(PBL_COLOR)
  s_palette.leaf = dark ? GColorDarkGray : GColorLightGray;
  s_palette.ring = GColorMintGreen;
  s_palette.ring_empty = GColorBlack;
  s_palette.connected = GColorPictonBlue;
  s_palette.disconnected = GColorRed;
  s_palette.sun = GColorChromeYellow;
  s_palette.rain = dark ? GColorPictonBlue : GColorBlue;
  s_palette.second_hand = s_settings.second_hand_argb
    ? (GColor) { .argb = s_settings.second_hand_argb } : s_palette.foreground;
#else
  s_palette.leaf = s_palette.background;
  s_palette.ring = s_palette.foreground;
  s_palette.ring_empty = s_palette.background;
  s_palette.connected = s_palette.foreground;
  s_palette.disconnected = s_palette.background;
  s_palette.sun = s_palette.foreground;
  s_palette.rain = s_palette.foreground;
  s_palette.second_hand = s_palette.foreground;
#endif

#if defined(PBL_COLOR)
  // The numbers' images match the text colour; only those shown are loaded.
  unload_numerals();
  const uint32_t *ids = s_settings.dark ? NUMERAL_WHITE : NUMERAL_BLACK;
  for (int hour = 0; hour < 12; hour++) {
    if (hour_has_numeral(hour) && ids[hour]) {
      s_numerals[hour] = gbitmap_create_with_resource(ids[hour]);
    }
  }
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

#if !defined(PBL_ROUND)
static int32_t isqrt(int32_t value) {
  if (value <= 0) {
    return 0;
  }
  int32_t root = value;
  int32_t next = (root + 1) / 2;
  while (next < root) {
    root = next;
    next = (root + value / root) / 2;
  }
  return root;
}
#endif

// Distance from the centre to the edge of the dial along an angle. Round
// screens use a circle; rectangular ones follow the dial's rounded rectangle so
// the minute track fills the whole face.
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
  const int32_t distance = to_side < to_top ? to_side : to_top;

  // Where the ray meets a rounded corner, find where it crosses the corner's
  // circle instead, working in sixteenths of a pixel.
  const int32_t radius = CORNER_RADIUS - EDGE_INSET;
  const int32_t corner_x = half_w - radius;
  const int32_t corner_y = half_h - radius;
  if (distance * sin_abs / TRIG_MAX_RATIO <= corner_x
      || distance * cos_abs / TRIG_MAX_RATIO <= corner_y) {
    return distance;
  }
  const int32_t along = (sin_abs * corner_x + cos_abs * corner_y) / (TRIG_MAX_RATIO / 16);
  const int32_t offset = along * along
    - 256 * (corner_x * corner_x + corner_y * corner_y - radius * radius);
  return (along + isqrt(offset)) / 16;
#endif
}

// The dial: the whole screen on round watches, a rectangle with rounded, black
// corners on the others.
static NOINLINE void fill_dial(GContext *ctx, GRect screen, GRect bounds) {
#if defined(PBL_ROUND)
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, screen, 0, GCornerNone);
#else
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, screen, 0, GCornerNone);
  const int16_t r = CORNER_RADIUS;
  const GRect b = bounds;
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, GRect(b.origin.x + r, b.origin.y, b.size.w - r * 2, b.size.h),
                     0, GCornerNone);
  graphics_fill_rect(ctx, GRect(b.origin.x, b.origin.y + r, b.size.w, b.size.h - r * 2),
                     0, GCornerNone);
  const int16_t left = b.origin.x + r;
  const int16_t right = b.origin.x + b.size.w - r - 1;
  const int16_t top = b.origin.y + r;
  const int16_t bottom = b.origin.y + b.size.h - r - 1;
  graphics_fill_circle(ctx, GPoint(left, top), r);
  graphics_fill_circle(ctx, GPoint(right, top), r);
  graphics_fill_circle(ctx, GPoint(left, bottom), r);
  graphics_fill_circle(ctx, GPoint(right, bottom), r);

  // Filled circles have stepped edges, so a smooth arc is drawn over the outer
  // quarter of each corner.
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  const GPoint corners[] = {
    GPoint(left, top), GPoint(right, top), GPoint(right, bottom), GPoint(left, bottom),
  };
  for (int i = 0; i < 4; i++) {
    // Quarters clockwise from 12 o'clock: top right, bottom right, bottom left,
    // top left. Corner i needs the quarter i + 3.
    const int32_t start = TRIG_MAX_ANGLE * ((i + 3) % 4) / 4;
    graphics_draw_arc(ctx, GRect(corners[i].x - r - 1, corners[i].y - r - 1, r * 2 + 3, r * 2 + 3),
                      GOvalScaleModeFitCircle, start, start + TRIG_MAX_ANGLE / 4);
  }
#endif
}

static GRect rect_grow(GRect rect, int16_t by) {
  return GRect(rect.origin.x - by, rect.origin.y - by,
               rect.size.w + by * 2, rect.size.h + by * 2);
}

static bool rects_overlap(GRect a, GRect b) {
  return a.origin.x < b.origin.x + b.size.w && b.origin.x < a.origin.x + a.size.w
    && a.origin.y < b.origin.y + b.size.h && b.origin.y < a.origin.y + a.size.h;
}

// Positive or negative according to which side of the line from a to b the
// point is on.
static int32_t side_of_line(GPoint a, GPoint b, GPoint point) {
  return (int32_t)(b.x - a.x) * (point.y - a.y) - (int32_t)(b.y - a.y) * (point.x - a.x);
}

static bool lines_cross(GPoint a, GPoint b, GPoint c, GPoint d) {
  return (side_of_line(c, d, a) > 0) != (side_of_line(c, d, b) > 0)
    && (side_of_line(a, b, c) > 0) != (side_of_line(a, b, d) > 0);
}

// A line that passes through a rectangle either ends inside it or crosses one
// of its diagonals.
static bool line_crosses_rect(GPoint a, GPoint b, GRect rect) {
  const GPoint top_left = rect.origin;
  const GPoint bottom_right = GPoint(rect.origin.x + rect.size.w, rect.origin.y + rect.size.h);
  const GPoint top_right = GPoint(bottom_right.x, top_left.y);
  const GPoint bottom_left = GPoint(top_left.x, bottom_right.y);
  return grect_contains_point(&rect, &a) || grect_contains_point(&rect, &b)
    || lines_cross(a, b, top_left, bottom_right)
    || lines_cross(a, b, top_right, bottom_left);
}

#if !defined(PBL_COLOR)
// ---------------------------------------------------------------------------
// Classic numerals for black-and-white watches
//
// The system fonts cannot be rotated, and these watches cannot show the soft
// edges of the images colour watches use, so the classic numerals are drawn as
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

// The 3 and 9 are for the modern numbers. The 9 is the 6 turned upside down.
static const int8_t GLYPH_3[] = {
  1, 5, 4, 1, 8, 0, 12, 1, 14, 5, 14, 10, 11, 14, 6, 15, 11, 17, 14, 21, 15, 26,
  13, 30, 8, 32, 4, 31, 1, 27,
  GLYPH_END,
};
static const int8_t GLYPH_9[] = {
  3, 29, 6, 32, 10, 31, 13, 27, 15, 20, 15, 12, 14, 5, 11, 1, 8, 0, 5, 1, 2, 5, 1, 10,
  2, 15, 5, 18, 8, 19, 11, 18, 14, 15, 15, 11,
  GLYPH_END,
};

// Only the digits of 12, 2, 3, 4, 6, 8, 9 and 10 are needed.
static const int8_t *const GLYPHS[10] = {
  GLYPH_0, GLYPH_1, GLYPH_2, GLYPH_3, GLYPH_4, NULL, GLYPH_6, NULL, GLYPH_8, GLYPH_9,
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
#endif

// ---------------------------------------------------------------------------
// Drawing

static int16_t text_width_in(const char *text, GFont font, int16_t box_width) {
  return graphics_text_layout_get_content_size(
    text, font, GRect(0, 0, box_width, 40),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter).w;
}

static int16_t text_width(const char *text, GFont font) {
  return text_width_in(text, font, TEXT_BOX_WIDTH);
}

static NOINLINE void draw_ticks(GContext *ctx, GRect bounds, GPoint centre) {
  for (int i = 0; i < 60; i++) {
    const bool hour = i % 5 == 0;
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    const int32_t outer = edge_distance(bounds, angle);
    const int32_t length = hour ? HOUR_TICK_LENGTH : MINUTE_TICK_LENGTH;
    graphics_context_set_stroke_color(ctx, s_palette.foreground);
    graphics_context_set_stroke_width(ctx, hour ? 2 : 1);
    graphics_draw_line(ctx, ray_point(centre, angle, outer - length),
                       ray_point(centre, angle, outer));
  }
}

// The area an hour number covers, just inside the ticks. Colour watches use
// the size of the number's image, and black-and-white watches a square as
// wide as the number. Classic numbers turn with the dial, so their height
// faces the ticks; the upright modern 3 and 9 face them with their width.
static GRect numeral_rect(GRect bounds, GPoint centre, int hour) {
  const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
  const int32_t edge = edge_distance(bounds, angle);
#if defined(PBL_COLOR)
  const GSize size = gbitmap_get_bounds(s_numerals[hour]).size;
  const int16_t width = size.w;
  const int16_t height = size.h;
#else
  const int length = hour == 0 || hour >= 10 ? 2 : 1;
  int16_t width = (length * GLYPH_WIDTH + (length - 1) * GLYPH_GAP) * CLASSIC_HEIGHT / GLYPH_HEIGHT;
  if (width < CLASSIC_HEIGHT) {
    width = CLASSIC_HEIGHT;
  }
  const int16_t height = width;
#endif
  const int32_t reach = s_settings.modern_numerals && hour % 6 ? width / 2 : CLASSIC_HEIGHT / 2;
  const GPoint at = ray_point(centre, angle, edge - HOUR_TICK_LENGTH - NUMERAL_GAP - reach);
  return GRect(at.x - width / 2, at.y - height / 2, width, height);
}

// Whether a rectangle or a line runs into any of the given areas. Empty areas,
// for things not shown, never count.
static bool rect_covered(GRect rect, const GRect *covers, int count) {
  for (int i = 0; i < count; i++) {
    if (covers[i].size.w && rects_overlap(rect, covers[i])) {
      return true;
    }
  }
  return false;
}

static bool line_covered(GPoint a, GPoint b, const GRect *covers, int count) {
  for (int i = 0; i < count; i++) {
    if (covers[i].size.w && line_crosses_rect(a, b, rect_grow(covers[i], 1))) {
      return true;
    }
  }
  return false;
}

// Hour numbers and the index lines between them. Any that would run into the
// information on the dial are left out.
static NOINLINE void draw_hours(GContext *ctx, GRect bounds, GPoint centre, const GRect *covers,
                       int cover_count) {
  for (int hour = 0; hour < 12; hour++) {
    const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
    if (!hour_has_numeral(hour)) {
      const int32_t edge = edge_distance(bounds, angle);
      const GPoint inner = ray_point(centre, angle, edge * 64 / 100);
      const GPoint outer = ray_point(centre, angle, edge * 82 / 100);
      if (!line_covered(inner, outer, covers, cover_count)) {
        graphics_context_set_stroke_color(ctx, s_palette.foreground);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_line(ctx, inner, outer);
      }
      continue;
    }

    const GRect area = numeral_rect(bounds, centre, hour);
    if (rect_covered(area, covers, cover_count)) {
      continue;
    }
#if defined(PBL_COLOR)
    // The images are already turned and sized to fill the area.
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_numerals[hour], area);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
#else
    char text[3];
    snprintf(text, sizeof(text), "%d", hour == 0 ? 12 : hour);
    // The tops of the classic numbers face outward, except on the lower half
    // of the dial, where that would turn them upside down. Modern numbers
    // stay upright.
    const bool lower = hour > 3 && hour < 9;
    draw_classic_numeral(ctx, text, grect_center_point(&area),
                         s_settings.modern_numerals ? 0
                         : lower ? angle - TRIG_MAX_ANGLE / 2 : angle);
#endif
  }
}

static void draw_text_box(GContext *ctx, const char *text, GFont font, GPoint centre,
                          int16_t visible_top, int16_t pad, int16_t height, int16_t width) {
  graphics_context_set_text_color(ctx, s_palette.foreground);
  graphics_draw_text(ctx, text, font,
                     GRect(centre.x - width / 2, visible_top - pad, width,
                           height + pad * 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_text_line(GContext *ctx, const char *text, GFont font, GPoint centre,
                           int16_t visible_top, int16_t pad, int16_t height) {
  draw_text_box(ctx, text, font, centre, visible_top, pad, height, TEXT_BOX_WIDTH);
}

// A bold value above a small label, centred on a point.
static void draw_complication(GContext *ctx, GPoint at, const char *value, const char *label) {
  const int16_t top = at.y - (DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  draw_text_line(ctx, value, s_date_font.font, at, top, s_date_font.pad, DATE_HEIGHT);
  draw_text_line(ctx, label, s_label_font.font, at, top + DATE_HEIGHT + LINE_GAP,
                 s_label_font.pad, LABEL_HEIGHT);
}

// ---------------------------------------------------------------------------
// Weather icons

// Condition names as the phone sends them (see src/pkjs/index.js).
typedef enum {
  WEATHER_UNKNOWN,
  WEATHER_CLEAR,
  WEATHER_FAIR,
  WEATHER_CLOUDY,
  WEATHER_FOG,
  WEATHER_DRIZZLE,
  WEATHER_RAIN,
  WEATHER_SHOWERS,
  WEATHER_SNOW,
  WEATHER_STORM,
} Weather;

static Weather weather_from_condition(const char *condition) {
  static const char *const names[] = {
    [WEATHER_CLEAR] = "CLEAR",
    [WEATHER_FAIR] = "FAIR",
    [WEATHER_CLOUDY] = "CLOUDY",
    [WEATHER_FOG] = "FOG",
    [WEATHER_DRIZZLE] = "DRIZZLE",
    [WEATHER_RAIN] = "RAIN",
    [WEATHER_SHOWERS] = "SHOWERS",
    [WEATHER_SNOW] = "SNOW",
    [WEATHER_STORM] = "STORM",
  };
  for (size_t i = 1; i < ARRAY_LENGTH(names); i++) {
    if (strcmp(condition, names[i]) == 0) {
      return i;
    }
  }
  return WEATHER_UNKNOWN;
}

// A point on the icon's 24 x 16 design grid.
static GPoint icon_point(GPoint origin, int16_t x, int16_t y) {
  return GPoint(origin.x + x * WEATHER_ICON_SCALE / 4, origin.y + y * WEATHER_ICON_SCALE / 4);
}

static int16_t icon_size(int16_t size) {
  return size * WEATHER_ICON_SCALE / 4;
}

// A cloud resting on a point: a large puff between a small one on the left and
// a medium one on the right, over a flat base. It is about three and a quarter
// times as wide as the radius and twice as tall. Growing it gives the outline
// that separates it from a sun or moon behind it.
static void draw_cloud(GContext *ctx, GPoint bottom, int16_t radius, int16_t grow,
                       GColor color) {
  const int16_t left_radius = radius * 55 / 100;
  const int16_t right_radius = radius * 65 / 100;
  const GPoint middle = GPoint(bottom.x, bottom.y - radius);
  const GPoint left = GPoint(bottom.x - radius, bottom.y - left_radius);
  const GPoint right = GPoint(bottom.x + radius * 105 / 100, bottom.y - right_radius);
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, middle, radius + grow);
  graphics_fill_circle(ctx, left, left_radius + grow);
  graphics_fill_circle(ctx, right, right_radius + grow);
  graphics_fill_rect(ctx, GRect(left.x, bottom.y - left_radius - grow, right.x - left.x,
                                left_radius + grow * 2 + 1),
                     0, GCornerNone);
}

static void draw_outlined_cloud(GContext *ctx, GPoint bottom, int16_t radius) {
  draw_cloud(ctx, bottom, radius, 1, s_palette.background);
  draw_cloud(ctx, bottom, radius, 0, s_palette.foreground);
}

static void draw_sun(GContext *ctx, GPoint centre, int16_t radius) {
  graphics_context_set_fill_color(ctx, s_palette.sun);
  graphics_fill_circle(ctx, centre, radius);
  graphics_context_set_stroke_color(ctx, s_palette.sun);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < 8; i++) {
    const int32_t angle = TRIG_MAX_ANGLE * i / 8;
    graphics_draw_line(ctx, ray_point(centre, angle, radius + 2),
                       ray_point(centre, angle, radius * 2 + 1));
  }
}

static void draw_moon(GContext *ctx, GPoint centre, int16_t radius) {
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  graphics_fill_circle(ctx, centre, radius);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, GPoint(centre.x + radius / 2, centre.y - radius / 3), radius * 5 / 6);
}

// Short slanted lines, round drops or snowflakes below a cloud.
static void draw_precipitation(GContext *ctx, GPoint origin, Weather weather) {
  static const int16_t columns[] = { 8, 13, 18 };
  for (size_t i = 0; i < ARRAY_LENGTH(columns); i++) {
    const int16_t x = columns[i];
    switch (weather) {
      case WEATHER_SNOW:
        graphics_context_set_fill_color(ctx, s_palette.foreground);
        graphics_fill_circle(ctx, icon_point(origin, x - 1, i % 2 ? 15 : 13), icon_size(3) / 2);
        break;
      case WEATHER_DRIZZLE:
        graphics_context_set_fill_color(ctx, s_palette.rain);
        graphics_fill_circle(ctx, icon_point(origin, x - 1, i % 2 ? 15 : 13), icon_size(2) / 2);
        break;
      default:
        graphics_context_set_stroke_color(ctx, s_palette.rain);
        graphics_context_set_stroke_width(ctx, WEATHER_ICON_STROKE);
        graphics_draw_line(ctx, icon_point(origin, x, 12), icon_point(origin, x - 2, 16));
        break;
    }
  }
}

static void draw_lightning(GContext *ctx, GPoint origin) {
  const GPoint points[] = {
    icon_point(origin, 14, 9), icon_point(origin, 10, 13),
    icon_point(origin, 14, 13), icon_point(origin, 10, 17),
  };
  graphics_context_set_stroke_color(ctx, s_palette.sun);
  graphics_context_set_stroke_width(ctx, WEATHER_ICON_STROKE);
  for (size_t i = 1; i < ARRAY_LENGTH(points); i++) {
    graphics_draw_line(ctx, points[i - 1], points[i]);
  }
}

// Draws the icon for a condition in a 24 x 16 unit box centred on a point.
static NOINLINE void draw_weather_icon(GContext *ctx, GPoint centre, Weather weather) {
  const GPoint origin = GPoint(centre.x - icon_size(24) / 2, centre.y - icon_size(16) / 2);
  switch (weather) {
    case WEATHER_CLEAR:
      if (s_daytime) {
        draw_sun(ctx, icon_point(origin, 12, 8), icon_size(4));
      } else {
        draw_moon(ctx, icon_point(origin, 12, 8), icon_size(7));
      }
      break;
    case WEATHER_FAIR:
      if (s_daytime) {
        draw_sun(ctx, icon_point(origin, 8, 6), icon_size(3));
      } else {
        draw_moon(ctx, icon_point(origin, 8, 6), icon_size(5));
      }
      draw_outlined_cloud(ctx, icon_point(origin, 14, 16), icon_size(5));
      break;
    case WEATHER_CLOUDY:
      draw_cloud(ctx, icon_point(origin, 11, 16), icon_size(7), 0, s_palette.foreground);
      break;
    case WEATHER_FOG:
      graphics_context_set_stroke_color(ctx, s_palette.foreground);
      graphics_context_set_stroke_width(ctx, WEATHER_ICON_STROKE);
      for (int i = 0; i < 3; i++) {
        const int16_t y = 3 + i * 5;
        const int16_t shift = i % 2 ? 2 : 0;
        graphics_draw_line(ctx, icon_point(origin, 3 + shift, y), icon_point(origin, 19 + shift, y));
      }
      break;
    case WEATHER_DRIZZLE:
    case WEATHER_RAIN:
    case WEATHER_SHOWERS:
    case WEATHER_SNOW:
      draw_cloud(ctx, icon_point(origin, 12, 10), icon_size(5), 0, s_palette.foreground);
      draw_precipitation(ctx, origin, weather);
      break;
    case WEATHER_STORM:
      draw_cloud(ctx, icon_point(origin, 12, 10), icon_size(5), 0, s_palette.foreground);
      draw_lightning(ctx, origin);
      break;
    default:
      break;
  }
}

static void format_temperature(char *buffer, size_t size) {
  if (s_temperature == NO_TEMPERATURE) {
    snprintf(buffer, size, "--\xc2\xb0");
  } else {
    snprintf(buffer, size, "%d\xc2\xb0", (int)s_temperature);
  }
}

static const char *weather_label(void) {
  return s_condition[0] ? s_condition : "WEATHER";
}

// The temperature above an icon for the conditions. Conditions without an
// icon, and the time before the first reading, are written out instead.
static NOINLINE void draw_weather(GContext *ctx, GPoint at) {
  char temperature[16];
  format_temperature(temperature, sizeof(temperature));
  const Weather weather = weather_from_condition(s_condition);
  if (weather == WEATHER_UNKNOWN) {
    draw_complication(ctx, at, temperature, weather_label());
    return;
  }
  const int16_t top = at.y - (DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  draw_text_line(ctx, temperature, s_date_font.font, at, top, s_date_font.pad, DATE_HEIGHT);
  draw_weather_icon(ctx, GPoint(at.x, top + DATE_HEIGHT + LINE_GAP + 1 + icon_size(16) / 2),
                    weather);
}

#if defined(PBL_HEALTH)
static NOINLINE void draw_health(GContext *ctx, GPoint at) {
  char value[16];
  const char *label;
  format_metric(s_metric, value, sizeof(value), &label);
  draw_complication(ctx, at, value, label);
}
#endif

// The day of the week, in capitals, and the day of the month. Both buffers
// hold four characters.
static void format_date(const struct tm *t, char *day, char *date) {
  strftime(day, 4, "%a", t);
  for (char *c = day; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
  snprintf(date, 4, "%d", t->tm_mday);
}

// The day of the week above the day of the month.
static NOINLINE void draw_date(GContext *ctx, GPoint at, const struct tm *t) {
  char day[4];
  char date[4];
  format_date(t, day, date);

  const int16_t top = at.y - (DATE_HEIGHT * 2 + LINE_GAP) / 2;
  draw_text_line(ctx, day, s_date_font.font, at, top, s_date_font.pad, DATE_HEIGHT);
  draw_text_line(ctx, date, s_date_font.font, at, top + DATE_HEIGHT + LINE_GAP, s_date_font.pad, DATE_HEIGHT);
}

// A ring of ten segments shows the battery level, full segments green and
// empty ones black (filled and empty on black-and-white watches). The dot
// inside it is light blue while the phone is connected and red with a white
// bar across it when it is not (filled, and empty with a bar, on black-and-
// white watches), and shows a crescent moon during Quiet Time.
static NOINLINE void draw_battery_ring(GContext *ctx, GPoint at) {
  const int16_t inner = RING_RADIUS * 65 / 100;
  const int filled = (s_battery.charge_percent + 5) / 10;

  // A solid disc behind the segments fills the gaps between them and the dot.
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  graphics_fill_circle(ctx, at, inner + 1);

  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < RING_SEGMENTS; i++) {
    const int32_t start = TRIG_MAX_ANGLE * i / RING_SEGMENTS;
    const int32_t step = TRIG_MAX_ANGLE / RING_SEGMENTS / 4;
    // Five points along each edge keep the ring round.
    GPoint points[10];
    for (int k = 0; k < 5; k++) {
      points[k] = ray_point(at, start + step * k, RING_RADIUS);
      points[9 - k] = ray_point(at, start + step * k, inner);
    }
    GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
    GPath *segment = gpath_create(&info);
    graphics_context_set_fill_color(ctx, i < filled ? s_palette.ring : s_palette.ring_empty);
    gpath_draw_filled(ctx, segment);
    gpath_draw_outline(ctx, segment);
    gpath_destroy(segment);
  }
  // A smooth circle over the outer edge rounds off the segments' corners.
  graphics_draw_circle(ctx, at, RING_RADIUS);

  // The dot, with a border in the text colour.
  const GColor dot = s_bluetooth_connected ? s_palette.connected : s_palette.disconnected;
  graphics_context_set_fill_color(ctx, dot);
  graphics_fill_circle(ctx, at, inner - 1);
  graphics_draw_circle(ctx, at, inner - 1);

  if (!s_bluetooth_connected) {
    // A bar across the dot, like a no-entry sign, so a lost connection
    // stands out. It takes the place of the Quiet Time moon.
    const int16_t radius = inner - 1;
    const int16_t half_width = radius - 2;
    const int16_t height = radius * 4 / 10 > 2 ? radius * 4 / 10 : 2;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, s_palette.foreground));
    graphics_fill_rect(ctx, GRect(at.x - half_width, at.y - height / 2, half_width * 2 + 1, height),
                       1, GCornersAll);
  } else if (quiet_time_is_active()) {
    // On black-and-white watches an empty dot needs a filled moon.
    const bool empty = gcolor_equal(dot, s_palette.background);
    const int16_t moon = inner - 3;
    graphics_context_set_fill_color(ctx, empty ? s_palette.foreground : s_palette.background);
    graphics_fill_circle(ctx, at, moon);
    graphics_context_set_fill_color(ctx, dot);
    graphics_fill_circle(ctx, GPoint(at.x + moon / 2, at.y - moon / 3), moon * 8 / 10);
  }
}

// A point on a hand pointing at an angle: across to the right and along
// towards the tip (negative along runs back past the centre), rounded to the
// nearest pixel, so the leaf and the line of a hand always line up.
static GPoint hand_point(GPoint centre, int32_t angle, int32_t across, int32_t along) {
  const int32_t sin = sin_lookup(angle);
  const int32_t cos = cos_lookup(angle);
  const int32_t x = across * cos + along * sin;
  const int32_t y = across * sin - along * cos;
  const int32_t half = TRIG_MAX_RATIO / 2;
  return GPoint(centre.x + (x >= 0 ? x + half : x - half) / TRIG_MAX_RATIO,
                centre.y + (y >= 0 ? y + half : y - half) / TRIG_MAX_RATIO);
}

// Hand styles; values match the settings page.
typedef enum {
  HANDS_LEAF,      // A thin line from a leaf-shaped base.
  HANDS_BATON,     // A slim solid bar with a grey inlay and a pointed end.
  HANDS_DAUPHINE,  // A slim faceted spear, half light and half dark.
  HANDS_BREGUET,   // A tapering needle through an open ring near the tip.
  HANDS_SWORD,     // A narrow blade that widens, then comes to a point.
  HANDS_COUNT,
} HandStyle;

// How wide a hand's shapes are on either side of its line: the minute hand
// is a little slimmer than the hour hand.
static int16_t hand_bulk(bool minute) {
  return minute ? LEAF_RADIUS * 8 / 10 : LEAF_RADIUS;
}

// Half the width of a slim bar or needle, never under a pixel and a half.
static int16_t slim_half_width(bool minute) {
  const int16_t half = hand_bulk(minute) * 6 / 10;
  return half > 2 ? half : 2;
}

// Each part of a hand is drawn twice for a hand with a border: first wider in
// the dial colour all the way round, then itself over that, so the border
// never cuts into the hand.
static void hand_line(GContext *ctx, GPoint a, GPoint b, int16_t width, bool border) {
  graphics_context_set_stroke_color(ctx, border ? s_palette.background : s_palette.foreground);
  graphics_context_set_stroke_width(ctx, width + (border ? HAND_GAP * 2 : 0));
  graphics_draw_line(ctx, a, b);
}

// A filled shape with an outline in the text colour, the given width.
static void hand_shape(GContext *ctx, GPoint *points, int count, GColor fill,
                       int16_t outline, bool border) {
  GPathInfo info = { .num_points = count, .points = points };
  GPath *path = gpath_create(&info);
  if (border) {
    graphics_context_set_stroke_color(ctx, s_palette.background);
    graphics_context_set_stroke_width(ctx, outline + HAND_GAP * 2);
  } else {
    graphics_context_set_fill_color(ctx, fill);
    gpath_draw_filled(ctx, path);
    graphics_context_set_stroke_color(ctx, s_palette.foreground);
    graphics_context_set_stroke_width(ctx, outline);
  }
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

// Fills a shape with no outline, for the facets and inlays inside a hand.
static void hand_fill(GContext *ctx, GPoint *points, int count, GColor fill) {
  GPathInfo info = { .num_points = count, .points = points };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, fill);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// An open ring, showing the dial through its middle.
static void hand_ring(GContext *ctx, GPoint at, int16_t radius, bool border) {
  if (!border) {
    graphics_context_set_fill_color(ctx, s_palette.background);
    graphics_fill_circle(ctx, at, radius);
  }
  graphics_context_set_stroke_color(ctx, border ? s_palette.background : s_palette.foreground);
  graphics_context_set_stroke_width(ctx, LEAF_OUTLINE + (border ? HAND_GAP * 2 : 0));
  graphics_draw_circle(ctx, at, radius);
}

static void draw_hand_parts(GContext *ctx, GPoint centre, int32_t angle, int16_t length,
                            int16_t width, bool minute, bool border) {
  const int16_t r = LEAF_RADIUS;
  const int16_t bulk = hand_bulk(minute);
  const GPoint tip = hand_point(centre, angle, 0, length);
  switch (s_settings.hand_style) {
    case HANDS_BATON: {
      // A solid bar from a short tail, its end cut to a shallow point.
      const int16_t half = slim_half_width(minute);
      GPoint points[] = {
        hand_point(centre, angle, -half, -r * 2),
        hand_point(centre, angle, -half, length - half * 2),
        tip,
        hand_point(centre, angle, half, length - half * 2),
        hand_point(centre, angle, half, -r * 2),
      };
      hand_shape(ctx, points, ARRAY_LENGTH(points), s_palette.foreground, 1, border);
      if (!border && half >= 3) {
        // A grey inlay down the outer part, like a watch's luminous strip.
        graphics_context_set_stroke_color(ctx, s_palette.leaf);
        graphics_context_set_stroke_width(ctx, (half - 2) * 2);
        graphics_draw_line(ctx, hand_point(centre, angle, 0, length * 35 / 100),
                           hand_point(centre, angle, 0, length - half * 3));
      }
      break;
    }
    case HANDS_DAUPHINE: {
      // Widest a little way out, tapering to points at both ends. The left
      // facet is light and the right dark, as if lit from the left; the
      // ridge between them runs down the middle.
      const int16_t widest = length * 18 / 100;
      GPoint points[] = {
        hand_point(centre, angle, 0, -r * 2),
        hand_point(centre, angle, -bulk, widest),
        tip,
        hand_point(centre, angle, bulk, widest),
      };
      if (border) {
        hand_shape(ctx, points, ARRAY_LENGTH(points), s_palette.leaf, 1, true);
        break;
      }
      hand_fill(ctx, points, ARRAY_LENGTH(points), s_palette.leaf);
      GPoint facet[] = { points[0], tip, points[3] };
      hand_fill(ctx, facet, ARRAY_LENGTH(facet), s_palette.foreground);
      GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
      GPath *outline = gpath_create(&info);
      graphics_context_set_stroke_color(ctx, s_palette.foreground);
      graphics_context_set_stroke_width(ctx, 1);
      gpath_draw_outline(ctx, outline);
      gpath_destroy(outline);
      break;
    }
    case HANDS_BREGUET: {
      // A needle tapering from the centre to a fine point, through an open
      // ring most of the way out.
      const int16_t half = slim_half_width(minute);
      GPoint needle[] = {
        hand_point(centre, angle, -half, -r * 2),
        tip,
        hand_point(centre, angle, half, -r * 2),
      };
      hand_shape(ctx, needle, ARRAY_LENGTH(needle), s_palette.foreground, 1, border);
      hand_ring(ctx, hand_point(centre, angle, 0, length * 70 / 100), bulk, border);
      break;
    }
    case HANDS_SWORD: {
      // Narrow at the centre, widening to its broadest about two thirds of
      // the way out, then sweeping to a point, with a ridge down the middle.
      const int16_t base = slim_half_width(minute) - 1;
      const int16_t shoulder = length * 66 / 100;
      GPoint points[] = {
        hand_point(centre, angle, -base, -r * 2),
        hand_point(centre, angle, -bulk, shoulder),
        tip,
        hand_point(centre, angle, bulk, shoulder),
        hand_point(centre, angle, base, -r * 2),
      };
      hand_shape(ctx, points, ARRAY_LENGTH(points), s_palette.leaf, LEAF_OUTLINE, border);
      if (!border) {
        hand_line(ctx, centre, hand_point(centre, angle, 0, shoulder), 1, false);
      }
      break;
    }
    default: {
      const int16_t leaf = length * LEAF_PERCENT / 100;
      hand_line(ctx, hand_point(centre, angle, 0, leaf - 2), tip, width, border);
      // The leaf's corners, across and along the hand.
      GPoint points[] = {
        hand_point(centre, angle, 0, -r),
        hand_point(centre, angle, -r * 7 / 10, -r * 7 / 10),
        hand_point(centre, angle, -r, leaf / 8),
        hand_point(centre, angle, -r * 6 / 10, leaf / 2),
        hand_point(centre, angle, -(width / 2), leaf),
        hand_point(centre, angle, width / 2, leaf),
        hand_point(centre, angle, r * 6 / 10, leaf / 2),
        hand_point(centre, angle, r, leaf / 8),
        hand_point(centre, angle, r * 7 / 10, -r * 7 / 10),
      };
      hand_shape(ctx, points, ARRAY_LENGTH(points), s_palette.leaf, LEAF_OUTLINE, border);
      break;
    }
  }
}

// Draws a hand in the chosen style. The minute hand, drawn over the hour
// hand, gets a narrow border in the dial colour, so the two read as separate
// pieces where they cross.
static NOINLINE void draw_hand(GContext *ctx, GPoint centre, int32_t angle, int16_t length,
                      int16_t width, bool minute) {
  if (minute) {
    draw_hand_parts(ctx, centre, angle, length, width, minute, true);
  }
  draw_hand_parts(ctx, centre, angle, length, width, minute, false);
}

// A round cap over the centre, where the hands meet.
static NOINLINE void draw_hub(GContext *ctx, GPoint centre) {
  graphics_context_set_fill_color(ctx, s_palette.leaf);
  graphics_fill_circle(ctx, centre, LEAF_RADIUS + 1);
  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, LEAF_OUTLINE);
  graphics_draw_circle(ctx, centre, LEAF_RADIUS + 1);
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  graphics_fill_circle(ctx, centre, 1);
}

// ---------------------------------------------------------------------------
// Layout

// The information around the centre of the dial.
typedef enum {
  ITEM_WEATHER,
  ITEM_HEALTH,
  ITEM_DATE,
  ITEM_BATTERY,
  ITEM_COUNT,
} Item;

// A hand as two stretches, each with how far its shapes reach either side:
// from the centre to the end of the base, and from there to the tip.
typedef struct {
  GPoint centre;
  GPoint tip;
  GPoint leaf_end;
  int16_t inner;
  int16_t outer;
} Hand;

static Hand make_hand(GPoint centre, int32_t angle, int16_t length, int16_t width, bool minute) {
  const int16_t bulk = hand_bulk(minute);
  int16_t inner = LEAF_RADIUS / 2;
  int16_t outer = width / 2;
  switch (s_settings.hand_style) {
    case HANDS_BATON: inner = outer = slim_half_width(minute); break;
    case HANDS_DAUPHINE: inner = bulk; outer = bulk / 2; break;
    case HANDS_BREGUET: inner = slim_half_width(minute); outer = bulk; break;
    case HANDS_SWORD: inner = slim_half_width(minute); outer = bulk; break;
    default: break;
  }
  return (Hand) {
    .centre = centre,
    .tip = ray_point(centre, angle, length),
    .leaf_end = ray_point(centre, angle, length * LEAF_PERCENT / 100),
    .inner = inner,
    .outer = outer,
  };
}

static bool item_shown(Item item) {
  switch (item) {
    case ITEM_WEATHER: return s_settings.show_weather;
#if defined(PBL_HEALTH)
    case ITEM_HEALTH: return s_settings.show_health;
#endif
    case ITEM_DATE: return s_settings.show_date;
    case ITEM_BATTERY: return s_settings.show_battery;
    default: return false;
  }
}

// The area an item covers, relative to the point it is drawn at.
static NOINLINE GRect item_extent(Item item, const struct tm *t) {
  const int16_t complication_top = -(DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  int16_t width = 0;
  int16_t top = complication_top;
  int16_t bottom = complication_top + DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT;
  switch (item) {
    case ITEM_WEATHER: {
      char temperature[16];
      format_temperature(temperature, sizeof(temperature));
      width = text_width(temperature, s_date_font.font);
      int16_t below = text_width(weather_label(), s_label_font.font);
      if (weather_from_condition(s_condition) != WEATHER_UNKNOWN) {
        below = icon_size(24);
        bottom = top + DATE_HEIGHT + LINE_GAP + 1 + icon_size(16);
      }
      width = width > below ? width : below;
      break;
    }
#if defined(PBL_HEALTH)
    case ITEM_HEALTH: {
      char value[16];
      const char *label;
      format_metric(s_metric, value, sizeof(value), &label);
      const int16_t value_width = text_width(value, s_date_font.font);
      const int16_t label_width = text_width(label, s_label_font.font);
      width = value_width > label_width ? value_width : label_width;
      break;
    }
#endif
    case ITEM_DATE: {
      char day[4];
      char date[4];
      format_date(t, day, date);
      const int16_t day_width = text_width(day, s_date_font.font);
      const int16_t date_width = text_width(date, s_date_font.font);
      width = day_width > date_width ? day_width : date_width;
      top = -(DATE_HEIGHT * 2 + LINE_GAP) / 2;
      bottom = top + DATE_HEIGHT * 2 + LINE_GAP;
      break;
    }
    case ITEM_BATTERY:
      width = RING_RADIUS * 2 + 2;
      top = -RING_RADIUS - 1;
      bottom = RING_RADIUS + 1;
      break;
    default:
      break;
  }
  return GRect(-width / 2 - 1, top, width + 2, bottom - top);
}

// Whether a rectangle lies inside the ring of ticks.
static bool rect_in_dial(GRect rect, GRect bounds, GPoint centre) {
  const GPoint corners[] = {
    rect.origin,
    GPoint(rect.origin.x + rect.size.w, rect.origin.y),
    GPoint(rect.origin.x, rect.origin.y + rect.size.h),
    GPoint(rect.origin.x + rect.size.w, rect.origin.y + rect.size.h),
  };
  for (size_t i = 0; i < ARRAY_LENGTH(corners); i++) {
    const int32_t dx = corners[i].x - centre.x;
    const int32_t dy = corners[i].y - centre.y;
    if (dx == 0 && dy == 0) {
      // The centre itself is inside the dial; the watch cannot take the
      // angle of a zero-length line.
      continue;
    }
    const int32_t angle = atan2_lookup(dx, -dy);
    const int32_t limit = edge_distance(bounds, angle) - HOUR_TICK_LENGTH - AVOID_GAP;
    if (dx * dx + dy * dy > limit * limit) {
      return false;
    }
  }
  return true;
}

static bool hand_crosses(const Hand *hand, GRect rect) {
  return line_crosses_rect(hand->centre, hand->leaf_end,
                           rect_grow(rect, hand->inner + 1 + AVOID_GAP))
    || line_crosses_rect(hand->leaf_end, hand->tip,
                         rect_grow(rect, hand->outer + 1 + AVOID_GAP));
}

// Everything an item has to keep clear of, worked out once per redraw.
typedef struct {
  GRect bounds;
  GPoint centre;
  Hand hands[2];
  GRect numerals[12];  // Empty for hours without a number.
  GRect name;          // Empty when no name is shown.
} Obstacles;

// Whether an item could sit in a rectangle: inside the dial, clear of both
// hands, the other items, the hour numbers and the name.
static bool item_is_clear(Item item, GRect rect, const GRect *rects, const Obstacles *o) {
  if (!rect_in_dial(rect, o->bounds, o->centre)) {
    return false;
  }
  for (int i = 0; i < 2; i++) {
    if (hand_crosses(&o->hands[i], rect)) {
      return false;
    }
  }
  const GRect spaced = rect_grow(rect, AVOID_GAP);
  for (int i = 0; i < ITEM_COUNT; i++) {
    if (i != (int)item && item_shown(i) && rects_overlap(spaced, rects[i])) {
      return false;
    }
  }
  if (o->name.size.w && rects_overlap(spaced, o->name)) {
    return false;
  }
  for (int hour = 0; hour < 12; hour++) {
    if (o->numerals[hour].size.w && rects_overlap(spaced, o->numerals[hour])) {
      return false;
    }
  }
  return true;
}

// Moves each item around the centre and outward, by as little as possible, to
// where neither hand crosses it. With an item hidden, the others may go all
// the way round the dial into the space it leaves. An item with nowhere clear
// to go stays where it was, partly covered.
static NOINLINE void avoid_hands(GPoint *points, GRect *rects, const GRect *extents,
                        const Obstacles *o) {
  const GPoint centre = o->centre;
  int shown = 0;
  for (int item = 0; item < ITEM_COUNT; item++) {
    shown += item_shown(item) ? 1 : 0;
  }
  const int max_steps = shown < ITEM_COUNT ? AVOID_STEPS_ROUND : AVOID_STEPS;
  for (int item = 0; item < ITEM_COUNT; item++) {
    if (!item_shown(item)) {
      continue;
    }
    const int32_t dx = points[item].x - centre.x;
    const int32_t dy = points[item].y - centre.y;
    // Smallest move first: no move, then one step around either way, and so
    // on, with steps outward mixed in by their cost.
    bool placed = false;
    for (int cost = 0; cost <= max_steps + AVOID_PUSHES * AVOID_PUSH_COST && !placed; cost++) {
      for (int push = 0; push <= AVOID_PUSHES && !placed; push++) {
        const int steps = cost - push * AVOID_PUSH_COST;
        if (steps < 0 || steps > max_steps) {
          continue;
        }
        for (int direction = 1; direction >= -1 && !placed; direction -= 2) {
          // No turn, and half a turn, are the same either way.
          if ((steps == 0 || steps == AVOID_STEPS_ROUND) && direction < 0) {
            continue;
          }
          const int32_t angle = steps * direction * AVOID_STEP;
          const int32_t sin = sin_lookup(angle);
          const int32_t cos = cos_lookup(angle);
          const int32_t scale = 6 + push;
          const GPoint at = GPoint(
            centre.x + (dx * cos - dy * sin) / TRIG_MAX_RATIO * scale / 6,
            centre.y + (dx * sin + dy * cos) / TRIG_MAX_RATIO * scale / 6);
          const GRect rect = GRect(at.x + extents[item].origin.x, at.y + extents[item].origin.y,
                                   extents[item].size.w, extents[item].size.h);
          if (item_is_clear(item, rect, rects, o)) {
            points[item] = at;
            rects[item] = rect;
            placed = true;
          }
        }
      }
    }
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // The unobstructed area shrinks when a Timeline Quick View is showing.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GPoint centre = grect_center_point(&bounds);
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  fill_dial(ctx, layer_get_bounds(layer), bounds);

  draw_ticks(ctx, bounds, centre);

  const int32_t reach_x = edge_distance(bounds, TRIG_MAX_ANGLE / 4);
  const int32_t reach_y = edge_distance(bounds, 0);
  const int16_t row = reach_y * 29 / 100;

  // The name sits halfway between the 12 and the complications below it.
  const int16_t below_numeral = reach_y - HOUR_TICK_LENGTH - NUMERAL_GAP - CLASSIC_HEIGHT;
  const int16_t above_row = row + (DATE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  const int16_t name_y = centre.y - (below_numeral + above_row) / 2;
  GRect name = GRectZero;
  if (s_settings.dial_name[0]) {
    draw_text_box(ctx, s_settings.dial_name, s_date_font.font, GPoint(centre.x, name_y),
                  name_y - DATE_HEIGHT / 2, s_date_font.pad, DATE_HEIGHT, NAME_BOX_WIDTH);
    const int16_t width = text_width_in(s_settings.dial_name, s_date_font.font, NAME_BOX_WIDTH);
    name = GRect(centre.x - width / 2, name_y - DATE_HEIGHT / 2, width, DATE_HEIGHT);
  }

  // The minute hand reaches the minute ticks; the hour and second hands are
  // measured from just inside the hour ticks.
  const int16_t reach = reach_x < reach_y ? reach_x : reach_y;
  const int16_t inside_ticks = reach - HOUR_TICK_LENGTH - 4;
  const int16_t minute_length = reach - MINUTE_TICK_LENGTH;
  const int16_t hour_length = inside_ticks * 65 / 100;
  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;

  const int16_t side = reach_x * 32 / 100;
  // Kept off the stack, which drawing text in a bundled font needs.
  static GPoint points[ITEM_COUNT];
  points[ITEM_WEATHER] = GPoint(centre.x - side, centre.y - row);
  points[ITEM_HEALTH] = GPoint(centre.x + side, centre.y - row);
  points[ITEM_DATE] = GPoint(centre.x - reach_x * 30 / 100, centre.y + row);
  points[ITEM_BATTERY] = GPoint(centre.x + reach_x * 25 / 100, centre.y + row);
  // Where each item sits; the name comes last, for the hour numbers to avoid.
  static GRect extents[ITEM_COUNT];
  static GRect covers[ITEM_COUNT + 1];
  for (int i = 0; i < ITEM_COUNT; i++) {
    extents[i] = item_shown(i) ? item_extent(i, t) : GRectZero;
    covers[i] = GRect(points[i].x + extents[i].origin.x, points[i].y + extents[i].origin.y,
                      extents[i].size.w, extents[i].size.h);
  }
  covers[ITEM_COUNT] = name;

  if (s_settings.avoid_hands) {
    // Kept off the stack, which is small on the oldest watches.
    static Obstacles o;
    o = (Obstacles) {
      .bounds = bounds,
      .centre = centre,
      .hands = {
        make_hand(centre, hour_angle, hour_length, HOUR_HAND_WIDTH, false),
        make_hand(centre, minute_angle, minute_length, MINUTE_HAND_WIDTH, true),
      },
      .name = name,
    };
    for (int hour = 0; hour < 12; hour++) {
      o.numerals[hour] = hour_has_numeral(hour) ? numeral_rect(bounds, centre, hour) : GRectZero;
    }
    avoid_hands(points, covers, extents, &o);
  }
  draw_hours(ctx, bounds, centre, covers, ITEM_COUNT + 1);

  if (item_shown(ITEM_WEATHER)) {
    draw_weather(ctx, points[ITEM_WEATHER]);
  }
#if defined(PBL_HEALTH)
  if (item_shown(ITEM_HEALTH)) {
    draw_health(ctx, points[ITEM_HEALTH]);
  }
#endif
  if (item_shown(ITEM_DATE)) {
    draw_date(ctx, points[ITEM_DATE], t);
  }
  if (item_shown(ITEM_BATTERY)) {
    draw_battery_ring(ctx, points[ITEM_BATTERY]);
  }

  draw_hand(ctx, centre, hour_angle, hour_length, HOUR_HAND_WIDTH, false);
  draw_hand(ctx, centre, minute_angle, minute_length, MINUTE_HAND_WIDTH, true);
  draw_hub(ctx, centre);

  if (s_settings.second_hand) {
    // A thin line with a short tail, pinned by a dot over the other hands.
    const int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
    graphics_context_set_stroke_color(ctx, s_palette.second_hand);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, ray_point(centre, second_angle + TRIG_MAX_ANGLE / 2, LEAF_RADIUS * 2),
                       ray_point(centre, second_angle, inside_ticks + 2));
    graphics_context_set_fill_color(ctx, s_palette.second_hand);
    graphics_fill_circle(ctx, centre, LEAF_RADIUS / 2);
  }
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
  if (s_settings.show_weather && (units_changed & MINUTE_UNIT)
      && tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

// A second hand costs battery, so only tick every second while it is shown.
static void subscribe_ticks(void) {
  tick_timer_service_subscribe(s_settings.second_hand ? SECOND_UNIT : MINUTE_UNIT,
                               tick_handler);
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

static void unload_fonts(void) {
  styled_font_unload(&s_date_font);
  styled_font_unload(&s_label_font);
}

// Loads the chosen font style for the information. The hour numbers have
// their own style in every case.
static void load_fonts(void) {
  unload_fonts();
  styled_font_load(&s_date_font, s_settings.info_font, DATE_FONT, DATE_PAD, FONTS_DATE);
  styled_font_load(&s_label_font, s_settings.info_font, LABEL_FONT, LABEL_PAD, FONTS_LABEL);
}

static bool read_settings(DictionaryIterator *iterator) {
  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.dark = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_INFO_FONT))) {
    s_settings.info_font = tuple_int(tuple) % FONT_STYLE_COUNT;
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
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SECOND_HAND))) {
    s_settings.second_hand = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_WEATHER))) {
    s_settings.show_weather = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_HEALTH))) {
    s_settings.show_health = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_DATE))) {
    s_settings.show_date = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_BATTERY))) {
    s_settings.show_battery = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_AVOID_HANDS))) {
    s_settings.avoid_hands = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SECOND_HAND_COLOR))) {
    // A colour as 0xRRGGBB, or -1 for the same colour as the text.
    const int32_t color = tuple_int(tuple);
    s_settings.second_hand_argb = color < 0 ? 0 : GColorFromHEX(color).argb;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_HAND_STYLE))) {
    s_settings.hand_style = tuple_int(tuple) % HANDS_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DIAL_NAME)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_settings.dial_name, tuple->value->cstring, sizeof(s_settings.dial_name) - 1);
    s_settings.dial_name[sizeof(s_settings.dial_name) - 1] = '\0';
    changed = true;
  }
  return changed;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  const Tuple *temperature = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  const Tuple *condition = dict_find(iterator, MESSAGE_KEY_CONDITION);
  const Tuple *daytime = dict_find(iterator, MESSAGE_KEY_DAYTIME);
  if (temperature) {
    s_temperature = temperature->value->int32;
    persist_write_int(PERSIST_KEY_TEMPERATURE, s_temperature);
  }
  if (condition) {
    strncpy(s_condition, condition->value->cstring, sizeof(s_condition) - 1);
    s_condition[sizeof(s_condition) - 1] = '\0';
    persist_write_string(PERSIST_KEY_CONDITION, s_condition);
  }
  if (daytime) {
    s_daytime = daytime->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_DAYTIME, s_daytime);
  }

  if (read_settings(iterator)) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    load_fonts();
    window_set_background_color(s_window, s_palette.background);
    subscribe_ticks();
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
    if (size == settings_size(saved.version)) {
      memcpy(&s_settings, &saved, size);
      s_settings.version = SETTINGS_VERSION;
    }
  }
  apply_palette();

  if (persist_exists(PERSIST_KEY_TEMPERATURE)) {
    s_temperature = persist_read_int(PERSIST_KEY_TEMPERATURE);
  }
  if (persist_exists(PERSIST_KEY_CONDITION)) {
    persist_read_string(PERSIST_KEY_CONDITION, s_condition, sizeof(s_condition));
  }
  if (persist_exists(PERSIST_KEY_DAYTIME)) {
    s_daytime = persist_read_bool(PERSIST_KEY_DAYTIME);
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
  load_saved_state();
  load_fonts();
  s_battery = battery_state_service_peek();
  s_bluetooth_connected = connection_service_peek_pebble_app_connection();

  s_window = window_create();
  window_set_background_color(s_window, s_palette.background);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  subscribe_ticks();
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
#if defined(PBL_COLOR)
  unload_numerals();
#endif
  unload_fonts();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
