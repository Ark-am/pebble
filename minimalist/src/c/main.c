// Minimalist: a quiet watch face for the Pebble 2 Duo, analog or digital.
//
// Analog draws twelve hour marks around the edge of the screen and two hands
// in one of five shapes. Digital is laid out like Retro F-91W's LCD: a top row
// with the day and the month and date, and the time below across the width,
// with PM to its right in 12-hour time. Both styles can hide the date. Both show a
// name, PEBBLE unless the wearer changes or clears it, and up to two widgets
// (weather, steps or battery): beside the centre on the dial, or in a row
// below the digital time. A crossed-out Bluetooth sign shows while the phone
// is disconnected, and the watch buzzes long and hard as it goes, unless the
// wearer turns it off. All the text comes in one of four font
// styles (src/c/font_styles.h). The settings come from the
// phone (src/pkjs/config.js) and are kept on the watch.

#include <pebble.h>

#include "font_styles.h"

// ---------------------------------------------------------------- Settings

enum { STYLE_ANALOG = 0, STYLE_DIGITAL = 1 };
enum { FORMAT_WATCH = 0, FORMAT_12H = 1, FORMAT_24H = 2 };
enum { HANDS_BATON, HANDS_DAUPHINE, HANDS_SKELETON, HANDS_LOLLIPOP, HANDS_LEAF, HANDS_COUNT };
// Values match WIDGET_OPTIONS in src/pkjs/config.js.
// The Pebble 2 Duo has no heart-rate sensor, so there is no heart-rate widget.
enum { WIDGET_NONE, WIDGET_WEATHER, WIDGET_STEPS, WIDGET_BATTERY, WIDGET_COUNT };
enum { WIDGET_LEFT, WIDGET_RIGHT };

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_TEMPERATURE 2
#define PERSIST_KEY_CONDITION 3
#define PERSIST_KEY_DAYTIME 4
#define SETTINGS_VERSION 8
// Version 1 ended before the name, version 2 before the numbers, and version 3
// before the font and hand styles, version 4 before the widgets, and version 5
// before the disconnection sign, version 6 before the battery threshold, and
// version 7 before the widget in the name's place.
#define SETTINGS_V1_SIZE offsetof(Settings, brand)
#define SETTINGS_V2_SIZE offsetof(Settings, numbers)
#define SETTINGS_V3_SIZE offsetof(Settings, font_style)
#define SETTINGS_V4_SIZE offsetof(Settings, widgets)
#define SETTINGS_V5_SIZE offsetof(Settings, show_disconnect)
#define SETTINGS_V6_SIZE offsetof(Settings, battery_below)
#define SETTINGS_V7_SIZE offsetof(Settings, widget_top)
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
  uint8_t widgets[2];  // Left and right.
  bool show_disconnect;  // The Bluetooth sign while the phone is disconnected.
  uint8_t battery_below;  // Show the battery widget only at or below this %; 0 is always.
  uint8_t widget_top;  // Shown in the name's place when the name is empty.
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
  .widgets = { WIDGET_NONE, WIDGET_NONE },
  .show_disconnect = true,
  .battery_below = 0,
  .widget_top = WIDGET_NONE,
};

// Whether the phone is connected, kept up to date by connection_service.
static bool s_connected = true;

// The weather, as the phone last sent it (src/pkjs/index.js); the watch asks
// for it again every WEATHER_REFRESH_MINUTES while a widget shows it.
#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN
static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[8];
static bool s_daytime = true;

// ---------------------------------------------------------------- Layout

// Hour marks sit this far in from the screen edge.
#define MARK_INSET 5
#define MARK_LONG 12   // 12, 3, 6 and 9
// Or, in their place, numerals with their outer edge at MARK_INSET.
#define MARK_SHORT 7
#define MARK_LONG_WIDTH 3
#define MARK_SHORT_WIDTH 1

#define HOUR_HAND_LENGTH 42
// The minute hand reaches just into the long marks at 3 and 9.
#define MINUTE_HAND_LENGTH 59
#define HUB_RADIUS 5
// Baton: lines this wide.
#define HOUR_BATON_WIDTH 7
#define MINUTE_BATON_WIDTH 5
// Dauphine, skeleton and leaf: half the width at the widest point.
#define HOUR_HALF_WIDTH 6
#define MINUTE_HALF_WIDTH 5
#define SKELETON_HOUR_HALF_WIDTH 4
#define SKELETON_MINUTE_HALF_WIDTH 3
// How far the shapes reach back past the centre.
#define HAND_TAIL 6
// Lollipop: a line this wide, ending in a ring of this radius at the tip.
#define HOUR_LOLLIPOP_WIDTH 4
#define MINUTE_LOLLIPOP_WIDTH 2
#define HOUR_RING_RADIUS 6
#define MINUTE_RING_RADIUS 5
#define RING_THICKNESS 2

