#include <pebble.h>

#include "font_styles.h"

// An analog watchface with four complication slots around the dial, at 12, 3,
// 6 and 9 o'clock. Each slot can show the weather, the date, the battery (with
// Bluetooth and Quiet Time alerts) or health data; tap the watch to cycle the
// health metric. Everything is configurable from the phone (see src/pkjs).

#define PERSIST_KEY_METRIC 1
#define PERSIST_KEY_TEMPERATURE 2
#define PERSIST_KEY_CONDITION 3
#define PERSIST_KEY_SETTINGS 4

#define WEATHER_REFRESH_MINUTES 30
#define LOW_BATTERY_PERCENT 20
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define VALUE_FONT FONT_KEY_GOTHIC_24_BOLD
  #define LABEL_FONT FONT_KEY_GOTHIC_18
  // Visible glyph heights and the blank space Pebble's fonts leave above them
  // (the bundled font styles have their own, in font_styles.h).
  #define VALUE_HEIGHT 17
  #define VALUE_PAD 7
  #define LABEL_HEIGHT 13
  #define LABEL_PAD 5
  #define SLOT_SIDE_WIDTH 64
  #define HOUR_HAND_WIDTH 9
  #define MINUTE_HAND_WIDTH 7
  #define HOUR_TICK_LENGTH 12
  #define MINUTE_TICK_LENGTH 6
  #define NUMERAL_RADIUS 12
#else
  #define VALUE_FONT FONT_KEY_GOTHIC_18_BOLD
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define VALUE_HEIGHT 13
  #define VALUE_PAD 5
  #define LABEL_HEIGHT 10
  #define LABEL_PAD 4
  #define SLOT_SIDE_WIDTH 48
  #define HOUR_HAND_WIDTH 7
  #define MINUTE_HAND_WIDTH 5
  #define HOUR_TICK_LENGTH 9
  #define MINUTE_TICK_LENGTH 4
  #define NUMERAL_RADIUS 9
#endif

#define SLOT_WIDE_WIDTH 90
#define LINE_GAP 2
#define EDGE_INSET 2

typedef enum {
  SLOT_NONE,
  SLOT_WEATHER,
  SLOT_DATE,
  SLOT_BATTERY,
  SLOT_HEALTH,
  SLOT_KIND_COUNT,
} SlotKind;

// Slot positions, clockwise from 12 o'clock.
enum { POSITION_TOP, POSITION_RIGHT, POSITION_BOTTOM, POSITION_LEFT, POSITION_COUNT };

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 3
typedef struct {
  uint8_t version;
  bool light_theme;
  uint8_t accent_argb;
  bool hour_numbers;
  bool second_hand;
  bool disconnect_vibe;
  uint8_t slots[POSITION_COUNT];
  // Added in version 2: degrees the whole dial is turned, clockwise.
  int16_t rotation;
  // Added in version 3: font styles for the hour numbers and for the
  // information (see font_styles.h).
  uint8_t number_font;
  uint8_t info_font;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 1: return offsetof(Settings, rotation);
    case 2: return offsetof(Settings, number_font);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

// Whether any of the four positions shows a kind of information.
static bool slot_shown(SlotKind kind) {
  for (int i = 0; i < POSITION_COUNT; i++) {
    if (s_settings.slots[i] == kind) {
      return true;
    }
  }
  return false;
}

typedef struct {
  GColor background;
  GColor foreground;
  GColor label;
  GColor minor_tick;
  GColor accent;
} Palette;

static Palette s_palette;

#define COLOR_WARNING PBL_IF_COLOR_ELSE(GColorRed, s_palette.foreground)
#define COLOR_CHARGING PBL_IF_COLOR_ELSE(GColorGreen, s_palette.foreground)

static Window *s_window;
static Layer *s_canvas;
static StyledFont s_numeral_font;
static StyledFont s_value_font;
static StyledFont s_label_font;

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
  if (slot_shown(SLOT_HEALTH) && (event != HealthEventSleepUpdate || s_metric == METRIC_SLEEP)) {
    layer_mark_dirty(s_canvas);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!slot_shown(SLOT_HEALTH)) {
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
    .light_theme = false,
    .accent_argb = GColorChromeYellowARGB8,
    .hour_numbers = false,
    .second_hand = false,
    .disconnect_vibe = true,
    .slots = { SLOT_WEATHER, SLOT_DATE, SLOT_HEALTH, SLOT_BATTERY },
  };
}

