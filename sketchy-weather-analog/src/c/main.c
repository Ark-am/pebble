#include <pebble.h>

// An analog watchface drawn in a hand-sketched style. A pencil-drawn widget
// sits in the centre: a picture above one or two values, such as the weather
// icon above the date and temperature. The widget has up to four screens,
// each pairing two kinds of information; they change every minute if chosen,
// and a tap on the watch moves to the next one. The hour and minute hands
// start outside a sketched ring around the widget, so they never cover it.
// Settings come from the phone (see src/pkjs).

#define PERSIST_KEY_TEMPERATURE 1
#define PERSIST_KEY_CONDITION 2
#define PERSIST_KEY_DAYTIME 3
#define PERSIST_KEY_SETTINGS 4

#define WEATHER_REFRESH_MINUTES 30
#define LOW_BATTERY_PERCENT 20
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define TEXT_FONT FONT_KEY_GOTHIC_18
  #define NUMERAL_FONT FONT_KEY_GOTHIC_24
  #define NUMERAL_HEIGHT 17
  #define NUMERAL_PAD 7
  #define NUMERAL_RADIUS 12
  #define DAY_FONT FONT_KEY_GOTHIC_18_BOLD
  #define DAY_HEIGHT 13
  #define DAY_PAD 5
  // Visible height of the text and the blank space the font leaves above it.
  #define TEXT_HEIGHT 13
  #define TEXT_PAD 5
  #define ICON_SIZE 76
  // How far the icon's centre sits above the widget's centre, and the
  // underline below it.
  #define ICON_RISE 15
  #define RULE_DROP 14
  // Radius of the sketched ring the hands start outside of.
  #define CLEAR_RADIUS 52
  #define HOUR_TICK_LENGTH 9
  #define MINUTE_TICK_LENGTH 4
  #define HOUR_HAND_WIDTH 6
  #define MINUTE_HAND_WIDTH 4
  #define HATCH_SPACING 4
#else
  #define TEXT_FONT FONT_KEY_GOTHIC_14
  #define NUMERAL_FONT FONT_KEY_GOTHIC_18
  #define NUMERAL_HEIGHT 13
  #define NUMERAL_PAD 5
  #define NUMERAL_RADIUS 9
  #define DAY_FONT FONT_KEY_GOTHIC_14_BOLD
  #define DAY_HEIGHT 10
  #define DAY_PAD 4
  #define TEXT_HEIGHT 10
  #define TEXT_PAD 4
  #define ICON_SIZE 54
  #define ICON_RISE 11
  #define RULE_DROP 10
  #define CLEAR_RADIUS 40
  #define HOUR_TICK_LENGTH 6
  #define MINUTE_TICK_LENGTH 3
  #define HOUR_HAND_WIDTH 5
  #define MINUTE_HAND_WIDTH 3
  #define HATCH_SPACING 3
#endif

#define SCREEN_COUNT 4

#define EDGE_INSET 2
#define HAND_GAP 3
#define TEXT_GAP 4

// What the centre widget can show. Values match the options in src/pkjs/config.js.
typedef enum {
  ITEM_NONE,
  ITEM_WEATHER,
  ITEM_DATE,
  ITEM_BATTERY,
  ITEM_STEPS,
  ITEM_HEART_RATE,
  ITEM_COUNT,
} Item;

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 3
typedef struct {
  uint8_t version;
  bool dark_theme;
  bool always_show_battery;
  bool disconnect_vibe;
  char temperature_unit;
  // Added in version 2.
  bool hour_numbers;
  // Added in version 3: the main and second item of each widget screen, and
  // whether the screens change every minute.
  uint8_t screens[SCREEN_COUNT][2];
  bool rotate_screens;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 1: return offsetof(Settings, hour_numbers);
    case 2: return offsetof(Settings, screens);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

typedef struct {
  GColor background;
  GColor ink;
  GColor faint;
  GColor hatch;
  GColor sun;
  GColor moon;
  GColor cloud;
  GColor back_cloud;
  GColor rain;
  GColor bolt;
  GColor warning;
  GColor charging;
  GColor heart;
  GColor calendar;
} Palette;

static Palette s_palette;

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
  WEATHER_COUNT,
} Weather;

static Window *s_window;
static Layer *s_canvas;
static GFont s_text_font;
static GFont s_numeral_font;
static GFont s_day_font;

static BatteryChargeState s_battery;
static bool s_bluetooth_connected;
static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[16];
static bool s_daytime = true;
// The widget screen being shown.
static int s_screen;

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .dark_theme = false,
    .always_show_battery = false,
    .disconnect_vibe = true,
    .temperature_unit = 'C',
    .hour_numbers = false,
    .screens = {
      { ITEM_WEATHER, ITEM_DATE },
      { ITEM_DATE, ITEM_BATTERY },
      { ITEM_STEPS, ITEM_HEART_RATE },
      { ITEM_NONE, ITEM_NONE },
    },
    .rotate_screens = false,
  };
}

