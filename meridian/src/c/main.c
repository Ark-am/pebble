#include <pebble.h>

// An analog watchface with four complications around the dial:
//   12 o'clock  weather (from the phone)
//    3 o'clock  day and date
//    6 o'clock  health metric; tap the watch to cycle through them
//    9 o'clock  battery, plus Bluetooth and Quiet Time alerts

#define PERSIST_KEY_METRIC 1
#define PERSIST_KEY_TEMPERATURE 2
#define PERSIST_KEY_CONDITION 3

#define WEATHER_REFRESH_MINUTES 30
#define LOW_BATTERY_PERCENT 20
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define VALUE_FONT FONT_KEY_GOTHIC_24_BOLD
  #define LABEL_FONT FONT_KEY_GOTHIC_18
  // Visible glyph heights and the blank space fonts leave above them.
  #define VALUE_HEIGHT 17
  #define VALUE_PAD 7
  #define LABEL_HEIGHT 13
  #define LABEL_PAD 5
  #define SLOT_SIDE_WIDTH 64
  #define HOUR_HAND_WIDTH 9
  #define MINUTE_HAND_WIDTH 7
  #define HOUR_TICK_LENGTH 12
  #define MINUTE_TICK_LENGTH 6
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
#endif

#define SLOT_WIDE_WIDTH 90
#define LINE_GAP 2
#define EDGE_INSET 2

#define COLOR_BACKGROUND GColorBlack
#define COLOR_FOREGROUND GColorWhite
#define COLOR_LABEL PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COLOR_MINOR_TICK PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define COLOR_ACCENT PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define COLOR_WARNING PBL_IF_COLOR_ELSE(GColorRed, GColorWhite)
#define COLOR_CHARGING PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite)

static Window *s_window;
static Layer *s_canvas;
static GFont s_value_font;
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
      snprintf(value, size, "%d", (int)(health_service_sum_today(HealthMetricActiveSeconds) / 60));
      *label = "ACTIVE MIN";
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
// Drawing

static void draw_ticks(GContext *ctx, GRect bounds, GPoint centre) {
  for (int i = 0; i < 60; i++) {
    const bool hour = i % 5 == 0;
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    const int32_t outer = edge_distance(bounds, angle);
    const int32_t length = hour ? HOUR_TICK_LENGTH : MINUTE_TICK_LENGTH;
    graphics_context_set_stroke_color(ctx, hour ? COLOR_FOREGROUND : COLOR_MINOR_TICK);
    graphics_context_set_stroke_width(ctx, hour ? 3 : 1);
    graphics_draw_line(ctx, ray_point(centre, angle, outer - length),
                       ray_point(centre, angle, outer));
  }

  // An accent triangle marks 12 o'clock.
  const int32_t top = edge_distance(bounds, 0);
  const int16_t size = HOUR_TICK_LENGTH / 2 + 2;
  const GPoint tip = ray_point(centre, 0, top - HOUR_TICK_LENGTH - 3);
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) {
      { tip.x - size, tip.y - size },
      { tip.x + size, tip.y - size },
      { tip.x, tip.y },
    },
  };
  GPath *marker = gpath_create(&info);
  graphics_context_set_fill_color(ctx, COLOR_ACCENT);
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
  draw_text_line(ctx, value, s_value_font, value_color, centre,
                 top, VALUE_PAD, VALUE_HEIGHT, width);
  if (label) {
    draw_text_line(ctx, label, s_label_font, COLOR_LABEL, centre,
                   top + VALUE_HEIGHT + LINE_GAP, LABEL_PAD, LABEL_HEIGHT, width);
  }
}

static void draw_battery_icon(GContext *ctx, GPoint centre) {
  const GRect body = GRect(centre.x - 9, centre.y - 4, 16, 9);
  const GColor fill = s_battery.is_charging
    ? COLOR_CHARGING
    : s_battery.charge_percent <= LOW_BATTERY_PERCENT ? COLOR_WARNING : COLOR_FOREGROUND;

  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, body);
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
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
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  graphics_fill_circle(ctx, centre, 6);
  graphics_context_set_fill_color(ctx, COLOR_BACKGROUND);
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