static void apply_palette(void) {
  const bool light = s_settings.light_theme;
  s_palette.background = light ? GColorWhite : GColorBlack;
  s_palette.foreground = light ? GColorBlack : GColorWhite;
#if defined(PBL_COLOR)
  s_palette.label = light ? GColorDarkGray : GColorLightGray;
  s_palette.minor_tick = light ? GColorLightGray : GColorDarkGray;
  s_palette.accent = (GColor) { .argb = s_settings.accent_argb };
#else
  s_palette.label = s_palette.foreground;
  s_palette.minor_tick = s_palette.foreground;
  s_palette.accent = s_palette.foreground;
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

// An angle on the dial, measured from 12 o'clock, turned by the rotation the
// wearer has chosen.
static int32_t dial_angle(int32_t angle) {
  return angle + TRIG_MAX_ANGLE * s_settings.rotation / 360;
}

// ---------------------------------------------------------------------------
// Turned text
//
// Pebble cannot draw text at an angle. On round watches, when the dial is
// turned, each piece of text (an hour number, or a slot's information with
// its icons) is drawn upright in the middle of the screen, copied into a
// small image and cleared. Once the ticks are drawn, the images are drawn
// back turned at their places, blending neighbouring pixels so the edges
// stay smooth. Everything else is drawn turned directly.
//
// Code that draws a piece calls piece_begin() for where to draw it, draws it
// there, then calls piece_end(). Without a turn they do nothing.

#if defined(PBL_ROUND)
#define MAX_PIECES 16

typedef struct {
  GBitmap *image;
  GPoint at;
} Piece;

static Piece s_pieces[MAX_PIECES];
static int s_piece_count;
static bool s_collecting;
static GPoint s_scratch;
static GSize s_piece_size;
static GBitmap *s_piece_image;

// Where to draw a piece of the given size that belongs at a point.
static GPoint piece_begin(GPoint at, GSize size) {
  s_piece_image = NULL;
  if (!s_collecting || s_piece_count == MAX_PIECES) {
    return at;
  }
  // Without memory for the copy, the piece is drawn upright where it goes.
  s_piece_image = gbitmap_create_blank(size, GBitmapFormat8Bit);
  if (!s_piece_image) {
    return at;
  }
  s_piece_size = size;
  s_pieces[s_piece_count].at = at;
  return s_scratch;
}

// Copies the piece just drawn and clears it from the screen.
static void piece_end(GContext *ctx) {
  if (!s_piece_image) {
    return;
  }
  const GSize size = s_piece_size;
  const GPoint origin = GPoint(s_scratch.x - size.w / 2, s_scratch.y - size.h / 2);
  uint8_t *copy = gbitmap_get_data(s_piece_image);
  const uint16_t stride = gbitmap_get_bytes_per_row(s_piece_image);
  const uint8_t background = s_palette.background.argb;
  GBitmap *screen = graphics_capture_frame_buffer(ctx);
  for (int16_t y = 0; y < size.h; y++) {
    uint8_t *out = copy + y * stride;
    const GBitmapDataRowInfo row = gbitmap_get_data_row_info(screen, origin.y + y);
    for (int16_t x = 0; x < size.w; x++) {
      const int16_t sx = origin.x + x;
      out[x] = sx >= row.min_x && sx <= row.max_x ? row.data[sx] : background;
    }
  }
  graphics_release_frame_buffer(ctx, screen);

  // Text can run past the piece's box, so clear a margin around it too.
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, GRect(origin.x - 24, origin.y - 12, size.w + 48, size.h + 24),
                     0, GCornerNone);
  s_pieces[s_piece_count++].image = s_piece_image;
  s_piece_image = NULL;
}

