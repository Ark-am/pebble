#include <pebble.h>

// A classic dress-watch face: a round dial with a minute scale and railroad
// track around the edge, Breguet-style hour numbers, slim leaf hands and a small
// seconds dial at half past seven. The dial colour, the running seconds and the
// name on the dial are configurable from the phone (see src/pkjs).

#define PERSIST_KEY_SETTINGS 1

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
#endif

#define EDGE_INSET 1
#define NUMERAL_GAP 3

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 1
typedef struct {
  uint8_t version;
  uint8_t dial;
  bool seconds;
  char dial_name[16];
} Settings;

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

typedef struct {
  GColor dial;
  GColor print;   // The minute scale, track and name.
  GColor metal;   // Hour numbers and hands.
  GColor edge;    // The shadow and outline that set the metal off the dial.
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static GFont s_numeral_font;
static GFont s_name_font;
static GFont s_scale_font;

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .dial = DIAL_SALMON,
    .seconds = false,
    .dial_name = "BALTIC",
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
// and 8 give way to the seconds dial.
static void draw_numerals(GContext *ctx, GPoint centre, int16_t distance) {
  const GSize size = graphics_text_layout_get_content_size(
    "12", s_numeral_font, GRect(0, 0, 80, 60), GTextOverflowModeFill, GTextAlignmentCenter);
  for (int hour = 1; hour <= 12; hour++) {
    if (hour == 7 || hour == 8) {
      continue;
    }
    char text[3];
    snprintf(text, sizeof(text), "%d", hour);
    const GPoint at = ray_point(centre, TRIG_MAX_ANGLE * hour / 12, distance);
    // Custom fonts leave space above the digits for taller characters, so the
    // box is lifted by a quarter of its height to centre the digits.
    const GRect box = GRect(at.x - 30, at.y - size.h / 2 - size.h / 4, 60, size.h + 4);
    graphics_context_set_text_color(ctx, s_palette.edge);
    graphics_draw_text(ctx, text, s_numeral_font, GRect(box.origin.x + 1, box.origin.y + 1,
                                                        box.size.w, box.size.h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, s_palette.metal);
    graphics_draw_text(ctx, text, s_numeral_font, box, GTextOverflowModeFill,
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
      letter, s_name_font, GRect(0, 0, 40, 30), GTextOverflowModeFill,
      GTextAlignmentLeft).w;
    total += widths[i] + (i ? NAME_SPACING : 0);
  }
  int16_t x = at.x - total / 2;
  graphics_context_set_text_color(ctx, s_palette.print);
  for (int i = 0; i < length; i++) {
    const char letter[2] = { name[i], '\0' };
    graphics_draw_text(ctx, letter, s_name_font,
                       GRect(x - 2, at.y - NAME_HEIGHT / 2 - NAME_PAD, widths[i] + 4,
                             NAME_HEIGHT + NAME_PAD * 2),
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
  graphics_context_set_fill_color(ctx, s_palette.dial);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  draw_scale(ctx, centre, radius);
  const int16_t inside_track = radius - SCALE_BAND - TRACK_WIDTH;
  const int16_t numeral_height = radius * 20 / 100;
  draw_numerals(ctx, centre, inside_track - NUMERAL_GAP - numeral_height / 2);
  draw_name(ctx, GPoint(centre.x, centre.y - radius * 38 / 100));
  draw_seconds_dial(ctx, ray_point(centre, TRIG_MAX_ANGLE * 5 / 8, radius * 45 / 100),
                    radius * 26 / 100, t);

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

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
}

// Running seconds cost battery, so only tick every second while they are shown.
static void subscribe_ticks(void) {
  tick_timer_service_subscribe(s_settings.seconds ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

// Clay sends toggles as numbers and dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
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
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DIAL_NAME)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_settings.dial_name, tuple->value->cstring, sizeof(s_settings.dial_name) - 1);
    s_settings.dial_name[sizeof(s_settings.dial_name) - 1] = '\0';
    changed = true;
  }
  return changed;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  if (read_settings(iterator)) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    window_set_background_color(s_window, s_palette.dial);
    subscribe_ticks();
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
    if (size == (int)sizeof(saved) && saved.version == SETTINGS_VERSION
        && saved.dial < DIAL_COUNT) {
      s_settings = saved;
    }
  }
  apply_palette();
}

static void init(void) {
  s_numeral_font = fonts_load_custom_font(resource_get_handle(NUMERAL_FONT));
  s_name_font = fonts_get_system_font(NAME_FONT);
  s_scale_font = fonts_get_system_font(SCALE_FONT);
  load_settings();

  s_window = window_create();
  window_set_background_color(s_window, s_palette.dial);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  subscribe_ticks();
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 0);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  fonts_unload_custom_font(s_numeral_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