static void draw_hand(GContext *ctx, GPoint centre, int32_t angle,
                      int16_t length, int16_t width, int16_t tail) {
  const GPoint tip = ray_point(centre, angle, length);
  const GPoint back = ray_point(centre, angle + TRIG_MAX_ANGLE / 2, tail);
  // A dark border keeps the hands legible where they cross text.
  graphics_context_set_stroke_color(ctx, COLOR_BACKGROUND);
  graphics_context_set_stroke_width(ctx, width + 2);
  graphics_draw_line(ctx, back, tip);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, back, tip);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // The unobstructed area shrinks when a Timeline Quick View is showing.
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GPoint centre = grect_center_point(&bounds);
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, COLOR_BACKGROUND);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  draw_ticks(ctx, bounds, centre);

  const int32_t reach_x = edge_distance(bounds, TRIG_MAX_ANGLE / 4);
  const int32_t reach_y = edge_distance(bounds, 0);
  const int16_t side_offset = reach_x * 52 / 100;
  const int16_t vertical_offset = reach_y * 50 / 100;

  // 12 o'clock: weather.
  char temperature[16];
  if (s_temperature == NO_TEMPERATURE) {
    snprintf(temperature, sizeof(temperature), "--\xc2\xb0");
  } else {
    snprintf(temperature, sizeof(temperature), "%d\xc2\xb0", (int)s_temperature);
  }
  draw_complication(ctx, GPoint(centre.x, centre.y - vertical_offset), SLOT_WIDE_WIDTH,
                    temperature, COLOR_FOREGROUND,
                    s_condition[0] ? s_condition : "WEATHER");

  // 3 o'clock: day and date.
  char day[4];
  char date[4];
  strftime(day, sizeof(day), "%a", t);
  for (char *c = day; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
  strftime(date, sizeof(date), "%d", t);
  draw_complication(ctx, GPoint(centre.x + side_offset, centre.y), SLOT_SIDE_WIDTH,
                    date, COLOR_ACCENT, day);

  // 9 o'clock: battery, with alerts above it.
  const GPoint battery = GPoint(centre.x - side_offset, centre.y);
  char percent[8];
  snprintf(percent, sizeof(percent), "%d%%", s_battery.charge_percent);
  const bool low = !s_battery.is_charging && s_battery.charge_percent <= LOW_BATTERY_PERCENT;
  draw_complication(ctx, battery, SLOT_SIDE_WIDTH, percent,
                    low ? COLOR_WARNING : COLOR_FOREGROUND, NULL);
  draw_battery_icon(ctx, GPoint(battery.x, battery.y + VALUE_HEIGHT / 2 + LINE_GAP + 4));
  draw_status_icons(ctx, GPoint(battery.x, battery.y - VALUE_HEIGHT - 6));

  // 6 o'clock: health, or the month and year on watches without it.
  char value[16];
#if defined(PBL_HEALTH)
  const char *label;
  format_metric(s_metric, value, sizeof(value), &label);
#else
  char label[8];
  strftime(value, sizeof(value), "%b", t);
  strftime(label, sizeof(label), "%Y", t);
#endif
  draw_complication(ctx, GPoint(centre.x, centre.y + vertical_offset), SLOT_WIDE_WIDTH,
                    value, COLOR_FOREGROUND, label);

  // Hands on top of everything.
  const int16_t minute_length = (reach_x < reach_y ? reach_x : reach_y) - HOUR_TICK_LENGTH - 4;
  const int16_t hour_length = minute_length * 62 / 100;
  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;
  draw_hand(ctx, centre, hour_angle, hour_length, HOUR_HAND_WIDTH, 6);
  draw_hand(ctx, centre, minute_angle, minute_length, MINUTE_HAND_WIDTH, 8);

  graphics_context_set_fill_color(ctx, COLOR_ACCENT);
  graphics_fill_circle(ctx, centre, HOUR_HAND_WIDTH / 2 + 2);
  graphics_context_set_fill_color(ctx, COLOR_BACKGROUND);
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
  if (tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

static void battery_handler(BatteryChargeState state) {
  s_battery = state;
  layer_mark_dirty(s_canvas);
}

static void bluetooth_handler(bool connected) {
  if (s_bluetooth_connected && !connected && !quiet_time_is_active()) {
    vibes_double_pulse();
  }
  s_bluetooth_connected = connected;
  layer_mark_dirty(s_canvas);
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
  s_value_font = fonts_get_system_font(VALUE_FONT);
  s_label_font = fonts_get_system_font(LABEL_FONT);
  load_saved_state();
  s_battery = battery_state_service_peek();
  s_bluetooth_connected = connection_service_peek_pebble_app_connection();

  s_window = window_create();
  window_set_background_color(s_window, COLOR_BACKGROUND);
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

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 32);
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