// Colour channels run from 0 to 3, two bits each in a GColor.
#define CHANNEL(argb, shift) (((argb) >> (shift)) & 3)

// Draws a piece turned about its centre, which lands on a point. Each screen
// pixel takes a blend of the four nearest pixels of the piece, weighted by
// how close they are; where the piece is only background, what is already on
// the screen shows through, so the piece sits cleanly over the ticks.
static void blend_piece(GBitmap *screen, const Piece *piece, int32_t angle, uint8_t background) {
  const GSize size = gbitmap_get_bounds(piece->image).size;
  const uint8_t *data = gbitmap_get_data(piece->image);
  const uint16_t stride = gbitmap_get_bytes_per_row(piece->image);
  const int32_t sin = sin_lookup(angle);
  const int32_t cos = cos_lookup(angle);
  // Far enough from the centre to cover the turned piece.
  const int16_t reach = (size.w + size.h) / 2 + 1;
  for (int16_t dy = -reach; dy <= reach; dy++) {
    const GBitmapDataRowInfo row = gbitmap_get_data_row_info(screen, piece->at.y + dy);
    for (int16_t dx = -reach; dx <= reach; dx++) {
      const int16_t x = piece->at.x + dx;
      if (x < row.min_x || x > row.max_x) {
        continue;
      }
      // The point in the piece, in 256ths of a pixel, found by turning back.
      const int32_t px = ((dx * cos + dy * sin) >> 8) + (size.w / 2) * 256;
      const int32_t py = ((dy * cos - dx * sin) >> 8) + (size.h / 2) * 256;
      const int32_t x0 = px >> 8;
      const int32_t y0 = py >> 8;
      if (x0 < -1 || y0 < -1 || x0 >= size.w || y0 >= size.h) {
        continue;
      }
      const int32_t fx = px & 255;
      const int32_t fy = py & 255;
      const int32_t weights[4] = {
        (256 - fx) * (256 - fy), fx * (256 - fy), (256 - fx) * fy, fx * fy,
      };
      const uint8_t under = row.data[x];
      int32_t r = 0, g = 0, b = 0;
      bool any = false;
      for (int i = 0; i < 4; i++) {
        const int32_t sx = x0 + (i & 1);
        const int32_t sy = y0 + (i >> 1);
        uint8_t colour = background;
        if (sx >= 0 && sy >= 0 && sx < size.w && sy < size.h) {
          colour = data[sy * stride + sx];
        }
        if (colour == background) {
          colour = under;
        } else if (weights[i]) {
          any = true;
        }
        r += CHANNEL(colour, 4) * weights[i];
        g += CHANNEL(colour, 2) * weights[i];
        b += CHANNEL(colour, 0) * weights[i];
      }
      if (any) {
        row.data[x] = 0xC0 | ((r + 32768) >> 16) << 4 | ((g + 32768) >> 16) << 2
          | ((b + 32768) >> 16);
      }
    }
  }
}

static void draw_pieces(GContext *ctx) {
  GBitmap *screen = graphics_capture_frame_buffer(ctx);
  const int32_t angle = TRIG_MAX_ANGLE * s_settings.rotation / 360;
  for (int i = 0; i < s_piece_count; i++) {
    if (screen) {
      blend_piece(screen, &s_pieces[i], angle, s_palette.background.argb);
    }
    gbitmap_destroy(s_pieces[i].image);
  }
  if (screen) {
    graphics_release_frame_buffer(ctx, screen);
  }
  s_piece_count = 0;
}
#else
static GPoint piece_begin(GPoint at, GSize size) {
  return at;
}

static void piece_end(GContext *ctx) {
}
#endif

// ---------------------------------------------------------------------------
// Drawing