// The bottom of the name's capitals sits this far above the centre on the
// analog dial, and the top of the date's this far below it.
#define ANALOG_BRAND_OFFSET 31
#define ANALOG_DATE_OFFSET 31

// Widgets: an icon WIDGET_ICON_EXTRA px taller than the name's capitals and
// the value in the name's font, WIDGET_GAP apart. On the dial they sit left
// and right of the centre, between the hub and the marks or numerals, keeping
// WIDGET_GAP from both, with the icon above the value. On the digital face
// they sit in a row ROW_GAP below the time, at its left and right edges, with
// the icon before the value.
#define WIDGET_ICON_EXTRA 5
#define WIDGET_GAP 3

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
static int16_t s_numeral_w;  // The wider of the dial's 3 and 9.

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
  const Text *t = &s_text[ROLE_DATE];
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

// ---------------------------------------------------------------- Widgets
//
// Small icons drawn in a square of the given size, in percentages of that size
// so they scale with the text, as in Alien Core.

typedef enum {
  ICON_NONE,
  ICON_STEPS,
  ICON_BATTERY,
  ICON_SUN,
  ICON_MOON,
  ICON_CLOUD,
  ICON_RAIN,
  ICON_SNOW,
  ICON_STORM,
  ICON_FOG,
} Icon;

#define PC(origin, percent) ((origin) + size * (percent) / 100)

static void icon_dot(GContext *ctx, GPoint at, int16_t size, int x, int y, int radius) {
  const int16_t r = size * radius / 100;
  graphics_fill_circle(ctx, GPoint(PC(at.x, x), PC(at.y, y)), r > 0 ? r : 1);
}

static void icon_line(GContext *ctx, GPoint at, int16_t size, int x0, int y0, int x1, int y1) {
  graphics_draw_line(ctx, GPoint(PC(at.x, x0), PC(at.y, y0)), GPoint(PC(at.x, x1), PC(at.y, y1)));
}

// A cloud across the top of the square, with room for weather below it.
static void icon_cloud(GContext *ctx, GPoint at, int16_t size) {
  icon_dot(ctx, at, size, 28, 42, 22);
  icon_dot(ctx, at, size, 58, 30, 28);
  icon_dot(ctx, at, size, 78, 46, 18);
  graphics_fill_rect(ctx, GRect(PC(at.x, 12), PC(at.y, 42), size * 76 / 100, size * 22 / 100), 0,
                     GCornerNone);
}

// Two footprints, one a step ahead of the other.
static void icon_steps(GContext *ctx, GPoint at, int16_t size) {
  icon_dot(ctx, at, size, 28, 58, 19);
  icon_dot(ctx, at, size, 28, 88, 11);
  icon_dot(ctx, at, size, 72, 26, 19);
  icon_dot(ctx, at, size, 72, 56, 11);
}