static void apply_palette(void) {
  const bool dark = s_settings.dark_theme;
  s_palette.background = dark ? GColorBlack : GColorWhite;
  s_palette.ink = dark ? GColorWhite : GColorBlack;
#if defined(PBL_COLOR)
  s_palette.faint = dark ? GColorDarkGray : GColorLightGray;
  s_palette.hatch = GColorBlack;
  s_palette.sun = GColorChromeYellow;
  s_palette.moon = GColorIcterine;
  s_palette.cloud = GColorPictonBlue;
  s_palette.back_cloud = GColorLightGray;
  s_palette.rain = dark ? GColorPictonBlue : GColorBlueMoon;
  s_palette.bolt = GColorYellow;
  s_palette.warning = GColorRed;
  s_palette.charging = GColorGreen;
  s_palette.heart = GColorRed;
  s_palette.calendar = GColorRed;
#else
  // Black and white: shapes are left unfilled and the pencil hatching gives
  // the sun and moon their tone.
  s_palette.faint = s_palette.ink;
  s_palette.hatch = s_palette.ink;
  s_palette.sun = s_palette.background;
  s_palette.moon = s_palette.background;
  s_palette.cloud = s_palette.background;
  s_palette.back_cloud = s_palette.background;
  s_palette.rain = s_palette.ink;
  s_palette.bolt = s_palette.background;
  s_palette.warning = s_palette.ink;
  s_palette.charging = s_palette.ink;
  s_palette.heart = s_palette.ink;
  s_palette.calendar = s_palette.ink;
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
// use a circle; rectangular ones follow the display's edge.
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

static int32_t distance_squared(GPoint a, GPoint b) {
  const int32_t dx = a.x - b.x;
  const int32_t dy = a.y - b.y;
  return dx * dx + dy * dy;
}

// ---------------------------------------------------------------------------
// Pencil strokes
//
// Every line is drawn twice, the second time a pixel or so off, the way a
// pencil sketch doubles back over itself. The offsets come from a hash of the
// coordinates, so the same drawing looks the same every time it is redrawn.

static int jitter(int32_t seed) {
  seed = seed * 1103515245 + 12345;
  return (int)((uint32_t)seed >> 16) % 3 - 1;
}

static void sketch_line(GContext *ctx, GPoint a, GPoint b, GColor color) {
  const int32_t seed = a.x * 73 + a.y * 151 + b.x * 37 + b.y * 211;
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, a, b);
  graphics_draw_line(ctx, GPoint(a.x + jitter(seed), a.y + jitter(seed + 1)),
                     GPoint(b.x + jitter(seed + 2), b.y + jitter(seed + 3)));
}

typedef bool (*PointFilter)(GPoint point, const void *data);

#define ARC_STEPS 32

// Outlines a circle as a ring of short strokes, keeping only the strokes whose
// ends both pass the filter (all of them when there is none). This traces the
// visible edge of overlapping shapes such as clouds and crescents.
static void sketch_circle(GContext *ctx, GPoint centre, int16_t radius, GColor color,
                          PointFilter visible, const void *data) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  for (int pass = 0; pass < 2; pass++) {
    GPoint previous = GPointZero;
    bool previous_shown = false;
    for (int i = 0; i <= ARC_STEPS; i++) {
      // The second pass wobbles in and out a little.
      const int16_t r = pass ? radius + jitter(centre.x * 7 + centre.y * 13 + i) : radius;
      const int32_t angle = TRIG_MAX_ANGLE * (i % ARC_STEPS) / ARC_STEPS
        + pass * TRIG_MAX_ANGLE / (ARC_STEPS * 3);
      const GPoint point = ray_point(centre, angle, r);
      const bool shown = !visible || visible(point, data);
      if (i > 0 && shown && previous_shown) {
        graphics_draw_line(ctx, previous, point);
      }
      previous = point;
      previous_shown = shown;
    }
  }
}

// Diagonal pencil shading across a disc.
static void hatch_circle(GContext *ctx, GPoint centre, int16_t radius) {
  graphics_context_set_stroke_color(ctx, s_palette.hatch);
  graphics_context_set_stroke_width(ctx, 1);
  const int16_t inner = radius - 1;
  for (int16_t d = -inner + HATCH_SPACING / 2; d < inner; d += HATCH_SPACING) {
    // 181 / 256 is close to 1 / sqrt(2), for 45-degree lines.
    const int16_t half = isqrt(inner * inner - d * d) * 181 / 256;
    const int16_t mid_x = centre.x + d * 181 / 256;
    const int16_t mid_y = centre.y + d * 181 / 256;
    graphics_draw_line(ctx, GPoint(mid_x - half, mid_y + half), GPoint(mid_x + half, mid_y - half));
  }
}

// ---------------------------------------------------------------------------
// Weather icons
//
// Icons are designed in a 100 x 100 unit box and scaled to ICON_SIZE pixels.
// Every icon keeps between y = 14 and y = 86 so it clears the rule below it.