static void draw_ticks(GContext *ctx, GRect bounds, GPoint centre) {
  for (int i = 0; i < 60; i++) {
    const bool hour = i % 5 == 0;
    const int32_t angle = dial_angle(TRIG_MAX_ANGLE * i / 60);
    const int32_t outer = edge_distance(bounds, angle);
    const int32_t length = hour ? HOUR_TICK_LENGTH : MINUTE_TICK_LENGTH;
    graphics_context_set_stroke_color(ctx, hour ? s_palette.foreground : s_palette.minor_tick);
    graphics_context_set_stroke_width(ctx, hour ? 3 : 1);
    graphics_draw_line(ctx, ray_point(centre, angle, outer - length),
                       ray_point(centre, angle, outer));
  }

  if (s_settings.hour_numbers && s_settings.slots[POSITION_TOP] == SLOT_NONE) {
    return;  // The accent "12" numeral marks the top instead.
  }

  // An accent triangle marks 12 o'clock, pointing in to the centre.
  const int32_t up = dial_angle(0);
  const int32_t top = edge_distance(bounds, up);
  const int16_t size = HOUR_TICK_LENGTH / 2 + 2;
  const GPoint tip = ray_point(centre, up, top - HOUR_TICK_LENGTH - 3);
  const GPoint base = ray_point(centre, up, top - HOUR_TICK_LENGTH - 3 + size);
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) {
      ray_point(base, up - TRIG_MAX_ANGLE / 4, size),
      ray_point(base, up + TRIG_MAX_ANGLE / 4, size),
      tip,
    },
  };
  GPath *marker = gpath_create(&info);
  graphics_context_set_fill_color(ctx, s_palette.accent);
  gpath_draw_filled(ctx, marker);
  gpath_destroy(marker);
}

static void draw_text_line(GContext *ctx, const char *text, GFont font, GColor color,
                           GPoint centre, int16_t visible_top, int16_t pad,
                           int16_t height, int16_t width) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font,
                     GRect(centre.x - width / 2, visible_top - pad, width, height + pad * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Draws a bold value above a small label, centred on a point.
static void draw_complication(GContext *ctx, GPoint centre, int16_t width,
                              const char *value, GColor value_color,
                              const char *label) {
  const int16_t top = centre.y - (VALUE_HEIGHT + LINE_GAP + LABEL_HEIGHT) / 2;
  draw_text_line(ctx, value, s_value_font.font, value_color, centre,
                 top, s_value_font.pad, VALUE_HEIGHT, width);
  if (label) {
    draw_text_line(ctx, label, s_label_font.font, s_palette.label, centre,
                   top + VALUE_HEIGHT + LINE_GAP, s_label_font.pad, LABEL_HEIGHT, width);
  }
}

static void draw_battery_icon(GContext *ctx, GPoint centre) {
  const GRect body = GRect(centre.x - 9, centre.y - 4, 16, 9);
  const GColor fill = s_battery.is_charging
    ? COLOR_CHARGING
    : s_battery.charge_percent <= LOW_BATTERY_PERCENT ? COLOR_WARNING : s_palette.foreground;

  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, body);
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  graphics_fill_rect(ctx, GRect(body.origin.x + body.size.w, centre.y - 2, 2, 5), 0, GCornerNone);

  const int16_t inner_w = body.size.w - 4;
  const int16_t level = inner_w * s_battery.charge_percent / 100;
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(body.origin.x + 2, body.origin.y + 2,
                                level > 0 ? level : 1, body.size.h - 4), 0, GCornerNone);
}

// Bluetooth rune, struck through, shown only while the phone is disconnected.
static void draw_bluetooth_off_icon(GContext *ctx, GPoint origin) {
  graphics_context_set_stroke_color(ctx, COLOR_WARNING);
  graphics_context_set_stroke_width(ctx, 1);
  const GPoint p[] = {
    { 0, 3 }, { 6, 9 }, { 3, 12 }, { 3, 0 }, { 6, 3 }, { 0, 9 },
  };
  for (size_t i = 1; i < ARRAY_LENGTH(p); i++) {
    graphics_draw_line(ctx, GPoint(origin.x + p[i - 1].x, origin.y + p[i - 1].y),
                       GPoint(origin.x + p[i].x, origin.y + p[i].y));
  }
  graphics_draw_line(ctx, GPoint(origin.x - 1, origin.y + 12),
                     GPoint(origin.x + 7, origin.y));
}