// A battery on its side, wider than its square, filled to the charge.
static void icon_battery(GContext *ctx, GPoint at, int16_t size) {
  const int16_t width = size * 3 / 2 - 2;
  const int16_t height = size * 7 / 10;
  const GRect body = GRect(at.x, at.y + (size - height) / 2, width, height);
  const int percent = battery_state_service_peek().charge_percent;
  graphics_draw_rect(ctx, body);
  graphics_fill_rect(ctx, GRect(body.origin.x + 2, body.origin.y + 2,
                                (width - 4) * percent / 100, height - 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(body.origin.x + width, body.origin.y + height / 4, 2, height / 2),
                     0, GCornerNone);
}

static int16_t icon_width(Icon icon, int16_t size) {
  return icon == ICON_NONE ? 0 : icon == ICON_BATTERY ? size * 3 / 2 : size;
}

static void draw_icon(GContext *ctx, Icon icon, GPoint at, int16_t size) {
  graphics_context_set_fill_color(ctx, foreground());
  graphics_context_set_stroke_color(ctx, foreground());
  graphics_context_set_stroke_width(ctx, 1);
  switch (icon) {
    case ICON_STEPS: icon_steps(ctx, at, size); break;
    case ICON_BATTERY: icon_battery(ctx, at, size); break;
    case ICON_SUN: {
      icon_dot(ctx, at, size, 50, 50, 25);
      const GPoint middle = GPoint(PC(at.x, 50), PC(at.y, 50));
      for (int i = 0; i < 8; i++) {
        const int32_t angle = TRIG_MAX_ANGLE * i / 8;
        graphics_draw_line(ctx, along(middle, angle, size * 36 / 100),
                           along(middle, angle, size * 49 / 100));
      }
      break;
    }
    case ICON_MOON:
      icon_dot(ctx, at, size, 50, 50, 44);
      graphics_context_set_fill_color(ctx, background());
      icon_dot(ctx, at, size, 70, 36, 38);
      break;
    case ICON_CLOUD:
      icon_cloud(ctx, GPoint(at.x, at.y + size / 8), size);
      break;
    case ICON_RAIN:
      icon_cloud(ctx, at, size);
      icon_line(ctx, at, size, 28, 74, 20, 96);
      icon_line(ctx, at, size, 54, 74, 46, 96);
      icon_line(ctx, at, size, 80, 74, 72, 96);
      break;
    case ICON_SNOW:
      icon_cloud(ctx, at, size);
      icon_dot(ctx, at, size, 24, 84, 7);
      icon_dot(ctx, at, size, 50, 92, 7);
      icon_dot(ctx, at, size, 76, 84, 7);
      break;
    case ICON_STORM:
      icon_cloud(ctx, at, size);
      icon_line(ctx, at, size, 58, 66, 42, 84);
      icon_line(ctx, at, size, 42, 84, 58, 84);
      icon_line(ctx, at, size, 58, 84, 44, 100);
      break;
    case ICON_FOG:
      icon_cloud(ctx, at, size);
      icon_line(ctx, at, size, 8, 78, 92, 78);
      icon_line(ctx, at, size, 20, 94, 80, 94);
      break;
    default:
      break;
  }
}

#undef PC

// The icon for the current weather. It is the moon on a clear night.
static Icon weather_icon(void) {
  static const struct { const char *word; Icon icon; } ICONS[] = {
    { "FAIR", ICON_CLOUD }, { "CLOUDY", ICON_CLOUD }, { "FOG", ICON_FOG },
    { "DRIZZLE", ICON_RAIN }, { "RAIN", ICON_RAIN }, { "SHOWERS", ICON_RAIN },
    { "SNOW", ICON_SNOW }, { "STORM", ICON_STORM },
  };
  if (strcmp(s_condition, "CLEAR") == 0) {
    return s_daytime ? ICON_SUN : ICON_MOON;
  }
  for (size_t i = 0; i < ARRAY_LENGTH(ICONS); i++) {
    if (strcmp(s_condition, ICONS[i].word) == 0) {
      return ICONS[i].icon;
    }
  }
  return ICON_CLOUD;
}

static Icon widget_icon(uint8_t widget) {
  switch (widget) {
    case WIDGET_WEATHER: return weather_icon();
    case WIDGET_STEPS: return ICON_STEPS;
    case WIDGET_BATTERY: return ICON_BATTERY;
    default: return ICON_NONE;
  }
}

// The value a widget shows beside its icon; -- when there is none yet.
static void widget_text(uint8_t widget, char *out, size_t size) {
  out[0] = '\0';
  switch (widget) {
    case WIDGET_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        snprintf(out, size, "--\xc2\xb0");
      } else {
        snprintf(out, size, "%d\xc2\xb0", (int)s_temperature);
      }
      break;
    case WIDGET_STEPS: {
#if defined(PBL_HEALTH)
      const int steps = health_service_sum_today(HealthMetricStepCount);
      if (steps >= 1000) {
        snprintf(out, size, "%d,%03d", steps / 1000, steps % 1000);
      } else {
        snprintf(out, size, "%d", steps);
      }
#else
      snprintf(out, size, "--");
#endif
      break;
    }
    case WIDGET_BATTERY:
      snprintf(out, size, "%d%%", battery_state_service_peek().charge_percent);
      break;
    default:
      break;
  }
}

static int16_t widget_icon_size(void) {
  return s_text[ROLE_SMALL].height + WIDGET_ICON_EXTRA;
}

// The height of a widget with its icon above its value.
static int16_t stacked_widget_height(void) {
  return widget_icon_size() + WIDGET_GAP + s_text[ROLE_SMALL].height;
}