static Weather weather_from_condition(const char *condition) {
  static const char *const names[WEATHER_COUNT] = {
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
  for (int i = 1; i < WEATHER_COUNT; i++) {
    if (strcmp(condition, names[i]) == 0) {
      return i;
    }
  }
  return WEATHER_UNKNOWN;
}

static GPoint s_icon_centre;

static GPoint icon_point(int16_t x, int16_t y) {
  return GPoint(s_icon_centre.x + (x - 50) * ICON_SIZE / 100,
                s_icon_centre.y + (y - 50) * ICON_SIZE / 100);
}

static int16_t icon_length(int16_t units) {
  const int16_t length = units * ICON_SIZE / 100;
  return length > 1 ? length : 1;
}

// Small irregularities in the sun's rays, in degrees, so they look hand-drawn.
static const int8_t s_ray_wobble[8] = { 0, 4, -3, 2, -4, 3, -2, 4 };

static void draw_sun(GContext *ctx, int16_t x, int16_t y, int16_t radius, int16_t ray) {
  const GPoint centre = icon_point(x, y);
  const int16_t r = icon_length(radius);
  graphics_context_set_fill_color(ctx, s_palette.sun);
  graphics_fill_circle(ctx, centre, r);
  hatch_circle(ctx, centre, r);
  sketch_circle(ctx, centre, r, s_palette.ink, NULL, NULL);

  const int16_t gap = icon_length(radius / 3 + 1);
  for (int i = 0; i < 8; i++) {
    const int32_t angle = TRIG_MAX_ANGLE * i / 8 + TRIG_MAX_ANGLE * s_ray_wobble[i] / 360;
    // Alternate rays are a little shorter.
    const int16_t length = icon_length(i % 2 ? ray * 3 / 4 : ray);
    sketch_line(ctx, ray_point(centre, angle, r + gap),
                ray_point(centre, angle, r + gap + length), s_palette.ink);
  }
}

typedef struct {
  GPoint centre;
  int16_t radius;
  bool inside;
} CircleFilter;

// Whether a point is inside (or outside) a circle, with a pixel of slack so
// the outline meets cleanly.
static bool circle_filter(GPoint point, const void *data) {
  const CircleFilter *filter = data;
  const int32_t d2 = distance_squared(point, filter->centre);
  return filter->inside
    ? d2 <= (filter->radius + 1) * (filter->radius + 1)
    : d2 >= (filter->radius - 1) * (filter->radius - 1);
}

// A crescent moon: a shaded disc with a bite taken out of its upper right.
static void draw_moon(GContext *ctx, int16_t x, int16_t y, int16_t radius) {
  const GPoint centre = icon_point(x, y);
  const int16_t r = icon_length(radius);
  const GPoint bite = icon_point(x + radius * 45 / 100, y - radius * 35 / 100);
  const int16_t bite_r = icon_length(radius * 82 / 100);

  graphics_context_set_fill_color(ctx, s_palette.moon);
  graphics_fill_circle(ctx, centre, r);
  hatch_circle(ctx, centre, r);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, bite, bite_r);

  const CircleFilter outside_bite = { bite, bite_r, false };
  const CircleFilter inside_moon = { centre, r, true };
  sketch_circle(ctx, centre, r, s_palette.ink, circle_filter, &outside_bite);
  sketch_circle(ctx, bite, bite_r, s_palette.ink, circle_filter, &inside_moon);
}

// A cloud is three overlapping puffs on a flat base.
typedef struct {
  GPoint centres[3];
  int16_t radii[3];
  int16_t left, right, top, bottom;
} Cloud;

typedef struct {
  const Cloud *cloud;
  int skip;
} CloudFilter;

// A point is on the cloud's outline when it is not inside any other puff or
// the base.
static bool cloud_filter(GPoint point, const void *data) {
  const CloudFilter *filter = data;
  const Cloud *cloud = filter->cloud;
  for (int i = 0; i < 3; i++) {
    const int16_t r = cloud->radii[i] - 1;
    if (i != filter->skip && distance_squared(point, cloud->centres[i]) < r * r) {
      return false;
    }
  }
  return !(point.x > cloud->left + 1 && point.x < cloud->right - 1
           && point.y > cloud->top && point.y < cloud->bottom - 1);
}

// Draws a cloud moved by (dx, dy) units and scaled to a percentage of full size.
static void draw_cloud(GContext *ctx, int16_t dx, int16_t dy, int16_t scale, GColor fill) {
  static const int8_t puffs[3][3] = { { 30, 60, 13 }, { 50, 48, 18 }, { 70, 58, 15 } };
  static const int16_t base = 73;
  Cloud cloud;
  for (int i = 0; i < 3; i++) {
    cloud.centres[i] = icon_point(50 + (puffs[i][0] - 50) * scale / 100 + dx,
                                  50 + (puffs[i][1] - 50) * scale / 100 + dy);
    cloud.radii[i] = icon_length(puffs[i][2] * scale / 100);
  }
  cloud.left = cloud.centres[0].x;
  cloud.right = cloud.centres[2].x;
  cloud.top = cloud.centres[1].y;
  cloud.bottom = icon_point(0, 50 + (base - 50) * scale / 100 + dy).y;

  graphics_context_set_fill_color(ctx, fill);
  for (int i = 0; i < 3; i++) {
    graphics_fill_circle(ctx, cloud.centres[i], cloud.radii[i]);
  }
  graphics_fill_rect(ctx, GRect(cloud.left, cloud.top, cloud.right - cloud.left + 1,
                                cloud.bottom - cloud.top + 1), 0, GCornerNone);

  for (int i = 0; i < 3; i++) {
    const CloudFilter filter = { &cloud, i };
    sketch_circle(ctx, cloud.centres[i], cloud.radii[i], s_palette.ink, cloud_filter, &filter);
  }
  sketch_line(ctx, GPoint(cloud.left, cloud.bottom), GPoint(cloud.right, cloud.bottom),
              s_palette.ink);
}

// Precipitation clouds sit higher in the box to leave room underneath.
#define RAISED_CLOUD -14

static void draw_rain(GContext *ctx, bool heavy) {
  static const int8_t drops[][2] = { { 34, 62 }, { 52, 64 }, { 70, 62 }, { 43, 74 }, { 61, 74 } };
  graphics_context_set_stroke_color(ctx, s_palette.rain);
  graphics_context_set_stroke_width(ctx, ICON_SIZE >= 60 ? 2 : 1);
  const int count = heavy ? 5 : 3;
  for (int i = 0; i < count; i++) {
    const int16_t length = heavy ? 10 : 14;
    graphics_draw_line(ctx, icon_point(drops[i][0], drops[i][1]),
                       icon_point(drops[i][0] - 4, drops[i][1] + length));
  }
}

static void draw_drizzle(GContext *ctx) {
  static const int8_t drops[][2] = {
    { 32, 64 }, { 50, 66 }, { 68, 64 }, { 41, 78 }, { 59, 78 },
  };
  graphics_context_set_fill_color(ctx, s_palette.rain);
  for (size_t i = 0; i < ARRAY_LENGTH(drops); i++) {
    graphics_fill_circle(ctx, icon_point(drops[i][0], drops[i][1]), icon_length(3));
  }
}

