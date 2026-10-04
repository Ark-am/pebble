#include <pebble.h>

#include "font_styles.h"

// A classic dress-watch face: a round dial with a minute scale and railroad
// track around the edge, Breguet-style hour numbers, slim leaf hands, a small
// seconds dial at half past seven and, if chosen, a matching small dial at half
// past four with the date, weather, battery or health data. The dial colour,
// the running seconds, the name on the dial and that information are
// configurable from the phone (see src/pkjs).

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_METRIC 2
#define PERSIST_KEY_TEMPERATURE 3
#define PERSIST_KEY_CONDITION 4

#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN
#define LOW_BATTERY_PERCENT 20

#if PBL_DISPLAY_WIDTH >= 200
  #define NUMERAL_FONT RESOURCE_ID_FONT_NUMERALS_22
  #define NAME_FONT FONT_KEY_GOTHIC_14_BOLD
  #define SCALE_FONT FONT_KEY_GOTHIC_09
  #define NAME_HEIGHT 10
  #define NAME_PAD 4
  #define NAME_SPACING 3
  #define SCALE_BAND 11
  #define TRACK_WIDTH 5
  #define HOUR_HAND_WIDTH 8
  #define MINUTE_HAND_WIDTH 6
  #define HUB_RADIUS 4
  #define VALUE_FONT FONT_KEY_GOTHIC_18_BOLD
  #define VALUE_HEIGHT 13
  #define VALUE_PAD 5
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define LABEL_HEIGHT 10
  #define LABEL_PAD 4
#else
  #define NUMERAL_FONT RESOURCE_ID_FONT_NUMERALS_16
  #define NAME_FONT FONT_KEY_GOTHIC_14_BOLD
  #define SCALE_FONT FONT_KEY_GOTHIC_09
  #define NAME_HEIGHT 10
  #define NAME_PAD 4
  #define NAME_SPACING 2
  #define SCALE_BAND 9
  #define TRACK_WIDTH 4
  #define HOUR_HAND_WIDTH 6
  #define MINUTE_HAND_WIDTH 5
  #define HUB_RADIUS 3
  #define VALUE_FONT FONT_KEY_GOTHIC_14_BOLD
  #define VALUE_HEIGHT 10
  #define VALUE_PAD 4
  #define LABEL_FONT FONT_KEY_GOTHIC_09
  #define LABEL_HEIGHT 7
  #define LABEL_PAD 2
#endif