// A widget centred on `middle`. Stacked, its icon sits above its value, both
// centred on x; otherwise the icon comes before the value, starting at `x`
// (left), ending at it (right) or centred on it. A value too wide for `room`
// ends in an ellipsis.
// Whether a widget shows now: the battery can wait until the charge falls to
// the chosen level.
static bool widget_visible(uint8_t widget) {
  if (widget == WIDGET_NONE || widget >= WIDGET_COUNT) {
    return false;
  }
  if (widget == WIDGET_BATTERY && s_settings.battery_below) {
    return battery_state_service_peek().charge_percent <= s_settings.battery_below;
  }
  return true;
}

static void draw_widget(GContext *ctx, uint8_t widget, GPoint middle, int16_t room,
                        GTextAlignment alignment, bool stacked) {
  if (!widget_visible(widget)) {
    return;
  }
  char text[16];
  widget_text(widget, text, sizeof(text));
  const Text *t = &s_text[ROLE_SMALL];
  const Icon icon = widget_icon(widget);
  const int16_t size = widget_icon_size();
  const int16_t icon_w = icon_width(icon, size);
  const int16_t text_w = graphics_text_layout_get_content_size(
    text, t->font, GRect(0, 0, 200, 50), GTextOverflowModeFill, GTextAlignmentLeft).w;
  GPoint icon_at;
  GRect text_box;
  if (stacked) {
    const int16_t shown_w = text_w < room ? text_w : room;
    const int16_t top = middle.y - stacked_widget_height() / 2;
    const int16_t text_top = top + size + WIDGET_GAP;
    icon_at = GPoint(middle.x - icon_w / 2, top);
    text_box = GRect(middle.x - shown_w / 2 - 1, text_top - t->top, shown_w + 2,
                     t->top + t->height + 4);
  } else {
    const int16_t room_w = room - icon_w - WIDGET_GAP;
    const int16_t shown_w = text_w < room_w ? text_w : room_w;
    const int16_t total = icon_w + WIDGET_GAP + shown_w;
    const int16_t start = alignment == GTextAlignmentLeft ? middle.x
                        : alignment == GTextAlignmentRight ? middle.x - total
                        : middle.x - total / 2;
    icon_at = GPoint(start, middle.y - size / 2);
    text_box = GRect(start + icon_w + WIDGET_GAP, middle.y - t->height / 2 - t->top, shown_w + 2,
                     t->top + t->height + 4);
  }
  draw_icon(ctx, icon, icon_at, size);
  graphics_context_set_text_color(ctx, foreground());
  graphics_draw_text(ctx, text, t->font, text_box, GTextOverflowModeTrailingEllipsis,
                     stacked ? GTextAlignmentCenter : GTextAlignmentLeft, NULL);
}

static bool widgets_shown(void) {
  return widget_visible(s_settings.widgets[WIDGET_LEFT]) ||
         widget_visible(s_settings.widgets[WIDGET_RIGHT]);
}

// The Bluetooth sign with a small cross at its lower right, in a square
// BT_SIGN_EXTRA px taller than the name's capitals, centred on `middle`.
#define BT_SIGN_EXTRA 7
static void draw_disconnected(GContext *ctx, GPoint middle) {
  const int16_t size = s_text[ROLE_SMALL].height + BT_SIGN_EXTRA;
  const GPoint at = GPoint(middle.x - size / 2, middle.y - size / 2);
  // The sign sits a little left of centre, to make room for the cross.
  const GPoint sign = GPoint(at.x - size * 3 / 16, at.y);
#define P(origin, px, py) GPoint((origin).x + size * (px) / 100, (origin).y + size * (py) / 100)
  graphics_context_set_stroke_color(ctx, foreground());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, P(sign, 50, 0), P(sign, 50, 100));
  graphics_draw_line(ctx, P(sign, 50, 0), P(sign, 78, 25));
  graphics_draw_line(ctx, P(sign, 78, 25), P(sign, 22, 75));
  graphics_draw_line(ctx, P(sign, 22, 25), P(sign, 78, 75));
  graphics_draw_line(ctx, P(sign, 78, 75), P(sign, 50, 100));
  graphics_draw_line(ctx, P(at, 70, 62), P(at, 100, 92));
  graphics_draw_line(ctx, P(at, 100, 62), P(at, 70, 92));
#undef P
}

static bool disconnect_shown(void) {
  return s_settings.show_disconnect && !s_connected;
}

// The widget in the name's place, if the name is empty and it shows now.
static uint8_t top_widget(void) {
  return !s_settings.brand[0] && widget_visible(s_settings.widget_top) ? s_settings.widget_top
                                                                       : WIDGET_NONE;
}