static void draw_snow(GContext *ctx) {
  static const int8_t flakes[][2] = { { 32, 68 }, { 68, 68 }, { 50, 79 } };
  const int16_t r = icon_length(7);
  for (size_t i = 0; i < ARRAY_LENGTH(flakes); i++) {
    const GPoint centre = icon_point(flakes[i][0], flakes[i][1]);
    for (int arm = 0; arm < 3; arm++) {
      const int32_t angle = TRIG_MAX_ANGLE * arm / 6;
      sketch_line(ctx, ray_point(centre, angle, r),
                  ray_point(centre, angle + TRIG_MAX_ANGLE / 2, r), s_palette.ink);
    }
  }
}

static void draw_bolt(GContext *ctx) {
  GPoint points[] = {
    icon_point(54, 54), icon_point(43, 71), icon_point(51, 71), icon_point(46, 88),
    icon_point(63, 66), icon_point(55, 66), icon_point(61, 54),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *bolt = gpath_create(&info);
  graphics_context_set_fill_color(ctx, s_palette.bolt);
  gpath_draw_filled(ctx, bolt);
  gpath_destroy(bolt);
  for (size_t i = 0; i < ARRAY_LENGTH(points); i++) {
    sketch_line(ctx, points[i], points[(i + 1) % ARRAY_LENGTH(points)], s_palette.ink);
  }
}

static void draw_fog(GContext *ctx) {
  static const int8_t lines[][3] = { { 22, 64, 70 }, { 32, 74, 80 }, { 24, 84, 64 } };
  for (size_t i = 0; i < ARRAY_LENGTH(lines); i++) {
    sketch_line(ctx, icon_point(lines[i][0], lines[i][1]),
                icon_point(lines[i][2], lines[i][1]), s_palette.ink);
  }
}

// A small sun by day or moon by night, peeping out from behind a cloud.
static void draw_small_sky(GContext *ctx, int16_t x, int16_t y) {
  if (s_daytime) {
    draw_sun(ctx, x, y, 12, 8);
  } else {
    draw_moon(ctx, x, y, 16);
  }
}

static void draw_weather_icon(GContext *ctx, Weather weather) {
  switch (weather) {
    case WEATHER_CLEAR:
      if (s_daytime) {
        draw_sun(ctx, 50, 50, 17, 12);
      } else {
        draw_moon(ctx, 50, 50, 26);
      }
      break;
    case WEATHER_FAIR:
      draw_small_sky(ctx, 64, 34);
      draw_cloud(ctx, -8, 8, 85, s_palette.cloud);
      break;
    case WEATHER_CLOUDY:
      draw_cloud(ctx, 10, -10, 75, s_palette.back_cloud);
      draw_cloud(ctx, -6, 6, 90, s_palette.cloud);
      break;
    case WEATHER_FOG:
      draw_cloud(ctx, 0, RAISED_CLOUD, 90, s_palette.back_cloud);
      draw_fog(ctx);
      break;
    case WEATHER_DRIZZLE:
      draw_cloud(ctx, 0, RAISED_CLOUD, 90, s_palette.cloud);
      draw_drizzle(ctx);
      break;
    case WEATHER_RAIN:
      draw_cloud(ctx, 0, RAISED_CLOUD, 90, s_palette.cloud);
      draw_rain(ctx, true);
      break;
    case WEATHER_SHOWERS:
      draw_small_sky(ctx, 68, 22);
      draw_cloud(ctx, -4, RAISED_CLOUD, 85, s_palette.cloud);
      draw_rain(ctx, false);
      break;
    case WEATHER_SNOW:
      draw_cloud(ctx, 0, RAISED_CLOUD, 90, s_palette.cloud);
      draw_snow(ctx);
      break;
    case WEATHER_STORM:
      draw_cloud(ctx, 0, RAISED_CLOUD, 90, s_palette.back_cloud);
      draw_bolt(ctx);
      break;
    default:
      draw_cloud(ctx, 0, 0, 90, s_palette.background);
      break;
  }
}

// ---------------------------------------------------------------------------
// Dial

static void draw_ticks(GContext *ctx, GRect bounds, GPoint centre) {
  for (int i = 0; i < 60; i++) {
    const bool hour = i % 5 == 0;
    const int32_t angle = TRIG_MAX_ANGLE * i / 60;
    const int32_t outer = edge_distance(bounds, angle);
    if (hour) {
      const GPoint a = ray_point(centre, angle, outer - HOUR_TICK_LENGTH);
      const GPoint b = ray_point(centre, angle, outer);
      graphics_context_set_stroke_color(ctx, s_palette.ink);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_line(ctx, a, b);
      sketch_line(ctx, a, b, s_palette.ink);
    } else {
      graphics_context_set_stroke_color(ctx, s_palette.faint);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, ray_point(centre, angle, outer - MINUTE_TICK_LENGTH),
                         ray_point(centre, angle, outer));
    }
  }
}