// Crescent moon for Quiet Time.
static void draw_quiet_time_icon(GContext *ctx, GPoint origin) {
  const GPoint centre = GPoint(origin.x + 6, origin.y + 6);
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  graphics_fill_circle(ctx, centre, 6);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, GPoint(centre.x + 3, centre.y - 2), 5);
}

static void draw_status_icons(GContext *ctx, GPoint centre) {
  const bool quiet = quiet_time_is_active();
  const bool disconnected = !s_bluetooth_connected;
  const int16_t icon_w = 12;
  const int16_t spacing = 4;
  const int count = (quiet ? 1 : 0) + (disconnected ? 1 : 0);
  if (count == 0) {
    return;
  }

  int16_t x = centre.x - (count * icon_w + (count - 1) * spacing) / 2;
  const int16_t y = centre.y - 6;
  if (disconnected) {
    draw_bluetooth_off_icon(ctx, GPoint(x + 3, y));
    x += icon_w + spacing;
  }
  if (quiet) {
    draw_quiet_time_icon(ctx, GPoint(x, y));
  }
}

// A hand that is widest at the centre and tapers to a point at its tip, so it
// covers little of the information it passes over. A short tail balances it
// on the other side of the centre.
static void draw_hand(GContext *ctx, GPoint centre, int32_t angle,
                      int16_t length, int16_t width, int16_t tail) {
  const int32_t side = angle + TRIG_MAX_ANGLE / 4;
  const GPoint back = ray_point(centre, angle + TRIG_MAX_ANGLE / 2, tail);
  const int16_t half = width / 2;
  GPoint points[] = {
    ray_point(back, side, half / 2),
    ray_point(centre, side, half),
    ray_point(centre, angle, length),
    ray_point(centre, side + TRIG_MAX_ANGLE / 2, half),
    ray_point(back, side + TRIG_MAX_ANGLE / 2, half / 2),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *hand = gpath_create(&info);
  // A thin dark border keeps the hands legible where they cross text. The
  // outline in the hand's colour fills the narrow tip, which a fill alone
  // leaves gaps in.
  graphics_context_set_stroke_color(ctx, s_palette.background);
  graphics_context_set_stroke_width(ctx, 4);
  gpath_draw_outline(ctx, hand);
  graphics_context_set_fill_color(ctx, s_palette.foreground);
  gpath_draw_filled(ctx, hand);
  graphics_context_set_stroke_color(ctx, s_palette.foreground);
  graphics_context_set_stroke_width(ctx, 2);
  gpath_draw_outline(ctx, hand);
  gpath_destroy(hand);
}

// Hour numbers sit just inside the ticks. A number is left out where a
// complication occupies its position, so they never overlap.
static void draw_hour_numbers(GContext *ctx, GRect bounds, GPoint centre) {
  static const int8_t slot_for_hour[12] = {
    POSITION_TOP, -1, -1, POSITION_RIGHT, -1, -1,
    POSITION_BOTTOM, -1, -1, POSITION_LEFT, -1, -1,
  };
  for (int hour = 0; hour < 12; hour++) {
    const int8_t slot = slot_for_hour[hour];
    if (slot >= 0 && s_settings.slots[slot] != SLOT_NONE) {
      continue;
    }
    const int32_t angle = dial_angle(TRIG_MAX_ANGLE * hour / 12);
    const int32_t distance = edge_distance(bounds, angle)
      - HOUR_TICK_LENGTH - 3 - NUMERAL_RADIUS;
    const GPoint at = piece_begin(ray_point(centre, angle, distance),
                                  GSize(NUMERAL_RADIUS * 3, VALUE_HEIGHT + 8));
    char text[3];
    snprintf(text, sizeof(text), "%d", hour == 0 ? 12 : hour);
    draw_text_line(ctx, text, s_numeral_font.font,
                   hour == 0 ? s_palette.accent : s_palette.foreground,
                   at, at.y - VALUE_HEIGHT / 2, s_numeral_font.pad, VALUE_HEIGHT,
                   NUMERAL_RADIUS * 4);
    piece_end(ctx);
  }
}

static void draw_weather(GContext *ctx, GPoint at, int16_t width) {
  char temperature[16];
  if (s_temperature == NO_TEMPERATURE) {
    snprintf(temperature, sizeof(temperature), "--\xc2\xb0");
  } else {
    snprintf(temperature, sizeof(temperature), "%d\xc2\xb0", (int)s_temperature);
  }
  draw_complication(ctx, at, width, temperature, s_palette.foreground,
                    s_condition[0] ? s_condition : "WEATHER");
}

static void draw_date(GContext *ctx, GPoint at, int16_t width, const struct tm *t) {
  char day[4];
  char date[4];
  strftime(day, sizeof(day), "%a", t);
  for (char *c = day; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
  strftime(date, sizeof(date), "%d", t);
  draw_complication(ctx, at, width, date, s_palette.accent, day);
}

// Battery percentage and gauge, with any alerts above them.
static void draw_battery(GContext *ctx, GPoint at, int16_t width) {
  char percent[8];
  snprintf(percent, sizeof(percent), "%d%%", s_battery.charge_percent);
  const bool low = !s_battery.is_charging && s_battery.charge_percent <= LOW_BATTERY_PERCENT;
  draw_complication(ctx, at, width, percent,
                    low ? COLOR_WARNING : s_palette.foreground, NULL);
  draw_battery_icon(ctx, GPoint(at.x, at.y + VALUE_HEIGHT / 2 + LINE_GAP + 4));
  draw_status_icons(ctx, GPoint(at.x, at.y - VALUE_HEIGHT - 6));
}

// Health data, or the month and year on watches without it.
static void draw_health(GContext *ctx, GPoint at, int16_t width, const struct tm *t) {
  char value[16];
#if defined(PBL_HEALTH)
  const char *label;
  format_metric(s_metric, value, sizeof(value), &label);
#else
  char label[8];
  strftime(value, sizeof(value), "%b", t);
  strftime(label, sizeof(label), "%Y", t);
#endif
  draw_complication(ctx, at, width, value, s_palette.foreground, label);
}

static void draw_slot(GContext *ctx, SlotKind kind, GPoint at, int16_t width,
                      const struct tm *t) {
  switch (kind) {
    case SLOT_WEATHER: draw_weather(ctx, at, width); break;
    case SLOT_DATE: draw_date(ctx, at, width, t); break;
    case SLOT_BATTERY: draw_battery(ctx, at, width); break;
    case SLOT_HEALTH: draw_health(ctx, at, width, t); break;
    default: break;
  }
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

#if defined(PBL_ROUND)
  // With the dial turned, the text is collected first (see "Turned text").
  s_collecting = s_settings.rotation != 0;
  const GRect screen = layer_get_bounds(layer);
  s_scratch = grect_center_point(&screen);
  if (!s_collecting) {
    draw_ticks(ctx, bounds, centre);
  }
#else
  draw_ticks(ctx, bounds, centre);
#endif
  if (s_settings.hour_numbers) {
    draw_hour_numbers(ctx, bounds, centre);
  }

  const int32_t reach_x = edge_distance(bounds, TRIG_MAX_ANGLE / 4);
  const int32_t reach_y = edge_distance(bounds, 0);

  // Each slot sits part of the way out towards the edge along its turned
  // direction. Its text is turned with it on round watches, and stays upright
  // on rectangular ones.
  for (int position = 0; position < POSITION_COUNT; position++) {
    if (s_settings.slots[position] == SLOT_NONE) {
      continue;
    }
    const int32_t angle = dial_angle(TRIG_MAX_ANGLE * position / POSITION_COUNT);
    const int32_t percent = position % 2 ? 52 : 50;
    const int16_t width = position % 2 ? SLOT_SIDE_WIDTH : SLOT_WIDE_WIDTH;
    // Tall enough for the battery's icons above and gauge below its value.
    const GPoint at = piece_begin(
      ray_point(centre, angle, edge_distance(bounds, angle) * percent / 100),
      GSize(width + 4, VALUE_HEIGHT * 2 + 28));
    draw_slot(ctx, s_settings.slots[position], at, width, t);
    piece_end(ctx);
  }

#if defined(PBL_ROUND)
  if (s_collecting) {
    s_collecting = false;
    draw_ticks(ctx, bounds, centre);
    draw_pieces(ctx);
  }
#endif

  // Hands on top of everything.
  const int16_t minute_length = (reach_x < reach_y ? reach_x : reach_y) - HOUR_TICK_LENGTH - 4;
  const int16_t hour_length = minute_length * 62 / 100;
  const int32_t minute_angle = dial_angle(TRIG_MAX_ANGLE * t->tm_min / 60);
  const int32_t hour_angle = dial_angle(TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720);
  draw_hand(ctx, centre, hour_angle, hour_length, HOUR_HAND_WIDTH, 6);
  draw_hand(ctx, centre, minute_angle, minute_length, MINUTE_HAND_WIDTH, 8);

  if (s_settings.second_hand) {
    const int32_t second_angle = dial_angle(TRIG_MAX_ANGLE * t->tm_sec / 60);
    graphics_context_set_stroke_color(ctx, s_palette.accent);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, ray_point(centre, second_angle + TRIG_MAX_ANGLE / 2, 12),
                       ray_point(centre, second_angle, minute_length + 2));
  }

  graphics_context_set_fill_color(ctx, s_palette.accent);
  graphics_fill_circle(ctx, centre, HOUR_HAND_WIDTH / 2 + 2);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, centre, 2);
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
  if (slot_shown(SLOT_WEATHER) && (units_changed & MINUTE_UNIT)
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
  styled_font_unload(&s_numeral_font);
  styled_font_unload(&s_value_font);
  styled_font_unload(&s_label_font);
}

// Loads the chosen font styles: one for the hour numbers, one for the
// information.
static void load_fonts(void) {
  unload_fonts();
  styled_font_load(&s_numeral_font, s_settings.number_font, VALUE_FONT, VALUE_PAD,
                   FONTS_NUMERAL);
  styled_font_load(&s_value_font, s_settings.info_font, VALUE_FONT, VALUE_PAD, FONTS_VALUE);
  styled_font_load(&s_label_font, s_settings.info_font, LABEL_FONT, LABEL_PAD, FONTS_LABEL);
}

static bool read_settings(DictionaryIterator *iterator) {
  static const uint32_t *slot_keys[POSITION_COUNT] = {
    &MESSAGE_KEY_SLOT_TOP, &MESSAGE_KEY_SLOT_RIGHT, &MESSAGE_KEY_SLOT_BOTTOM, &MESSAGE_KEY_SLOT_LEFT,
  };
  bool changed = false;
  const Tuple *tuple;

  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.light_theme = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_ACCENT_COLOR))) {
    s_settings.accent_argb = GColorFromHEX(tuple_int(tuple)).argb;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_HOUR_NUMBERS))) {
    s_settings.hour_numbers = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SECOND_HAND))) {
    s_settings.second_hand = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DISCONNECT_VIBE))) {
    s_settings.disconnect_vibe = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NUMBER_FONT))) {
    s_settings.number_font = tuple_int(tuple) % FONT_STYLE_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_INFO_FONT))) {
    s_settings.info_font = tuple_int(tuple) % FONT_STYLE_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_ROTATION))) {
    const int32_t degrees = tuple_int(tuple) % 360;
    s_settings.rotation = degrees > 180 ? degrees - 360 : degrees < -180 ? degrees + 360 : degrees;
    changed = true;
  }
  for (int i = 0; i < POSITION_COUNT; i++) {
    if ((tuple = dict_find(iterator, *slot_keys[i]))) {
      const int32_t kind = tuple_int(tuple);
      s_settings.slots[i] = kind >= 0 && kind < SLOT_KIND_COUNT ? kind : SLOT_NONE;
      changed = true;
    }
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
  unload_fonts();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