static bool weather_shown(void) {
  return s_settings.widgets[WIDGET_LEFT] == WIDGET_WEATHER ||
         s_settings.widgets[WIDGET_RIGHT] == WIDGET_WEATHER ||
         (!s_settings.brand[0] && s_settings.widget_top == WIDGET_WEATHER);
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

  const int16_t brand_top = centre.y - ANALOG_BRAND_OFFSET - s_text[ROLE_DATE].height;
  const int16_t brand_middle = brand_top + s_text[ROLE_DATE].height / 2;
  const uint8_t widget_top = top_widget();
  if (s_settings.brand[0]) {
    draw_brand(ctx, bounds, brand_top);
  } else if (widget_top != WIDGET_NONE) {
    draw_widget(ctx, widget_top, GPoint(centre.x, brand_middle), bounds.size.w - BRAND_MARGIN * 2,
                GTextAlignmentCenter, false);
  }
  // The disconnection sign: halfway between the 12 and the name (or the
  // widget in its place), or in the name's place when there is neither.
  if (disconnect_shown()) {
    const int16_t twelve = MARK_INSET + (s_settings.numbers ? s_text[ROLE_NUMERALS].height
                                                             : MARK_LONG);
    const int16_t line_top = s_settings.brand[0] ? brand_top
                           : widget_top != WIDGET_NONE ? brand_middle - widget_icon_size() / 2
                           : -1;
    const int16_t middle = line_top >= 0 ? (twelve + line_top) / 2 : brand_middle;
    draw_disconnected(ctx, GPoint(centre.x, middle));
  }
  if (s_settings.show_date) {
    char date[8];
    snprintf(date, sizeof(date), "%s %d", DAYS[s_now.tm_wday], s_now.tm_mday);
    draw_text_at(ctx, date, ROLE_DATE, 0, centre.y + ANALOG_DATE_OFFSET, bounds.size.w,
                 GTextAlignmentCenter);
  }

  // The widgets, centred between the hub and the 9 and 3.
  if (widgets_shown()) {
    const int16_t outer = MARK_INSET + (s_settings.numbers ? s_numeral_w : MARK_LONG) + WIDGET_GAP;
    const int16_t inner = centre.x - HUB_RADIUS - WIDGET_GAP;
    const int16_t room = inner - outer;
    draw_widget(ctx, s_settings.widgets[WIDGET_LEFT], GPoint(outer + room / 2, centre.y), room,
                GTextAlignmentCenter, true);
    draw_widget(ctx, s_settings.widgets[WIDGET_RIGHT],
                GPoint(bounds.size.w - outer - room / 2, centre.y), room, GTextAlignmentCenter,
                true);
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
  // The name's line holds the name, or the widget in its place.
  const uint8_t widget_top = top_widget();
  const bool brand = s_settings.brand[0] != '\0' || widget_top != WIDGET_NONE;
  const int16_t brand_h = s_settings.brand[0] ? s_text[ROLE_DATE].height : widget_icon_size();
  const int16_t pm_h = s_text[ROLE_SMALL].height;
  const int16_t time_h = s_text[time_role].height;
  const int16_t row_h = s_settings.show_date ? s_text[ROLE_ROW].height : 0;
  const int16_t widgets_h = widgets_shown() ? widget_icon_size() : 0;
  const int16_t block = (brand ? brand_h + BRAND_GAP : 0) + (row_h ? row_h + ROW_GAP : 0) + time_h +
                        (widgets_h ? ROW_GAP + widgets_h : 0);
  const int16_t top = (bounds.size.h - block) / 2;
  const int16_t row_top = top + (brand ? brand_h + BRAND_GAP : 0);
  const int16_t time_top = row_top + (row_h ? row_h + ROW_GAP : 0);

  if (s_settings.brand[0]) {
    draw_brand(ctx, bounds, top);
  } else if (widget_top != WIDGET_NONE) {
    draw_widget(ctx, widget_top, GPoint(bounds.size.w / 2, top + brand_h / 2),
                bounds.size.w - BRAND_MARGIN * 2, GTextAlignmentCenter, false);
  }
  // The disconnection sign, halfway between the top of the screen and the
  // rest, so nothing moves when it comes and goes.
  if (disconnect_shown()) {
    draw_disconnected(ctx, GPoint(bounds.size.w / 2, top / 2));
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

  // The widgets, in a row below the time at its left and right edges.
  if (widgets_h) {
    const int16_t middle = time_top + time_h + ROW_GAP + widgets_h / 2;
    const int16_t room = width / 2 - WIDGET_GAP;
    draw_widget(ctx, s_settings.widgets[WIDGET_LEFT], GPoint(left, middle), room,
                GTextAlignmentLeft, false);
    draw_widget(ctx, s_settings.widgets[WIDGET_RIGHT], GPoint(left + width, middle), room,
                GTextAlignmentRight, false);
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
  const int16_t three = text_width("3", s_text[ROLE_NUMERALS].font);
  const int16_t nine = text_width("9", s_text[ROLE_NUMERALS].font);
  s_numeral_w = three > nine ? three : nine;
}

// ---------------------------------------------------------------- Events

static void request_weather(void) {
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();
}

// Three long buzzes when the phone disconnects, so it is hard to miss. Pebble
// has no strength setting, so the length makes it intense.
static const uint32_t DISCONNECT_VIBE[] = { 800, 200, 800, 200, 800 };

// Redraws as the charge changes, so the battery widget appears on time.
static void battery_handler(BatteryChargeState state) {
  layer_mark_dirty(s_canvas);
}

static void connection_handler(bool connected) {
  if (s_connected && !connected && s_settings.show_disconnect && !quiet_time_is_active()) {
    vibes_enqueue_custom_pattern((VibePattern) {
      .durations = DISCONNECT_VIBE,
      .num_segments = ARRAY_LENGTH(DISCONNECT_VIBE),
    });
  }
  s_connected = connected;
  layer_mark_dirty(s_canvas);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  if (weather_shown() && tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
  layer_mark_dirty(s_canvas);
}

static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  const Settings before = s_settings;
  const Tuple *tuple;

  // The weather, from src/pkjs/index.js.
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE))) {
    s_temperature = tuple->value->int32;
    persist_write_int(PERSIST_KEY_TEMPERATURE, s_temperature);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CONDITION)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_condition, tuple->value->cstring, sizeof(s_condition) - 1);
    s_condition[sizeof(s_condition) - 1] = '\0';
    persist_write_string(PERSIST_KEY_CONDITION, s_condition);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DAYTIME))) {
    s_daytime = tuple_int(tuple) != 0;
    persist_write_bool(PERSIST_KEY_DAYTIME, s_daytime);
  }

  // The settings, from the settings page.
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
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BATTERY_BELOW))) {
    const int32_t below = tuple_int(tuple);
    s_settings.battery_below = (below > 0 && below < 100) ? below : 0;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SHOW_DISCONNECT))) {
    s_settings.show_disconnect = tuple_int(tuple) != 0;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_WIDGET_TOP))) {
    const int32_t widget = tuple_int(tuple);
    s_settings.widget_top = (widget >= 0 && widget < WIDGET_COUNT) ? widget : WIDGET_NONE;
  }
  const uint32_t widget_keys[] = { MESSAGE_KEY_WIDGET_LEFT, MESSAGE_KEY_WIDGET_RIGHT };
  for (int side = WIDGET_LEFT; side <= WIDGET_RIGHT; side++) {
    if ((tuple = dict_find(iterator, widget_keys[side]))) {
      const int32_t widget = tuple_int(tuple);
      s_settings.widgets[side] = (widget >= 0 && widget < WIDGET_COUNT) ? widget : WIDGET_NONE;
    }
  }

  if (memcmp(&before, &s_settings, sizeof(s_settings)) != 0) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
  }
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
      (saved.version == 7 && size == (int)SETTINGS_V7_SIZE) ||
      (saved.version == 6 && size == (int)SETTINGS_V6_SIZE) ||
      (saved.version == 5 && size == (int)SETTINGS_V5_SIZE) ||
      (saved.version == 4 && size == (int)SETTINGS_V4_SIZE) ||
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
    for (int side = WIDGET_LEFT; side <= WIDGET_RIGHT; side++) {
      if (s_settings.widgets[side] >= WIDGET_COUNT) {
        s_settings.widgets[side] = WIDGET_NONE;
      }
    }
    if (s_settings.widget_top >= WIDGET_COUNT) {
      s_settings.widget_top = WIDGET_NONE;
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

static void load_weather(void) {
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
  load_settings();
  load_weather();

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
  s_connected = connection_service_peek_pebble_app_connection();
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = connection_handler,
  });
  app_message_register_inbox_received(inbox_received);
  // Room for every setting the settings page sends at once.
  app_message_open(256, 16);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
  unload_fonts();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