// Hour numbers sit just inside the hour ticks.
static void draw_hour_numbers(GContext *ctx, GRect bounds, GPoint centre) {
  graphics_context_set_text_color(ctx, s_palette.ink);
  for (int hour = 0; hour < 12; hour++) {
    const int32_t angle = TRIG_MAX_ANGLE * hour / 12;
    const int32_t distance = edge_distance(bounds, angle)
      - HOUR_TICK_LENGTH - 3 - NUMERAL_RADIUS;
    const GPoint at = ray_point(centre, angle, distance);
    char text[3];
    snprintf(text, sizeof(text), "%d", hour == 0 ? 12 : hour);
    graphics_draw_text(ctx, text, s_numeral_font,
                       GRect(at.x - NUMERAL_RADIUS * 2, at.y - NUMERAL_HEIGHT / 2 - NUMERAL_PAD,
                             NUMERAL_RADIUS * 4, NUMERAL_HEIGHT + NUMERAL_PAD * 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

// A hand drawn as a heavy pencil stroke from just outside the ring, with a
// lighter stroke beside it.
static void draw_hand(GContext *ctx, GPoint centre, int32_t angle, int16_t from,
                      int16_t to, int16_t width) {
  const GPoint start = ray_point(centre, angle, from);
  const GPoint tip = ray_point(centre, angle, to);
  graphics_context_set_stroke_color(ctx, s_palette.ink);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, start, tip);

  const int32_t side = angle + TRIG_MAX_ANGLE / 4;
  const int16_t offset = width / 2 + 1;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, ray_point(ray_point(centre, angle, from + 1), side, offset),
                     ray_point(ray_point(centre, angle, to - 2), side, offset - 1));
}

// ---------------------------------------------------------------------------
// Other widget pictures, on the same 100 x 100 unit grid as the weather icons

static void sketch_polygon(GContext *ctx, const GPoint *points, size_t count, GColor color) {
  for (size_t i = 0; i < count; i++) {
    sketch_line(ctx, points[i], points[(i + 1) % count], color);
  }
}

static void draw_text_centred(GContext *ctx, const char *text, GFont font, GPoint centre,
                              int16_t width, int16_t height, int16_t pad) {
  graphics_draw_text(ctx, text, font,
                     GRect(centre.x - width / 2, centre.y - height / 2 - pad, width,
                           height + pad * 2),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// A tear-off calendar page with the day of the week on it.
static void draw_calendar(GContext *ctx, const struct tm *t) {
  const GPoint top_left = icon_point(22, 20);
  const GPoint bottom_right = icon_point(78, 84);
  const int16_t band = icon_point(0, 34).y;
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, GRect(top_left.x, top_left.y, bottom_right.x - top_left.x,
                                bottom_right.y - top_left.y), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_palette.calendar);
  graphics_fill_rect(ctx, GRect(top_left.x, top_left.y, bottom_right.x - top_left.x,
                                band - top_left.y), 0, GCornerNone);
  const GPoint page[] = {
    top_left, GPoint(bottom_right.x, top_left.y), bottom_right, GPoint(top_left.x, bottom_right.y),
  };
  sketch_polygon(ctx, page, ARRAY_LENGTH(page), s_palette.ink);
  sketch_line(ctx, GPoint(top_left.x, band), GPoint(bottom_right.x, band), s_palette.ink);
  // Binder rings.
  sketch_line(ctx, icon_point(36, 13), icon_point(36, 26), s_palette.ink);
  sketch_line(ctx, icon_point(64, 13), icon_point(64, 26), s_palette.ink);

  char day[4];
  strftime(day, sizeof(day), "%a", t);
  for (char *c = day; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
  graphics_context_set_text_color(ctx, s_palette.ink);
  draw_text_centred(ctx, day, s_day_font, icon_point(50, 60), bottom_right.x - top_left.x,
                    DAY_HEIGHT, DAY_PAD);
}

// A large battery, filled to the charge level.
static void draw_battery_picture(GContext *ctx) {
  const GPoint top_left = icon_point(16, 32);
  const GPoint bottom_right = icon_point(78, 68);
  const bool low = s_battery.charge_percent <= LOW_BATTERY_PERCENT;
  const GColor level_color = s_battery.is_charging ? s_palette.charging
    : low ? s_palette.warning : s_palette.ink;

  const int16_t inner_left = top_left.x + 3;
  const int16_t inner_w = bottom_right.x - 3 - inner_left;
  const int16_t level = inner_w * s_battery.charge_percent / 100;
  graphics_context_set_fill_color(ctx, level_color);
  graphics_fill_rect(ctx, GRect(inner_left, top_left.y + 3, level > 0 ? level : 1,
                                bottom_right.y - top_left.y - 5), 0, GCornerNone);

  const GPoint body[] = {
    top_left, GPoint(bottom_right.x, top_left.y), bottom_right, GPoint(top_left.x, bottom_right.y),
  };
  sketch_polygon(ctx, body, ARRAY_LENGTH(body), s_palette.ink);
  const GPoint nub_top = icon_point(78, 42);
  const GPoint nub_bottom = icon_point(85, 58);
  graphics_context_set_fill_color(ctx, s_palette.ink);
  graphics_fill_rect(ctx, GRect(nub_top.x, nub_top.y, nub_bottom.x - nub_top.x,
                                nub_bottom.y - nub_top.y), 0, GCornerNone);
}

// One footprint: a sole, a heel and three toes.
static void draw_foot(GContext *ctx, int16_t x, int16_t y) {
  static const int8_t toes[][2] = { { -8, -16 }, { -1, -19 }, { 6, -17 } };
  graphics_context_set_fill_color(ctx, s_palette.ink);
  graphics_fill_circle(ctx, icon_point(x, y), icon_length(10));
  graphics_fill_circle(ctx, icon_point(x + 2, y + 17), icon_length(7));
  for (size_t i = 0; i < ARRAY_LENGTH(toes); i++) {
    graphics_fill_circle(ctx, icon_point(x + toes[i][0], y + toes[i][1]), icon_length(4));
  }
  sketch_circle(ctx, icon_point(x, y), icon_length(10), s_palette.ink, NULL, NULL);
}

static void draw_footprints(GContext *ctx) {
  draw_foot(ctx, 34, 62);
  draw_foot(ctx, 64, 38);
}

typedef struct {
  GPoint other;
  int16_t other_radius;
  int16_t below;
} HeartFilter;

// The visible part of each lobe: outside the other lobe and above the point
// where the sides run down to the tip.
static bool heart_filter(GPoint point, const void *data) {
  const HeartFilter *filter = data;
  const int16_t r = filter->other_radius - 1;
  return point.y <= filter->below && distance_squared(point, filter->other) >= r * r;
}

static void draw_heart(GContext *ctx) {
  const GPoint left = icon_point(37, 38);
  const GPoint right = icon_point(63, 38);
  const int16_t r = icon_length(14);
  GPoint sides[] = { icon_point(24, 45), icon_point(50, 80), icon_point(76, 45) };

  graphics_context_set_fill_color(ctx, s_palette.heart);
  graphics_fill_circle(ctx, left, r);
  graphics_fill_circle(ctx, right, r);
  GPathInfo info = { .num_points = ARRAY_LENGTH(sides), .points = sides };
  GPath *point = gpath_create(&info);
  gpath_draw_filled(ctx, point);
  gpath_destroy(point);

  const int16_t below = sides[0].y;
  const HeartFilter left_filter = { right, r, below };
  const HeartFilter right_filter = { left, r, below };
  sketch_circle(ctx, left, r, s_palette.ink, heart_filter, &left_filter);
  sketch_circle(ctx, right, r, s_palette.ink, heart_filter, &right_filter);
  sketch_line(ctx, sides[0], sides[1], s_palette.ink);
  sketch_line(ctx, sides[1], sides[2], s_palette.ink);
}

static void draw_item_picture(GContext *ctx, Item item, GPoint centre, const struct tm *t) {
  s_icon_centre = centre;
  switch (item) {
    case ITEM_WEATHER: draw_weather_icon(ctx, weather_from_condition(s_condition)); break;
    case ITEM_DATE: draw_calendar(ctx, t); break;
    case ITEM_BATTERY: draw_battery_picture(ctx); break;
    case ITEM_STEPS: draw_footprints(ctx); break;
    case ITEM_HEART_RATE: draw_heart(ctx); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// Widget values

static int32_t steps_today(void) {
#if defined(PBL_HEALTH)
  const time_t start = time_start_of_today();
  if (health_service_metric_accessible(HealthMetricStepCount, start, time(NULL))
      & HealthServiceAccessibilityMaskAvailable) {
    return health_service_sum_today(HealthMetricStepCount);
  }
#endif
  return -1;
}

static int32_t heart_rate(void) {
#if defined(PBL_HEALTH)
  const time_t now = time(NULL);
  if (health_service_metric_accessible(HealthMetricHeartRateBPM, now, now)
      & HealthServiceAccessibilityMaskAvailable) {
    const int32_t bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
    return bpm > 0 ? bpm : -1;
  }
#endif
  return -1;
}

// Writes an item's value as text. The short form drops units and shortens
// large numbers, for when the full form will not fit inside the ring.
static void format_item(Item item, bool brief, const struct tm *t, char *text, size_t size) {
  switch (item) {
    case ITEM_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        snprintf(text, size, "--\xc2\xb0");
      } else if (brief) {
        snprintf(text, size, "%d\xc2\xb0", (int)s_temperature);
      } else {
        snprintf(text, size, "%d\xc2\xb0%c", (int)s_temperature, s_settings.temperature_unit);
      }
      break;
    case ITEM_DATE: {
      char month[4];
      strftime(month, sizeof(month), "%b", t);
      snprintf(text, size, brief ? "%s %d" : "%s %02d", month, t->tm_mday);
      break;
    }
    case ITEM_BATTERY:
      snprintf(text, size, "%d%%", s_battery.charge_percent);
      break;
    case ITEM_STEPS: {
      const int32_t steps = steps_today();
      if (steps < 0) {
        snprintf(text, size, "--");
      } else if (steps < 1000) {
        snprintf(text, size, "%d", (int)steps);
      } else if (!brief) {
        snprintf(text, size, "%d,%03d", (int)(steps / 1000), (int)(steps % 1000));
      } else if (steps < 10000) {
        snprintf(text, size, "%d.%dk", (int)(steps / 1000), (int)(steps % 1000 / 100));
      } else {
        snprintf(text, size, "%dk", (int)(steps / 1000));
      }
      break;
    }
    case ITEM_HEART_RATE: {
      const int32_t bpm = heart_rate();
      if (bpm < 0) {
        snprintf(text, size, "--");
      } else {
        snprintf(text, size, brief ? "%d" : "%d bpm", (int)bpm);
      }
      break;
    }
    default:
      text[0] = '\0';
      break;
  }
}

static int16_t text_width(const char *text) {
  return graphics_text_layout_get_content_size(text, s_text_font, GRect(0, 0, 200, 40),
                                               GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// Half the width of the ring at a distance below its centre.
static int16_t ring_half_width(int16_t below) {
  return isqrt(CLEAR_RADIUS * CLEAR_RADIUS - below * below);
}

// One or two values under a pencil rule, split by a short upright stroke.
// Values switch to their short forms if they would not fit inside the ring.
static void draw_values(GContext *ctx, GPoint centre, Item main, Item second,
                        const struct tm *t) {
  const int16_t rule_y = centre.y + RULE_DROP;
  const int16_t text_top = rule_y + 3;
  const int16_t room = ring_half_width(RULE_DROP + 3 + TEXT_HEIGHT) * 2 - 6;

  char first[16];
  char other[16];
  int16_t first_w = 0;
  int16_t other_w = 0;
  int16_t total = 0;
  // Try the full forms, then a short second value, then both short.
  for (int attempt = 0; attempt < 3; attempt++) {
    format_item(main, attempt == 2, t, first, sizeof(first));
    format_item(second, attempt >= 1, t, other, sizeof(other));
    first_w = text_width(first);
    other_w = second != ITEM_NONE ? text_width(other) : 0;
    total = first_w + (second != ITEM_NONE ? TEXT_GAP * 2 + other_w : 0);
    if (total <= room) {
      break;
    }
  }

  const int16_t left = centre.x - total / 2;
  const int16_t rule_half_max = ring_half_width(RULE_DROP) - 3;
  const int16_t rule_half = total / 2 + 3 < rule_half_max ? total / 2 + 3 : rule_half_max;
  sketch_line(ctx, GPoint(centre.x - rule_half, rule_y), GPoint(centre.x + rule_half, rule_y),
              s_palette.ink);

  graphics_context_set_text_color(ctx, s_palette.ink);
  graphics_draw_text(ctx, first, s_text_font,
                     GRect(left, text_top - TEXT_PAD, first_w + 2, TEXT_HEIGHT + TEXT_PAD * 2),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (second == ITEM_NONE) {
    return;
  }
  const int16_t divider = left + first_w + TEXT_GAP;
  sketch_line(ctx, GPoint(divider, rule_y + 1), GPoint(divider, text_top + TEXT_HEIGHT + 1),
              s_palette.ink);
  graphics_draw_text(ctx, other, s_text_font,
                     GRect(divider + TEXT_GAP, text_top - TEXT_PAD, other_w + 2,
                           TEXT_HEIGHT + TEXT_PAD * 2),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// ---------------------------------------------------------------------------
// Widget screens

static bool screen_used(int screen) {
  return s_settings.screens[screen][0] != ITEM_NONE || s_settings.screens[screen][1] != ITEM_NONE;
}

// The next screen with something on it, or the same one if it is the only one.
static int next_screen(int from) {
  for (int i = 1; i <= SCREEN_COUNT; i++) {
    const int candidate = (from + i) % SCREEN_COUNT;
    if (screen_used(candidate)) {
      return candidate;
    }
  }
  return from;
}

// The items on the current screen. A screen with only a second item shows
// it as the main one, and with nothing chosen anywhere the weather and date
// are shown.
static void screen_items(Item *main, Item *second) {
  if (!screen_used(s_screen)) {
    s_screen = next_screen(s_screen);
  }
  *main = s_settings.screens[s_screen][0];
  *second = s_settings.screens[s_screen][1];
  if (*main == ITEM_NONE) {
    *main = *second;
    *second = ITEM_NONE;
  }
  if (*main == ITEM_NONE) {
    *main = ITEM_WEATHER;
    *second = ITEM_DATE;
  }
}

#if defined(PBL_HEALTH)
static bool item_shown(Item item) {
  Item main;
  Item second;
  screen_items(&main, &second);
  return main == item || second == item;
}
#endif

static void draw_widget(GContext *ctx, GPoint centre, const struct tm *t) {
  Item main;
  Item second;
  screen_items(&main, &second);
  draw_item_picture(ctx, main, GPoint(centre.x, centre.y - ICON_RISE), t);
  draw_values(ctx, centre, main, second, t);
}

// ---------------------------------------------------------------------------
// Status icons, tucked inside the ticks at 1:30 and 10:30

static void draw_battery_status(GContext *ctx, GPoint centre) {
  const bool low = s_battery.charge_percent <= LOW_BATTERY_PERCENT;
  if (!s_settings.always_show_battery && !low && !s_battery.is_charging) {
    return;
  }
  const GRect body = GRect(centre.x - 8, centre.y - 4, 14, 8);
  const GColor level_color = s_battery.is_charging ? s_palette.charging
    : low ? s_palette.warning : s_palette.ink;

  const GPoint corners[] = {
    body.origin, GPoint(body.origin.x + body.size.w, body.origin.y),
    GPoint(body.origin.x + body.size.w, body.origin.y + body.size.h),
    GPoint(body.origin.x, body.origin.y + body.size.h),
  };
  for (int i = 0; i < 4; i++) {
    sketch_line(ctx, corners[i], corners[(i + 1) % 4], s_palette.ink);
  }
  graphics_context_set_fill_color(ctx, s_palette.ink);
  graphics_fill_rect(ctx, GRect(body.origin.x + body.size.w + 1, centre.y - 2, 2, 4),
                     0, GCornerNone);

  const int16_t inner_w = body.size.w - 3;
  const int16_t level = inner_w * s_battery.charge_percent / 100;
  graphics_context_set_fill_color(ctx, level_color);
  graphics_fill_rect(ctx, GRect(body.origin.x + 2, body.origin.y + 2,
                                level > 0 ? level : 1, body.size.h - 3), 0, GCornerNone);
}

// Bluetooth rune, struck through, shown only while the phone is disconnected.
static void draw_bluetooth_off(GContext *ctx, GPoint origin) {
  const GPoint p[] = {
    { 0, 3 }, { 6, 9 }, { 3, 12 }, { 3, 0 }, { 6, 3 }, { 0, 9 },
  };
  for (size_t i = 1; i < ARRAY_LENGTH(p); i++) {
    sketch_line(ctx, GPoint(origin.x + p[i - 1].x, origin.y + p[i - 1].y),
                GPoint(origin.x + p[i].x, origin.y + p[i].y), s_palette.warning);
  }
  sketch_line(ctx, GPoint(origin.x - 1, origin.y + 12), GPoint(origin.x + 7, origin.y),
              s_palette.warning);
}

// Crescent for Quiet Time.
static void draw_quiet_time(GContext *ctx, GPoint origin) {
  const GPoint centre = GPoint(origin.x + 6, origin.y + 6);
  graphics_context_set_fill_color(ctx, s_palette.ink);
  graphics_fill_circle(ctx, centre, 6);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, GPoint(centre.x + 3, centre.y - 2), 5);
}

static void draw_alerts(GContext *ctx, GPoint centre) {
  const bool quiet = quiet_time_is_active();
  const bool disconnected = !s_bluetooth_connected;
  const int count = (quiet ? 1 : 0) + (disconnected ? 1 : 0);
  if (count == 0) {
    return;
  }
  const int16_t icon_w = 12;
  const int16_t spacing = 4;
  int16_t x = centre.x - (count * icon_w + (count - 1) * spacing) / 2;
  const int16_t y = centre.y - 6;
  if (disconnected) {
    draw_bluetooth_off(ctx, GPoint(x + 3, y));
    x += icon_w + spacing;
  }
  if (quiet) {
    draw_quiet_time(ctx, GPoint(x, y));
  }
}

// ---------------------------------------------------------------------------
// Drawing

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
  if (s_settings.hour_numbers) {
    draw_hour_numbers(ctx, bounds, centre);
  }

  // Status icons move further in when hour numbers are shown, to clear them.
  const int32_t corner_right = TRIG_MAX_ANGLE / 8;
  const int32_t corner_left = -TRIG_MAX_ANGLE / 8;
  const int16_t inset = HOUR_TICK_LENGTH + 12
    + (s_settings.hour_numbers ? NUMERAL_RADIUS * 2 : 0);
  draw_battery_status(ctx, ray_point(centre, corner_right,
                                     edge_distance(bounds, corner_right) - inset));
  draw_alerts(ctx, ray_point(centre, corner_left,
                             edge_distance(bounds, corner_left) - inset));

  // The widget and the ring that keeps the hands out of it.
  sketch_circle(ctx, centre, CLEAR_RADIUS, s_palette.faint, NULL, NULL);
  draw_widget(ctx, centre, t);

  // The hands run from just outside the ring towards the ticks.
  const int32_t reach_x = edge_distance(bounds, TRIG_MAX_ANGLE / 4);
  const int32_t reach_y = edge_distance(bounds, 0);
  const int16_t start = CLEAR_RADIUS + HAND_GAP;
  const int16_t minute_end = (reach_x < reach_y ? reach_x : reach_y) - HOUR_TICK_LENGTH - 2;
  const int16_t hour_end = start + (minute_end - start) * 60 / 100;
  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;
  draw_hand(ctx, centre, hour_angle, start, hour_end, HOUR_HAND_WIDTH);
  draw_hand(ctx, centre, minute_angle, start, minute_end, MINUTE_HAND_WIDTH);
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
  if (s_settings.rotate_screens) {
    s_screen = next_screen(s_screen);
  }
  layer_mark_dirty(s_canvas);
  if (tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

// A tap on the watch moves to the next widget screen.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  const int next = next_screen(s_screen);
  if (next != s_screen) {
    s_screen = next;
    layer_mark_dirty(s_canvas);
  }
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if (item_shown(ITEM_STEPS) || item_shown(ITEM_HEART_RATE)) {
    layer_mark_dirty(s_canvas);
  }
}
#endif

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
  static const uint32_t *screen_keys[SCREEN_COUNT][2] = {
    { &MESSAGE_KEY_SCREEN_1_MAIN, &MESSAGE_KEY_SCREEN_1_SECOND },
    { &MESSAGE_KEY_SCREEN_2_MAIN, &MESSAGE_KEY_SCREEN_2_SECOND },
    { &MESSAGE_KEY_SCREEN_3_MAIN, &MESSAGE_KEY_SCREEN_3_SECOND },
    { &MESSAGE_KEY_SCREEN_4_MAIN, &MESSAGE_KEY_SCREEN_4_SECOND },
  };
  bool changed = false;
  const Tuple *tuple;

  for (int screen = 0; screen < SCREEN_COUNT; screen++) {
    for (int slot = 0; slot < 2; slot++) {
      if ((tuple = dict_find(iterator, *screen_keys[screen][slot]))) {
        const int32_t item = tuple_int(tuple);
        s_settings.screens[screen][slot] = item >= 0 && item < ITEM_COUNT ? item : ITEM_NONE;
        changed = true;
      }
    }
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_ROTATE_SCREENS))) {
    s_settings.rotate_screens = tuple_int(tuple) != 0;
    changed = true;
  }

  if ((tuple = dict_find(iterator, MESSAGE_KEY_THEME))) {
    s_settings.dark_theme = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BATTERY))) {
    s_settings.always_show_battery = tuple_int(tuple) == 1;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_HOUR_NUMBERS))) {
    s_settings.hour_numbers = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DISCONNECT_VIBE))) {
    s_settings.disconnect_vibe = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE_UNIT))
      && tuple->type == TUPLE_CSTRING) {
    const char *unit = tuple->value->cstring;
    s_settings.temperature_unit = unit[0] == 'F' ? 'F' : 'C';
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
    s_daytime = tuple_int(daytime) != 0;
    persist_write_bool(PERSIST_KEY_DAYTIME, s_daytime);
  }

  if (read_settings(iterator)) {
    s_screen = 0;
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
}

static void init(void) {
  s_text_font = fonts_get_system_font(TEXT_FONT);
  s_numeral_font = fonts_get_system_font(NUMERAL_FONT);
  s_day_font = fonts_get_system_font(DAY_FONT);
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
  accel_tap_service_subscribe(tap_handler);
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
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
  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