#define EDGE_INSET 1
#define NUMERAL_GAP 3

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 3
typedef struct {
  uint8_t version;
  uint8_t dial;
  bool seconds;
  char dial_name[16];
  // Added in version 2.
  uint8_t info;
  // Added in version 3: font styles for the hour numbers and for the name
  // and information (see font_styles.h).
  uint8_t number_font;
  uint8_t info_font;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 1: return offsetof(Settings, info);
    case 2: return offsetof(Settings, number_font);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

// Values match the options on the settings page.
typedef enum {
  DIAL_SALMON,
  DIAL_NAVY,
  DIAL_SILVER,
  DIAL_BLACK_GOLD,
  DIAL_AZURE,
  DIAL_COUNT,
} Dial;

// What the small dial at half past four shows. Values match the options on the
// settings page.
typedef enum {
  INFO_NONE,
  INFO_DATE,
  INFO_WEATHER,
  INFO_BATTERY,
  INFO_HEALTH,
  INFO_COUNT,
} Info;

typedef struct {
  GColor dial;
  GColor print;   // The minute scale, track and name.
  GColor metal;   // Hour numbers and hands.
  GColor edge;    // The shadow and outline that set the metal off the dial.
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static StyledFont s_numeral_font;
static StyledFont s_name_font;
static GFont s_scale_font;
static StyledFont s_value_font;
static StyledFont s_label_font;

static BatteryChargeState s_battery;
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
      snprintf(value, size, "%dh%02d", (int)(minutes / 60), (int)(minutes % 60));
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
  if (s_settings.info == INFO_HEALTH
      && (event != HealthEventSleepUpdate || s_metric == METRIC_SLEEP)) {
    layer_mark_dirty(s_canvas);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_settings.info != INFO_HEALTH) {
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
    .dial = DIAL_SALMON,
    .seconds = true,
    .dial_name = "BALTIC",
    .info = INFO_NONE,
  };
}

static void apply_palette(void) {
#if defined(PBL_COLOR)
  switch (s_settings.dial) {
    case DIAL_NAVY:
      s_palette = (Palette) { GColorOxfordBlue, GColorLightGray, GColorWhite, GColorBlack };
      break;
    case DIAL_SILVER:
      // Blued numbers and hands, as on a silver dial.
      s_palette = (Palette) { GColorLightGray, GColorBlack, GColorOxfordBlue, GColorWhite };
      break;
    case DIAL_BLACK_GOLD:
      s_palette = (Palette) { GColorBlack, GColorRajah, GColorRajah, GColorWindsorTan };
      break;
    case DIAL_AZURE:
      s_palette = (Palette) { GColorCobaltBlue, GColorWhite, GColorWhite, GColorOxfordBlue };
      break;
    default:
      s_palette = (Palette) { GColorMelon, GColorBulgarianRose, GColorWhite, GColorDarkGray };
      break;
  }
#else
  const bool light = s_settings.dial == DIAL_SALMON || s_settings.dial == DIAL_SILVER;
  const GColor ink = light ? GColorBlack : GColorWhite;
  const GColor paper = light ? GColorWhite : GColorBlack;
  s_palette = (Palette) { paper, ink, ink, paper };
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

// ---------------------------------------------------------------------------
// Drawing

static void draw_text_centred(GContext *ctx, const char *text, GFont font, GColor color,
                              GPoint at, int16_t visible_height, int16_t pad) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font,
                     GRect(at.x - 30, at.y - visible_height / 2 - pad, 60,
                           visible_height + pad * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// The minute scale (5 to 60) in an outer band, then a railroad track with a
// mark for every minute.
static void draw_scale(GContext *ctx, GPoint centre, int16_t radius) {
  const int16_t band_inner = radius - SCALE_BAND;
  const int16_t track_inner = band_inner - TRACK_WIDTH;
  graphics_context_set_stroke_color(ctx, s_palette.print);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, centre, radius);
  graphics_draw_circle(ctx, centre, band_inner);
  graphics_draw_circle(ctx, centre, track_inner);
  for (int i = 0; i < 60; i++) {
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    graphics_draw_line(ctx, ray_point(centre, angle, track_inner),
                       ray_point(centre, angle, band_inner));
    if (i % 5 == 0) {
      char text[3];
      snprintf(text, sizeof(text), "%d", i == 0 ? 60 : i);
      draw_text_centred(ctx, text, s_scale_font, s_palette.print,
                        ray_point(centre, angle, radius - SCALE_BAND / 2), 7, 2);
    }
  }
}

// The hour numbers, upright and set off by a shadow like applied metal. The 7
// and 8 give way to the seconds dial, and the 4 and 5 to the information dial
// when one is shown.
static void draw_numerals(GContext *ctx, GPoint centre, int16_t distance) {
  const GSize size = graphics_text_layout_get_content_size(
    "12", s_numeral_font.font, GRect(0, 0, 80, 60), GTextOverflowModeFill, GTextAlignmentCenter);
  for (int hour = 1; hour <= 12; hour++) {
    if (hour == 7 || hour == 8 || (s_settings.info != INFO_NONE && (hour == 4 || hour == 5))) {
      continue;
    }
    char text[3];
    snprintf(text, sizeof(text), "%d", hour);
    const GPoint at = ray_point(centre, TRIG_MAX_ANGLE * hour / 12, distance);
    // Custom fonts leave space above the digits for taller characters, so the
    // box is lifted by a quarter of its height to centre the digits.
    const GRect box = GRect(at.x - 30, at.y - size.h / 2 - size.h / 4, 60, size.h + 4);
    graphics_context_set_text_color(ctx, s_palette.edge);
    graphics_draw_text(ctx, text, s_numeral_font.font,
                       GRect(box.origin.x + 1, box.origin.y + 1, box.size.w, box.size.h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, s_palette.metal);
    graphics_draw_text(ctx, text, s_numeral_font.font, box, GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
  }
}

// The name, in widely spaced capitals.
static void draw_name(GContext *ctx, GPoint at) {
  const char *name = s_settings.dial_name;
  const int length = strlen(name);
  if (length == 0) {
    return;
  }
  int16_t widths[sizeof(s_settings.dial_name)];
  int16_t total = 0;
  for (int i = 0; i < length; i++) {
    const char letter[2] = { name[i], '\0' };
    widths[i] = graphics_text_layout_get_content_size(
      letter, s_name_font.font, GRect(0, 0, 40, 30), GTextOverflowModeFill,
      GTextAlignmentLeft).w;
    total += widths[i] + (i ? NAME_SPACING : 0);
  }
  int16_t x = at.x - total / 2;
  graphics_context_set_text_color(ctx, s_palette.print);
  for (int i = 0; i < length; i++) {
    const char letter[2] = { name[i], '\0' };
    graphics_draw_text(ctx, letter, s_name_font.font,
                       GRect(x - 2, at.y - NAME_HEIGHT / 2 - s_name_font.pad, widths[i] + 4,
                             NAME_HEIGHT + s_name_font.pad * 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    x += widths[i] + NAME_SPACING;
  }
}

// A small dial with a mark every five seconds, and its hand while the running
// seconds are switched on.
static void draw_seconds_dial(GContext *ctx, GPoint at, int16_t radius, const struct tm *t) {
  graphics_context_set_stroke_color(ctx, s_palette.print);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, at, radius);
  for (int i = 0; i < 12; i++) {
    const int32_t angle = TRIG_MAX_ANGLE * i / 12;
    const int16_t length = i % 3 == 0 ? radius / 3 : radius / 6;
    graphics_draw_line(ctx, ray_point(at, angle, radius - length), ray_point(at, angle, radius));
  }
  if (!s_settings.seconds) {
    return;
  }
  const int32_t angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  graphics_context_set_stroke_color(ctx, s_palette.metal);
  graphics_draw_line(ctx, ray_point(at, angle + TRIG_MAX_ANGLE / 2, radius / 4),
                     ray_point(at, angle, radius - 2));
  graphics_context_set_fill_color(ctx, s_palette.metal);
  graphics_fill_circle(ctx, at, 2);
}

// A value above a small label, inside a ring that matches the seconds dial.
// The original Pebble has no health data, so the health choice shows the date
// there.
static void draw_info_dial(GContext *ctx, GPoint at, int16_t radius, const struct tm *t) {
  char value[16];
  char label_buffer[16];
  const char *label = label_buffer;
  GColor value_color = s_palette.metal;
  switch (s_settings.info) {
    case INFO_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        snprintf(value, sizeof(value), "--\xc2\xb0");
      } else {
        snprintf(value, sizeof(value), "%d\xc2\xb0", (int)s_temperature);
      }
      label = s_condition[0] ? s_condition : "WEATHER";
      break;
    case INFO_BATTERY:
      snprintf(value, sizeof(value), "%d%%", s_battery.charge_percent);
      label = s_battery.is_charging ? "CHARGING" : "BATTERY";
#if defined(PBL_COLOR)
      if (!s_battery.is_charging && s_battery.charge_percent <= LOW_BATTERY_PERCENT) {
        value_color = GColorRed;
      }
#endif
      break;
#if defined(PBL_HEALTH)
    case INFO_HEALTH:
      format_metric(s_metric, value, sizeof(value), &label);
      break;
#endif
    case INFO_DATE:
    default:
      snprintf(value, sizeof(value), "%d", t->tm_mday);
      strftime(label_buffer, 4, "%a", t);
      for (char *c = label_buffer; *c; c++) {
        if (*c >= 'a' && *c <= 'z') {
          *c -= 'a' - 'A';
        }
      }
      break;
  }

  graphics_context_set_stroke_color(ctx, s_palette.print);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, at, radius);

  const int16_t top = at.y - (VALUE_HEIGHT + 2 + LABEL_HEIGHT) / 2;
  const int16_t width = radius * 2 - 4;
  graphics_context_set_text_color(ctx, value_color);
  graphics_draw_text(ctx, value, s_value_font.font,
                     GRect(at.x - width / 2, top - s_value_font.pad, width,
                           VALUE_HEIGHT + s_value_font.pad * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, s_palette.print);
  graphics_draw_text(ctx, label, s_label_font.font,
                     GRect(at.x - width / 2, top + VALUE_HEIGHT + 2 - s_label_font.pad, width,
                           LABEL_HEIGHT + s_label_font.pad * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// A slim leaf: widest a third of the way out, pointed at both ends.
static void draw_hand(GContext *ctx, GPoint centre, int32_t angle, int16_t length,
                      int16_t width) {
  const int16_t half = width / 2;
  GPoint points[] = {
    { 0, length / 8 },
    { -half, -length / 3 },
    { 0, -length },
    { half, -length / 3 },
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  gpath_rotate_to(path, angle);
  gpath_move_to(path, centre);
  graphics_context_set_fill_color(ctx, s_palette.metal);
  gpath_draw_filled(ctx, path);
  graphics_context_set_stroke_color(ctx, s_palette.edge);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // The unobstructed area shrinks when a Timeline Quick View is showing.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GPoint centre = grect_center_point(&bounds);
  const int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2
    - EDGE_INSET;
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  // Black around a round dial.
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_palette.dial);
  graphics_fill_circle(ctx, centre, radius);

  draw_scale(ctx, centre, radius);
  const int16_t inside_track = radius - SCALE_BAND - TRACK_WIDTH;
  const int16_t numeral_height = radius * 20 / 100;
  draw_numerals(ctx, centre, inside_track - NUMERAL_GAP - numeral_height / 2);
  draw_name(ctx, GPoint(centre.x, centre.y - radius * 38 / 100));
  draw_seconds_dial(ctx, ray_point(centre, TRIG_MAX_ANGLE * 5 / 8, radius * 45 / 100),
                    radius * 26 / 100, t);
  if (s_settings.info != INFO_NONE) {
    draw_info_dial(ctx, ray_point(centre, TRIG_MAX_ANGLE * 3 / 8, radius * 45 / 100),
                   radius * 26 / 100, t);
  }

  const int16_t minute_length = inside_track - 2;
  const int16_t hour_length = minute_length * 62 / 100;
  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;
  draw_hand(ctx, centre, hour_angle, hour_length, HOUR_HAND_WIDTH);
  draw_hand(ctx, centre, minute_angle, minute_length, MINUTE_HAND_WIDTH);

  graphics_context_set_fill_color(ctx, s_palette.metal);
  graphics_fill_circle(ctx, centre, HUB_RADIUS);
  graphics_context_set_stroke_color(ctx, s_palette.edge);
  graphics_draw_circle(ctx, centre, HUB_RADIUS);
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
  if (s_settings.info == INFO_WEATHER && (units_changed & MINUTE_UNIT)
      && tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery = state;
  if (s_settings.info == INFO_BATTERY) {
    layer_mark_dirty(s_canvas);
  }
}

// Running seconds cost battery, so only tick every second while they are shown.
static void subscribe_ticks(void) {
  tick_timer_service_subscribe(s_settings.seconds ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

// Clay sends toggles as numbers and dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void unload_fonts(void) {
  styled_font_unload(&s_numeral_font);
  styled_font_unload(&s_name_font);
  styled_font_unload(&s_value_font);
  styled_font_unload(&s_label_font);
}

// Loads the chosen font styles: one for the hour numbers, one for the name
// and information. The face's usual numbers are its own Breguet-style font.
static void load_fonts(void) {
  unload_fonts();
  if (s_settings.number_font == FONT_STYLE_PEBBLE) {
    s_numeral_font = (StyledFont) {
      fonts_load_custom_font(resource_get_handle(NUMERAL_FONT)), 0, true,
    };
  } else {
    styled_font_load(&s_numeral_font, s_settings.number_font, NULL, 0, FONTS_NUMERAL);
  }
  styled_font_load(&s_name_font, s_settings.info_font, NAME_FONT, NAME_PAD, FONTS_NAME);
  styled_font_load(&s_value_font, s_settings.info_font, VALUE_FONT, VALUE_PAD, FONTS_VALUE);
  styled_font_load(&s_label_font, s_settings.info_font, LABEL_FONT, LABEL_PAD, FONTS_LABEL);
}

static bool read_settings(DictionaryIterator *iterator) {
  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DIAL_COLOR))) {
    const int32_t dial = tuple_int(tuple);
    s_settings.dial = dial >= 0 && dial < DIAL_COUNT ? dial : DIAL_SALMON;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SECONDS))) {
    s_settings.seconds = tuple_int(tuple) != 0;
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
  if ((tuple = dict_find(iterator, MESSAGE_KEY_INFO))) {
    const int32_t info = tuple_int(tuple);
    s_settings.info = info >= 0 && info < INFO_COUNT ? info : INFO_NONE;
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
    window_set_background_color(s_window, GColorBlack);
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

static void load_settings(void) {
  settings_set_defaults(&s_settings);
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings saved;
    const int size = persist_read_data(PERSIST_KEY_SETTINGS, &saved, sizeof(saved));
    if (size == settings_size(saved.version) && saved.dial < DIAL_COUNT) {
      memcpy(&s_settings, &saved, size);
      s_settings.version = SETTINGS_VERSION;
      if (s_settings.info >= INFO_COUNT) {
        s_settings.info = INFO_NONE;
      }
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
  s_scale_font = fonts_get_system_font(SCALE_FONT);
  load_settings();
  load_fonts();
  s_battery = battery_state_service_peek();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  subscribe_ticks();
  battery_state_service_subscribe(battery_handler);
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
  accel_tap_service_subscribe(tap_handler);
#endif
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
  battery_state_service_unsubscribe();
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
