#include <pebble.h>

#include "avatars.h"
#include "font_styles.h"
#if defined(PBL_SPEAKER)
#include "sounds.h"
#endif

// An alien-tech watch face: a glowing core inside a ring of twelve armoured
// plates. The core shows the time as glowing digits or as hands, and the
// plate for the current hour glows, with the current minute's five-minute
// plate glowing more dimly.
//
// Each plate also holds one of twelve avatars. A flick of the wrist, or a long
// press on touch watches, opens the selector: a light runs round the plates
// and the avatar of the plate it rests on shows in the core. Swipes
// or dragging round the plates, or further flicks without a touchscreen, step
// the light through them. Tapping the core, or leaving it for a few seconds, transforms the face into that avatar: the core flares and opens
// like an iris, the plates flash, and sparks and shock rings fly out. The
// avatars are alive while they show: they blink, sway, flap and scuttle. After
// a few seconds the time comes back.
//
// The face also powers up plate by plate when it appears, and a light runs
// round the ring on the hour. Nothing else moves while the time shows, so the
// face costs little battery.
//
// Watches with a speaker add sound effects: the light ratchets round, each
// plate has its own note, the transformation surges and booms, and each avatar
// has a call of its own.

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_AVATAR 2
#define PERSIST_KEY_TEMPERATURE 3
#define PERSIST_KEY_CONDITION 4
#define PERSIST_KEY_DAYTIME 5

#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 8

// Sizes for the name in the core. Values match the NAME_SIZE options in
// src/pkjs/config.js.
enum { NAME_SMALL, NAME_MEDIUM, NAME_LARGE, NAME_SIZE_COUNT };

// Each size of the name: Pebble's font, its visible height and the blank
// space it leaves above. The bundled font styles are sized to match (see
// tools/fonts.json).
typedef struct {
  const char *font;
  int8_t height;
  int8_t pad;
} NameSize;

#if PBL_DISPLAY_WIDTH >= 200
static const NameSize NAME_SIZES[NAME_SIZE_COUNT] = {
  { FONT_KEY_GOTHIC_14, 10, 4 }, { FONT_KEY_GOTHIC_18, 13, 5 }, { FONT_KEY_GOTHIC_24, 17, 7 },
};
#else
static const NameSize NAME_SIZES[NAME_SIZE_COUNT] = {
  { FONT_KEY_GOTHIC_09, 7, 2 }, { FONT_KEY_GOTHIC_14, 10, 4 }, { FONT_KEY_GOTHIC_18, 13, 5 },
};
#endif

// What can show above and below the time. Values match the SLOT_OPTIONS in
// src/pkjs/config.js.
typedef enum {
  SLOT_NAME,
  SLOT_DATE,
  SLOT_WEATHER,
  SLOT_STEPS,
  SLOT_HEART_RATE,
  SLOT_BATTERY,
  SLOT_NOTHING,
  SLOT_COUNT,
} Slot;

// Values match the TIME_FORMAT options in src/pkjs/config.js.
enum { TIME_FORMAT_WATCH, TIME_FORMAT_12H, TIME_FORMAT_24H };

// Which sound effects play, each including the ones before. Values match the
// SOUND options in src/pkjs/config.js.
enum { SOUND_OFF, SOUND_AVATARS, SOUND_ALL };

#define VOLUME_MEDIUM 60

#define DEFAULT_LONG_PRESS_TENTHS 10
#define DEFAULT_SELECT_SECONDS 3

// Values match the STYLE options in src/pkjs/config.js.
typedef enum {
  STYLE_DIGITAL,
  STYLE_ANALOG,
} Style;

typedef struct {
  uint8_t version;
  uint8_t style;
  uint8_t glow_argb;
  // Whether flicks (and long presses on touch watches) open the avatars.
  bool animation;
  // Added in version 2: the name in the core, empty to show none, with its
  // size and font style (see font_styles.h).
  char name[24];
  uint8_t name_size;
  uint8_t name_font;
  // Added in version 3: see the TIME_FORMAT values.
  uint8_t time_format;
  // Added in version 4: which sounds play (see SOUND_OFF), and how loud, from
  // 0 to 100.
  uint8_t sound;
  uint8_t sound_volume;
  // Added in version 5: how long a press on the screen opens the selector,
  // in tenths of a second.
  uint8_t long_press_tenths;
  // Added in version 6: how long the selector waits before transforming, in
  // seconds, counted from the last touch or flick.
  uint8_t select_seconds;
  // Added in version 7: whether the backlight stays on from the selector
  // opening until the time comes back.
  bool keep_light;
  // Added in version 8: what shows above and below the time (see Slot).
  uint8_t top_slot;
  uint8_t bottom_slot;
} Settings;

// How much of the settings each version saved, so older settings carry over
// and the fields added since keep their defaults.
static int settings_size(uint8_t version) {
  switch (version) {
    case 1: return offsetof(Settings, name);
    case 2: return offsetof(Settings, time_format);
    case 3: return offsetof(Settings, sound);
    case 4: return offsetof(Settings, long_press_tenths);
    case 5: return offsetof(Settings, select_seconds);
    case 6: return offsetof(Settings, keep_light);
    case 7: return offsetof(Settings, top_slot);
    case SETTINGS_VERSION: return sizeof(Settings);
    default: return -1;
  }
}

static Settings s_settings;

typedef struct {
  GColor background;
  GColor glow;
  GColor dim;      // A darker glow, for fine lines and trails.
  GColor minute;   // The minute's plate and the avatars' shading: dimmer than
                   // the glow, but clear of grey.
  GColor plate;    // Unlit plates.
  GColor edge;     // Plate outlines; only on black and white watches.
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;
static StyledFont s_name_font;

static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[8];
static bool s_daytime = true;

#define PLATE_COUNT 12
#define PLATE_ANGLE (TRIG_MAX_ANGLE / PLATE_COUNT)

// The dial's parts as fractions of its radius, in hundredths.
#define BEZEL_INNER 84   // The plates run from here to the edge.
#define CORE_OUTER 78    // The glowing ring of the core.
#define CORE_INNER 68    // Inside it, the dark centre with the time.

// ---------------------------------------------------------------------------
// States
//
// The face shows the time until a flick or tap opens the selector. Each
// state but the time runs on a timer and returns to the time when it is done.
// Frames come every FRAME_MS while much is moving and every SLOW_FRAME_MS
// while only an avatar is, and the animations follow the clock, so they take
// the same time however fast a watch draws.

typedef enum {
  STATE_TIME,
  STATE_POWER_UP,      // The plates light in turn and the iris opens.
  STATE_CHASE,         // A light runs round the ring on the hour.
  STATE_OPENING,       // The ring spins up to the chosen avatar.
  STATE_SELECTING,     // Stepping through the avatars.
  STATE_TRANSFORMING,  // The flare, iris and shock rings.
  STATE_AVATAR,        // The chosen avatar, large.
  STATE_CLOSING,       // The light runs home and the time comes back.
} State;

#define FRAME_MS 33
#define SLOW_FRAME_MS 66
#define LONGEST_FRAME_MS 120
#define POWER_UP_MS 1100
#define CHASE_MS 1000
#define POP_MS 300
#define OPENING_MS 900
#define STEP_MS 250
#define SNAP_MS 200
#define TRANSFORMING_MS 1000
#define AVATAR_MS 4000
#define CLOSING_MS 700

static State s_state = STATE_TIME;
static int32_t s_state_ms;        // Time spent in the current state.
static int32_t s_idle_ms;         // Time since the last flick or touch.
static AppTimer *s_frame_timer;
static int32_t s_last_frame;      // The clock at the last frame.
static int32_t s_life_ms;         // Keeps the avatars moving between states.
static int32_t s_pop_ms = POP_MS; // Time since a new avatar came up.
static int s_shown = -1;          // The avatar that last came up.
static int s_pips;                // Pips of the selector's charge sounded.
static int s_avatar;              // The chosen avatar, kept between uses.

// The light's turn and its animation from one turn to another. The plates
// stay put: the turn picks which of them is lit, so the light moves clockwise
// round the hours as the turn goes down, and each avatar lights the plate at
// its own hour.
static int32_t s_ring;
static int32_t s_ring_from;
static int32_t s_ring_to;
static int32_t s_ring_ms;
static int32_t s_ring_duration;

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .style = STYLE_DIGITAL,
    .glow_argb = GColorGreenARGB8,
    .animation = true,
    .name = "PEBBLE",
    .name_size = NAME_MEDIUM,
    .name_font = FONT_STYLE_PEBBLE,
    .time_format = TIME_FORMAT_WATCH,
    .sound = SOUND_AVATARS,
    .sound_volume = VOLUME_MEDIUM,
    .long_press_tenths = DEFAULT_LONG_PRESS_TENTHS,
    .select_seconds = DEFAULT_SELECT_SECONDS,
    .keep_light = true,
    .top_slot = SLOT_NAME,
    .bottom_slot = SLOT_DATE,
  };
}

// ---------------------------------------------------------------------------
// Sound
//
// Plays a sound effect in place of any already playing, if the setting
// includes it and it is not Quiet Time, so a flick in the night stays silent.
// The watch's own mute also applies.

#if defined(PBL_SPEAKER)
static void play_sound(const Sound *sound, int level) {
  if (s_settings.sound < level || quiet_time_is_active()) {
    return;
  }
  speaker_stop();
  speaker_play_tracks(sound->tracks, sound->count, s_settings.sound_volume);
}
#define PLAY(sound, level) play_sound(sound, level)
#else
#define PLAY(sound, level)
#endif

static void load_name_font(void) {
  static const CustomFont *const FONTS[NAME_SIZE_COUNT] = {
    FONTS_NAME_SMALL, FONTS_NAME_MEDIUM, FONTS_NAME_LARGE,
  };
  const NameSize *size = &NAME_SIZES[s_settings.name_size];
  styled_font_unload(&s_name_font);
  styled_font_load(&s_name_font, s_settings.name_font, size->font, size->pad,
                   FONTS[s_settings.name_size]);
}

#if defined(PBL_COLOR)
// A colour with each channel halved.
static GColor dimmed(GColor color) {
  return (GColor) { .argb = 0xC0 | ((color.r / 2) << 4) | ((color.g / 2) << 2) | (color.b / 2) };
}
#endif

static void apply_palette(void) {
  s_palette.background = GColorBlack;
#if defined(PBL_COLOR)
  s_palette.glow = (GColor) { .argb = s_settings.glow_argb };
  s_palette.dim = dimmed(s_palette.glow);
  const GColor glow = s_palette.glow;
  s_palette.minute = (GColor) { .argb = 0xC0 | ((glow.r * 2 / 3) << 4) | ((glow.g * 2 / 3) << 2)
                                        | (glow.b * 2 / 3) };
  s_palette.plate = GColorDarkGray;
  s_palette.edge = GColorDarkGray;
#else
  s_palette.glow = GColorWhite;
  s_palette.dim = GColorWhite;
  s_palette.minute = GColorWhite;
  s_palette.plate = GColorBlack;
  s_palette.edge = GColorWhite;
#endif
}

// ---------------------------------------------------------------------------
// Geometry and timing

// The clock in milliseconds, wrapping every few hours.
static int32_t clock_ms(void) {
  time_t seconds;
  uint16_t ms;
  time_ms(&seconds, &ms);
  return (int32_t)(seconds % 10000) * 1000 + ms;
}

// Where something repeating every period is in its cycle, as an angle.
static int32_t cycle(int32_t period) {
  return (s_life_ms % period) * TRIG_MAX_ANGLE / period;
}

static GPoint ray_point(GPoint centre, int32_t angle, int32_t distance) {
  return GPoint(centre.x + sin_lookup(angle) * distance / TRIG_MAX_RATIO,
                centre.y - cos_lookup(angle) * distance / TRIG_MAX_RATIO);
}

// The whole-number square root, rounded down.
static int32_t isqrt(int32_t value) {
  int32_t root = 0;
  while ((root + 1) * (root + 1) <= value) {
    root++;
  }
  return root;
}

// Eases from 0 to 1000 as x goes from 0 to 1000, slowing towards the end.
static int32_t ease_out(int32_t x) {
  const int32_t rest = 1000 - x;
  return 1000 - rest * rest / 1000 * rest / 1000;
}

// A scale in thousandths that springs up from about three quarters, past
// full size and back, as progress goes from 0 to 1000.
static int32_t pop_scale(int32_t progress) {
  const int32_t spring = sin_lookup(TRIG_MAX_ANGLE / 2 * progress / 1000);
  return 720 + 280 * progress / 1000 + spring * 200 / TRIG_MAX_RATIO;
}

// How far through a stretch of the current state it is, from 0 to 1000.
static int32_t stage(int32_t from, int32_t to) {
  if (s_state_ms <= from) {
    return 0;
  }
  if (s_state_ms >= to) {
    return 1000;
  }
  return (s_state_ms - from) * 1000 / (to - from);
}

// How closed the core's iris is, from 0 (open) to 1000 (closed): closing
// between two times and opening again by a third.
static int32_t iris(int32_t close_from, int32_t closed_at, int32_t open_at) {
  if (s_state_ms < closed_at) {
    return stage(close_from, closed_at);
  }
  return 1000 - stage(closed_at, open_at);
}

// The lit plate, whose avatar is the one shown.
static int top_plate(void) {
  // The nearest plate, rounding down for turns either way.
  const int32_t offset = -s_ring + PLATE_ANGLE / 2;
  const int32_t steps = offset >= 0 ? offset / PLATE_ANGLE
                                    : -((-offset + PLATE_ANGLE - 1) / PLATE_ANGLE);
  return ((steps % PLATE_COUNT) + PLATE_COUNT) % PLATE_COUNT;
}

static void ring_animate(int32_t to, int32_t duration) {
  s_ring_from = s_ring;
  s_ring_to = to;
  s_ring_ms = 0;
  s_ring_duration = duration;
}

static bool ring_moving(void) {
  return s_ring_ms < s_ring_duration;
}

// ---------------------------------------------------------------------------
// Drawing

// One armoured plate: a curved block between two radii, with a dark slot
// across its middle.
static void draw_plate(GContext *ctx, GPoint centre, int32_t middle, int16_t inner,
                       int16_t outer, GColor fill) {
  const int32_t half = PLATE_ANGLE / 2 - DEG_TO_TRIGANGLE(3);
  GPoint points[] = {
    ray_point(centre, middle - half, inner),
    ray_point(centre, middle - half, outer),
    ray_point(centre, middle, outer),
    ray_point(centre, middle + half, outer),
    ray_point(centre, middle + half, inner),
    ray_point(centre, middle, inner),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  const bool plain = gcolor_equal(fill, s_palette.plate);
  graphics_context_set_fill_color(ctx, fill);
  gpath_draw_filled(ctx, path);
  graphics_context_set_stroke_color(ctx, plain ? s_palette.edge : fill);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);

  const int16_t depth = (outer - inner) / 3;
  graphics_context_set_stroke_color(ctx, s_palette.background);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, ray_point(centre, middle, inner + depth),
                     ray_point(centre, middle, outer - depth));
}

// Which plates are lit. The hour's plate glows and the minute's glows dimly;
// in the animations a head of light runs round with a trail, plates fill up
// in turn, or they all flash.
typedef struct {
  int lit;
  int minute;
  int head;
  int trail;
  int filled;
  bool flash;
} Lights;

static GColor plate_fill(const Lights *lights, int i) {
  if (i == lights->lit || i == lights->head) {
    return s_palette.glow;
  }
  if (i == lights->trail) {
    return s_palette.dim;
  }
  if (lights->flash || i < lights->filled) {
    return s_palette.minute;
  }
#if defined(PBL_COLOR)
  if (i == lights->minute) {
    return s_palette.minute;
  }
#endif
  return s_palette.plate;
}

// The ring of plates. Black and white watches have no
// dimmer glow, so there only the inner half of the minute's plate is filled.
static void draw_plates(GContext *ctx, GPoint centre, int16_t radius, const Lights *lights) {
  const int16_t inner = radius * BEZEL_INNER / 100;
  for (int i = 0; i < PLATE_COUNT; i++) {
    draw_plate(ctx, centre, PLATE_ANGLE * i, inner, radius, plate_fill(lights, i));
#if !defined(PBL_COLOR)
    if (i == lights->minute && i != lights->lit) {
      draw_plate(ctx, centre, PLATE_ANGLE * i, inner, (inner + radius) / 2,
                 s_palette.minute);
    }
#endif
  }
  graphics_context_set_stroke_color(ctx, s_palette.dim);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, centre, inner - 3);
}

// The core: a glowing ring with four vents cut across it, around a dark
// centre. It swells a little while it flares. In the selector it charges up
// clockwise from 12 o'clock towards the transformation, shown by charge from
// 0 to 1000; otherwise charge is negative.
static void draw_core(GContext *ctx, GPoint centre, int16_t radius, int32_t flare,
                      int32_t charge) {
  const int16_t outer = radius * CORE_OUTER / 100 + 3 * flare / 1000;
  const int16_t inner = radius * CORE_INNER / 100;
  const bool charging = charge >= 0;
  graphics_context_set_fill_color(ctx, charging ? PBL_IF_COLOR_ELSE(s_palette.dim, GColorBlack)
                                                : s_palette.glow);
  graphics_fill_circle(ctx, centre, outer);
  if (charging) {
    graphics_context_set_fill_color(ctx, s_palette.glow);
    graphics_fill_radial(ctx, GRect(centre.x - outer, centre.y - outer, outer * 2, outer * 2),
                         GOvalScaleModeFitCircle, outer - inner + 1, 0,
                         TRIG_MAX_ANGLE * charge / 1000);
  }
#if !defined(PBL_COLOR)
  if (charging) {
    graphics_context_set_stroke_color(ctx, s_palette.glow);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, centre, outer);
  }
#endif
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, centre, inner);

  const int16_t vent = (outer - inner) / 2 + 1;
  graphics_context_set_stroke_color(ctx, s_palette.background);
  graphics_context_set_stroke_width(ctx, vent);
  for (int i = 0; i < 4; i++) {
    const int32_t angle = TRIG_MAX_ANGLE / 8 + TRIG_MAX_ANGLE * i / 4;
    graphics_draw_line(ctx, ray_point(centre, angle, inner - 1),
                       ray_point(centre, angle, outer + 1));
  }
}

// The iris over the core's centre: glow closing in from the edge, leaving an
// opening that shrinks to nothing when fully closed.
static void draw_iris(GContext *ctx, GPoint centre, int16_t inner, int32_t closed) {
  if (closed <= 0) {
    return;
  }
  const int16_t thickness = inner * closed / 1000;
  graphics_context_set_fill_color(ctx, s_palette.glow);
  graphics_fill_radial(ctx, GRect(centre.x - inner, centre.y - inner, inner * 2, inner * 2),
                       GOvalScaleModeFitCircle, thickness, 0, TRIG_MAX_ANGLE);
}

static void draw_text_centred(GContext *ctx, const char *text, GFont font, GPoint centre,
                              int16_t width) {
  const GSize size = graphics_text_layout_get_content_size(
    text, font, GRect(0, 0, width, 100), GTextOverflowModeFill, GTextAlignmentCenter);
  graphics_draw_text(ctx, text, font,
                     GRect(centre.x - width / 2, centre.y - size.h / 2, width, size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// The day and date, e.g. "SAT 31".
static void format_date(const struct tm *t, char *buffer, size_t size) {
  strftime(buffer, size, "%a %d", t);
  for (char *c = buffer; *c; c++) {
    if (*c >= 'a' && *c <= 'z') {
      *c -= 'a' - 'A';
    }
  }
}

// Fonts to suit the size of the core: larger cores, as on the Pebble Round
// 2, take larger digits.
static GFont info_font(int16_t inner) {
  return fonts_get_system_font(inner >= 80 ? FONT_KEY_GOTHIC_24_BOLD
                               : inner >= 58 ? FONT_KEY_GOTHIC_18_BOLD
                               : FONT_KEY_GOTHIC_14_BOLD);
}

// The visible height of the information's text and the blank space its font
// leaves above it.
static void info_metrics(int16_t inner, int16_t *height, int16_t *pad) {
  *height = inner >= 80 ? 17 : inner >= 58 ? 13 : 10;
  *pad = inner >= 80 ? 7 : inner >= 58 ? 5 : 4;
}

// The value a slot shows; its icon says what the value is.
static void slot_text(Slot slot, const struct tm *t, char *out, size_t size) {
  out[0] = '\0';
  switch (slot) {
    case SLOT_NAME:
      strncpy(out, s_settings.name, size - 1);
      out[size - 1] = '\0';
      break;
    case SLOT_DATE:
      format_date(t, out, size);
      break;
    case SLOT_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        snprintf(out, size, "--\xc2\xb0");
      } else {
        snprintf(out, size, "%d\xc2\xb0", (int)s_temperature);
      }
      break;
    case SLOT_STEPS: {
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
    case SLOT_HEART_RATE: {
#if defined(PBL_HEALTH)
      const HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
#else
      const int bpm = 0;
#endif
      if (bpm > 0) {
        snprintf(out, size, "%d", (int)bpm);
      } else {
        snprintf(out, size, "--");
      }
      break;
    }
    default:
      // The battery's icon says it all.
      break;
  }
}

// ---------------------------------------------------------------------------
// Icons
//
// Small icons beside the information, drawn in a square of the given size at
// a point, in percentages of that size so they scale with the text.

typedef enum {
  ICON_NONE,
  ICON_STEPS,
  ICON_HEART,
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

static void icon_heart(GContext *ctx, GPoint at, int16_t size) {
  icon_dot(ctx, at, size, 29, 32, 26);
  icon_dot(ctx, at, size, 71, 32, 26);
  GPoint points[] = {
    GPoint(PC(at.x, 3), PC(at.y, 42)), GPoint(PC(at.x, 97), PC(at.y, 42)),
    GPoint(PC(at.x, 50), PC(at.y, 98)),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
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

static void draw_icon(GContext *ctx, Icon icon, GPoint at, int16_t size, GColor colour) {
  graphics_context_set_fill_color(ctx, colour);
  graphics_context_set_stroke_color(ctx, colour);
  graphics_context_set_stroke_width(ctx, 1);
  switch (icon) {
    case ICON_STEPS: icon_steps(ctx, at, size); break;
    case ICON_HEART: icon_heart(ctx, at, size); break;
    case ICON_BATTERY: icon_battery(ctx, at, size); break;
    case ICON_SUN:
      icon_dot(ctx, at, size, 50, 50, 25);
      for (int i = 0; i < 8; i++) {
        const int32_t angle = TRIG_MAX_ANGLE * i / 8;
        const GPoint middle = GPoint(PC(at.x, 50), PC(at.y, 50));
        graphics_draw_line(ctx, ray_point(middle, angle, size * 36 / 100),
                           ray_point(middle, angle, size * 49 / 100));
      }
      break;
    case ICON_MOON:
      icon_dot(ctx, at, size, 50, 50, 44);
      graphics_context_set_fill_color(ctx, s_palette.background);
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

static Icon slot_icon(Slot slot) {
  switch (slot) {
    case SLOT_WEATHER: return weather_icon();
    case SLOT_STEPS: return ICON_STEPS;
    case SLOT_HEART_RATE: return ICON_HEART;
    case SLOT_BATTERY: return ICON_BATTERY;
    default: return ICON_NONE;
  }
}

// A slot centred on a point: its icon, if it has one, then its value, if it
// has one, cut short if it is too wide for the room. Height and pad describe the text's
// font. The text lives in a static buffer to spare the stack.
static void draw_slot(GContext *ctx, Slot slot, const struct tm *t, GFont font, GPoint middle,
                      int16_t room, int16_t height, int16_t pad, GColor colour) {
  static char text[24];
  slot_text(slot, t, text, sizeof(text));
  const Icon icon = slot_icon(slot);
  if (!text[0] && icon == ICON_NONE) {
    return;
  }
  const int16_t size = height + 6 > 14 ? height + 6 : 14;
  const int16_t gap = icon != ICON_NONE && text[0] ? 3 : 0;
  const int16_t icon_w = icon_width(icon, size);
  const int16_t text_w = graphics_text_layout_get_content_size(
    text, font, GRect(0, 0, 400, 100), GTextOverflowModeFill, GTextAlignmentLeft).w;
  const int16_t room_w = room - icon_w - gap;
  const int16_t shown_w = text_w < room_w ? text_w : room_w;
  const int16_t left = middle.x - (icon_w + gap + shown_w) / 2;
  if (icon != ICON_NONE) {
    draw_icon(ctx, icon, GPoint(left, middle.y - size / 2), size, colour);
  }
  graphics_context_set_text_color(ctx, colour);
  graphics_draw_text(ctx, text, font,
                     GRect(left + icon_w + gap, middle.y - height / 2 - pad, shown_w + 2,
                           height + pad * 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// What shows in the dark centre above the time, in the name's size and font,
// cut short if it is too wide for the circle there.
static void draw_top_slot(GContext *ctx, GPoint centre, int16_t inner, const struct tm *t) {
  const int16_t height = NAME_SIZES[s_settings.name_size].height;
  const int16_t middle = inner * 62 / 100;
  const int16_t top = middle + height / 2;
  const int16_t half = top < inner ? isqrt(inner * inner - top * top) : inner / 2;
  draw_slot(ctx, s_settings.top_slot, t, s_name_font.font,
            GPoint(centre.x, centre.y - middle), half * 2, height, s_name_font.pad,
            s_palette.glow);
}

// What shows below the time, centred on a height.
static void draw_bottom_slot(GContext *ctx, GPoint centre, int16_t inner, int16_t below,
                             const struct tm *t, GColor colour) {
  int16_t height;
  int16_t pad;
  info_metrics(inner, &height, &pad);
  draw_slot(ctx, s_settings.bottom_slot, t, info_font(inner), GPoint(centre.x, centre.y + below),
            inner * 3 / 2, height, pad, colour);
}

static int16_t text_width(const char *text, GFont font, int16_t room) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, room, 100),
                                               GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// The largest digits that leave room for the widest time, plus anything
// beside it, across the core, so the digits stay one size all day. Larger
// cores, as on the Pebble Round 2, take larger digits.
static GFont digits_font(int16_t inner, int16_t beside) {
  static const char *const FONTS[] = {
    FONT_KEY_LECO_42_NUMBERS, FONT_KEY_LECO_38_BOLD_NUMBERS, FONT_KEY_LECO_36_BOLD_NUMBERS,
    FONT_KEY_LECO_32_BOLD_NUMBERS, FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    FONT_KEY_LECO_20_BOLD_NUMBERS,
  };
  GFont font = NULL;
  for (size_t i = inner >= 80 ? 0 : inner >= 58 ? 1 : 4; i < ARRAY_LENGTH(FONTS); i++) {
    font = fonts_get_system_font(FONTS[i]);
    if (text_width("00:00", font, inner * 3) + beside <= inner * 2 - 6) {
      break;
    }
  }
  return font;
}

static bool use_24h(void) {
  switch (s_settings.time_format) {
    case TIME_FORMAT_12H: return false;
    case TIME_FORMAT_24H: return true;
    default: return clock_is_24h_style();
  }
}

// Glowing digits in the dark centre, with the day and date below them. In
// 12-hour time AM or PM sits beside the minutes, and the two are centred
// together.
static void draw_digital(GContext *ctx, GPoint centre, int16_t inner, const struct tm *t) {
  static char time_text[8];
  const bool twenty_four = use_24h();
  if (twenty_four) {
    strftime(time_text, sizeof(time_text), "%H:%M", t);
  } else {
    const int hour = t->tm_hour % 12;
    snprintf(time_text, sizeof(time_text), "%d:%02d", hour ? hour : 12, t->tm_min);
  }

  GFont meridiem_font = fonts_get_system_font(inner >= 80 ? FONT_KEY_GOTHIC_18_BOLD
                                              : inner >= 58 ? FONT_KEY_GOTHIC_14_BOLD
                                              : FONT_KEY_GOTHIC_09);
  const char *meridiem = t->tm_hour < 12 ? "AM" : "PM";
  const int16_t gap = 2;
  const int16_t meridiem_w = twenty_four ? 0 : text_width(meridiem, meridiem_font, inner);
  GFont time_font = digits_font(inner, twenty_four ? 0 : gap + text_width("PM", meridiem_font,
                                                                         inner));
  graphics_context_set_text_color(ctx, s_palette.glow);
  const int16_t time_y = centre.y - inner / 10;
  int16_t time_x = centre.x;
  if (!twenty_four) {
    const int16_t time_w = text_width(time_text, time_font, inner * 2);
    time_x -= (gap + meridiem_w) / 2;
    draw_text_centred(ctx, meridiem, meridiem_font,
                      GPoint(time_x + time_w / 2 + gap + meridiem_w / 2, time_y + inner / 8),
                      meridiem_w + 4);
  }
  draw_text_centred(ctx, time_text, time_font, GPoint(time_x, time_y), inner * 2);
  draw_bottom_slot(ctx, centre, inner, inner * 45 / 100, t, s_palette.glow);
}

// Hands in the dark centre, with the day and date below the middle.
static void draw_analog(GContext *ctx, GPoint centre, int16_t inner, const struct tm *t) {
  const bool large = inner >= 58;
  draw_bottom_slot(ctx, centre, inner, inner * 50 / 100, t, s_palette.dim);

  const int32_t minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  const int32_t hour_angle = TRIG_MAX_ANGLE * ((t->tm_hour % 12) * 60 + t->tm_min) / 720;
  graphics_context_set_stroke_color(ctx, s_palette.glow);
  graphics_context_set_stroke_width(ctx, large ? 7 : 5);
  graphics_draw_line(ctx, centre, ray_point(centre, hour_angle, inner * 55 / 100));
  graphics_context_set_stroke_width(ctx, large ? 4 : 3);
  graphics_draw_line(ctx, centre, ray_point(centre, minute_angle, inner * 88 / 100));
  graphics_context_set_fill_color(ctx, s_palette.glow);
  graphics_fill_circle(ctx, centre, large ? 6 : 4);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_circle(ctx, centre, large ? 2 : 1);
}

// ---------------------------------------------------------------------------
// Avatars
//
// An avatar is a run of shapes on a grid of half units, 0 to 200 each way (see
// tools/make_avatars.py). Each starts with a byte of its motion (high four
// bits), shade (next two) and type (low two); a zero byte ends the run.

enum { SHAPE_END, SHAPE_POLY, SHAPE_CIRCLE, SHAPE_LINE };
enum { SHADE_GLOW, SHADE_DIM, SHADE_CUT };

// How a shape moves while its avatar shows. These match the motions in
// tools/make_avatars.py.
typedef enum {
  MOTION_STILL,
  MOTION_WAVE,     // Ripples, more towards its end, like a tentacle.
  MOTION_FLAP,     // Narrows towards the middle and back, like a wing.
  MOTION_BLINK,    // Hidden for a moment now and then, like an eye.
  MOTION_SWING,    // Rocks about a pivot, like a claw.
  MOTION_SQUASH,   // Widens as it flattens and back, like a jellyfish.
  MOTION_STEP_A,   // Lifts and drops, like a leg; the two steps take turns.
  MOTION_STEP_B,
  MOTION_FLICK,    // Shows for a moment now and then, like a tongue.
} Motion;

#define BLINK_EVERY_MS 2600
#define BLINK_MS 170
#define FLICK_EVERY_MS 1500
#define FLICK_MS 450

// Where a grid point goes in its shape's motion. Along is how far the point
// is through its shape, from 0 to 1000, and number which shape it is, so
// shapes do not all ripple together.
static GPoint moved(int32_t x, int32_t y, Motion motion, int32_t along, int number, int32_t pivot_x,
                    int32_t pivot_y) {
  switch (motion) {
    case MOTION_WAVE: {
      const int32_t angle = cycle(1400) + TRIG_MAX_ANGLE / 1000 * along
        + TRIG_MAX_ANGLE / 5 * number;
      x += sin_lookup(angle) * along / 1000 * 10 / TRIG_MAX_RATIO;
      break;
    }
    case MOTION_FLAP: {
      const int32_t closing = (TRIG_MAX_RATIO - cos_lookup(cycle(800))) / 2;
      x = 100 + (x - 100) * (1000 - closing * 450 / TRIG_MAX_RATIO) / 1000;
      break;
    }
    case MOTION_SWING: {
      // Shapes either side of the middle rock opposite ways.
      const int32_t rock = sin_lookup(cycle(1800)) * DEG_TO_TRIGANGLE(9) / TRIG_MAX_RATIO;
      const int32_t angle = pivot_x < 100 ? -rock : rock;
      const int32_t dx = x - pivot_x;
      const int32_t dy = y - pivot_y;
      x = pivot_x + (dx * cos_lookup(angle) - dy * sin_lookup(angle)) / TRIG_MAX_RATIO;
      y = pivot_y + (dx * sin_lookup(angle) + dy * cos_lookup(angle)) / TRIG_MAX_RATIO;
      break;
    }
    case MOTION_SQUASH: {
      const int32_t squash = sin_lookup(cycle(1600)) * 70 / TRIG_MAX_RATIO;
      x = 100 + (x - 100) * (1000 + squash) / 1000;
      y = 100 + (y - 100) * (1000 - squash) / 1000;
      break;
    }
    case MOTION_STEP_A:
      y += sin_lookup(cycle(900)) * 4 / TRIG_MAX_RATIO;
      break;
    case MOTION_STEP_B:
      y -= sin_lookup(cycle(900)) * 4 / TRIG_MAX_RATIO;
      break;
    default:
      break;
  }
  return GPoint(x, y);
}

// An avatar at a size, centred on a point and bobbing gently: polygons,
// circles and thick lines in the glow or its dimmer shade, and cut out in the
// background colour for eyes and markings. Black and white watches draw the
// dimmer shade as the glow.
static void draw_avatar(GContext *ctx, int avatar, GPoint centre, int16_t size) {
  static GPoint points[AVATAR_MAX_POINTS];
  const uint8_t *data = AVATAR_SHAPES[avatar];
  centre.y += sin_lookup(cycle(2000)) * size / 50 / TRIG_MAX_RATIO;
  const bool blinking = s_life_ms % BLINK_EVERY_MS < BLINK_MS;
  const bool flicking = s_life_ms % FLICK_EVERY_MS < FLICK_MS;
  #define TO_SCREEN(point) GPoint(centre.x + ((point).x - 100) * size / 200, \
                                  centre.y + ((point).y - 100) * size / 200)
  for (int number = 0; *data != SHAPE_END; number++) {
    const uint8_t head = *data++;
    const int type = head & 3;
    const int shade = (head >> 2) & 3;
    const Motion motion = head >> 4;
    const bool hidden = (motion == MOTION_BLINK && blinking)
      || (motion == MOTION_FLICK && !flicking);
    const GColor colour = shade == SHADE_CUT ? s_palette.background
      : shade == SHADE_DIM ? PBL_IF_COLOR_ELSE(s_palette.minute, s_palette.glow)
      : s_palette.glow;
    int32_t pivot_x = 0;
    int32_t pivot_y = 0;
    if (motion == MOTION_SWING) {
      pivot_x = *data++;
      pivot_y = *data++;
    }
    if (type == SHAPE_CIRCLE) {
      if (!hidden) {
        const GPoint at = moved(data[0], data[1], motion, 0, number, pivot_x, pivot_y);
        const int16_t radius = data[2] * size / 200;
        graphics_context_set_fill_color(ctx, colour);
        graphics_fill_circle(ctx, TO_SCREEN(at), radius > 0 ? radius : 1);
      }
      data += 3;
      continue;
    }
    int16_t width = 0;
    if (type == SHAPE_LINE) {
      width = *data++ * size / 200;
      width = width > 0 ? width : 1;
    }
    const int count = *data++;
    if (!hidden) {
      for (int i = 0; i < count; i++) {
        const int32_t along = count > 1 ? i * 1000 / (count - 1) : 0;
        const GPoint at = moved(data[i * 2], data[i * 2 + 1], motion, along, number, pivot_x,
                                pivot_y);
        points[i] = TO_SCREEN(at);
      }
      if (type == SHAPE_LINE) {
        graphics_context_set_stroke_color(ctx, colour);
        graphics_context_set_stroke_width(ctx, width);
        for (int i = 1; i < count; i++) {
          graphics_draw_line(ctx, points[i - 1], points[i]);
        }
      } else {
        GPathInfo info = { .num_points = count, .points = points };
        GPath *path = gpath_create(&info);
        graphics_context_set_fill_color(ctx, colour);
        gpath_draw_filled(ctx, path);
        gpath_destroy(path);
      }
    }
    data += count * 2;
  }
  #undef TO_SCREEN
}

// An avatar in the selector, springing up as it arrives, with its name below.
static void draw_selection(GContext *ctx, GPoint centre, int16_t inner, int avatar) {
  const int32_t pop = pop_scale(s_pop_ms >= POP_MS ? 1000 : s_pop_ms * 1000 / POP_MS);
  draw_avatar(ctx, avatar, GPoint(centre.x, centre.y - inner * 12 / 100),
              inner * 120 / 100 * pop / 1000);
  graphics_context_set_text_color(ctx, s_palette.glow);
  draw_text_centred(ctx, AVATAR_NAMES[avatar], info_font(inner),
                    GPoint(centre.x, centre.y + inner * 66 / 100), inner * 2);
}

// Rings of glow pulsing out from the core to beyond the screen's corners,
// one after another, at a steady speed.
static void draw_shock_rings(GContext *ctx, GPoint centre, int16_t from, int16_t to) {
  static const int16_t starts[] = { 250, 370, 490 };
  graphics_context_set_stroke_color(ctx, s_palette.glow);
  graphics_context_set_stroke_width(ctx, 4);
  for (size_t i = 0; i < ARRAY_LENGTH(starts); i++) {
    const int32_t reach = stage(starts[i], starts[i] + 450);
    if (reach > 0 && reach < 1000) {
      graphics_draw_circle(ctx, centre, from + (to - from) * reach / 1000);
    }
  }
}

// Sparks flying out through the gaps between the plates.
static void draw_sparks(GContext *ctx, GPoint centre, int16_t from, int16_t to) {
  const int32_t reach = ease_out(stage(250, 750));
  if (reach == 0 || reach == 1000) {
    return;
  }
  const int16_t distance = from + (to - from) * reach / 1000;
  graphics_context_set_stroke_color(ctx, s_palette.glow);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < PLATE_COUNT; i++) {
    const int32_t angle = PLATE_ANGLE * i + PLATE_ANGLE / 2;
    graphics_draw_line(ctx, ray_point(centre, angle, distance),
                       ray_point(centre, angle, distance + from / 5));
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const GPoint centre = grect_center_point(&bounds);
  const int16_t short_side = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  const int16_t radius = short_side / 2 - 2;
  const int16_t inner = radius * CORE_INNER / 100;
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // The plates: the hour's and the minute's while showing the time, otherwise
  // the one at the top, with a trail behind it while the ring turns.
  Lights lights = {
    .lit = t->tm_hour % 12, .minute = t->tm_min / 5, .head = -1, .trail = -1,
  };
  if (s_state == STATE_POWER_UP) {
    lights.filled = stage(0, 700) * PLATE_COUNT / 1000;
    lights.head = lights.filled < PLATE_COUNT ? lights.filled : -1;
    if (s_state_ms < 700) {
      lights.lit = lights.minute = -1;
    }
  } else if (s_state == STATE_CHASE) {
    lights.head = stage(0, CHASE_MS) * PLATE_COUNT / 1000;
    lights.trail = (lights.head + PLATE_COUNT - 1) % PLATE_COUNT;
  } else if (s_state != STATE_TIME) {
    lights.minute = -1;
    lights.lit = top_plate();
    if (ring_moving()) {
      const bool clockwise = s_ring_to > s_ring_from;
      lights.trail = (lights.lit + (clockwise ? 1 : PLATE_COUNT - 1)) % PLATE_COUNT;
    }
    lights.flash = s_state == STATE_TRANSFORMING && s_state_ms >= 220 && s_state_ms < 420;
  }

  // What each state shows in the core, and how closed its iris is.
  bool show_time = false;
  bool show_selection = false;
  bool show_avatar = false;
  int32_t closed = 0;
  int32_t charge = -1;
  int32_t pop = 1000;
  switch (s_state) {
    case STATE_TIME:
    case STATE_CHASE:
      show_time = true;
      break;
    case STATE_POWER_UP:
      closed = 1000 - stage(500, 900);
      show_time = true;
      break;
    case STATE_OPENING:
      closed = iris(0, 180, 380);
      show_time = s_state_ms < 180;
      show_selection = !show_time;
      break;
    case STATE_SELECTING:
      show_selection = true;
      const int32_t wait_ms = s_settings.select_seconds * 1000;
      charge = s_idle_ms >= wait_ms ? 1000 : s_idle_ms * 1000 / wait_ms;
      break;
    case STATE_TRANSFORMING:
      closed = iris(0, 250, 650);
      show_selection = s_state_ms < 250;
      show_avatar = !show_selection;
      pop = pop_scale(stage(250, 650));
      break;
    case STATE_AVATAR:
      show_avatar = true;
      break;
    case STATE_CLOSING:
      closed = iris(0, 250, 500);
      show_avatar = s_state_ms < 250;
      show_time = !show_avatar;
      break;
  }

  draw_plates(ctx, centre, radius, &lights);
  draw_core(ctx, centre, radius, closed, charge);
  if (show_time) {
    draw_top_slot(ctx, centre, inner, t);
    if (s_settings.style == STYLE_ANALOG) {
      draw_analog(ctx, centre, inner, t);
    } else {
      draw_digital(ctx, centre, inner, t);
    }
  } else if (show_selection) {
    draw_selection(ctx, centre, inner, top_plate());
  } else if (show_avatar) {
    draw_avatar(ctx, s_avatar, centre, inner * 165 / 100 * pop / 1000);
  }
  draw_iris(ctx, centre, inner, closed);

  if (s_state == STATE_TRANSFORMING) {
    const int16_t corner = (bounds.size.w + bounds.size.h) * 6 / 10;
    draw_shock_rings(ctx, centre, radius * CORE_OUTER / 100, corner);
    draw_sparks(ctx, centre, radius * CORE_OUTER / 100, corner);
  }
}

// ---------------------------------------------------------------------------
// Running the states

static void next_frame(void *context);

// Keeps the backlight on from the selector opening until the time comes back,
// if the setting says so. Letting go hands it back to the watch, which leaves
// it on for its usual few seconds, so the time can be read.
static void update_light(void) {
  static bool s_light_held;
  const bool wanted = s_settings.keep_light && s_state >= STATE_OPENING;
  if (wanted == s_light_held) {
    return;
  }
  s_light_held = wanted;
  light_enable(wanted);
  if (!wanted) {
    light_enable_interaction();
  }
}

static void enter_state(State state) {
  s_state = state;
  s_state_ms = 0;
  s_idle_ms = 0;
  update_light();
  if (state != STATE_TIME && !s_frame_timer) {
    s_last_frame = clock_ms();
    s_frame_timer = app_timer_register(FRAME_MS, next_frame, NULL);
  }
  layer_mark_dirty(s_canvas);
}

// The hour's plate, where the light starts and finishes.
static int hour_plate(void) {
  const time_t now = time(NULL);
  return localtime(&now)->tm_hour % 12;
}

// Opens the selector: the light runs clockwise from the hour's plate, round
// the dial, and comes to rest on the chosen avatar's plate.
static void open_selector(void) {
  s_ring = -hour_plate() * PLATE_ANGLE;
  ring_animate(-2 * TRIG_MAX_ANGLE - s_avatar * PLATE_ANGLE, OPENING_MS);
  enter_state(STATE_OPENING);
  PLAY(&SFX_OPEN, SOUND_AVATARS);
}

// Goes back from the avatar to choosing another.
static void back_to_selecting(void) {
  enter_state(STATE_SELECTING);
  PLAY(&SFX_BACK, SOUND_AVATARS);
}

// Steps to the next avatar by turning the ring back one plate, or to the
// one before by turning it on one.
static void step_avatar(bool forwards) {
  const int32_t from = ring_moving() ? s_ring_to : s_ring;
  ring_animate(from + (forwards ? -PLATE_ANGLE : PLATE_ANGLE), STEP_MS);
  s_idle_ms = 0;
}

static void transform(void) {
  s_avatar = top_plate();
  persist_write_int(PERSIST_KEY_AVATAR, s_avatar);
  enter_state(STATE_TRANSFORMING);
  PLAY(&SFX_TRANSFORM, SOUND_AVATARS);
}

// Closes back to the time, the light running on clockwise to the hour's plate.
static void close_to_time(void) {
  int32_t home = -hour_plate() * PLATE_ANGLE;
  while (home >= s_ring) {
    home -= TRIG_MAX_ANGLE;
  }
  ring_animate(home, CLOSING_MS);
  enter_state(STATE_CLOSING);
  PLAY(&SFX_CLOSE, SOUND_AVATARS);
}

static void next_frame(void *context) {
  s_frame_timer = NULL;
  const int32_t clock = clock_ms();
  int32_t step = clock - s_last_frame;
  step = step < 0 || step > LONGEST_FRAME_MS ? LONGEST_FRAME_MS : step;
  s_last_frame = clock;
  s_state_ms += step;
  s_idle_ms += step;
  s_life_ms += step;
  s_pop_ms += step;
  if (ring_moving()) {
    s_ring_ms += step;
    const int32_t progress = s_ring_ms >= s_ring_duration
      ? 1000 : s_ring_ms * 1000 / s_ring_duration;
    s_ring = s_ring_from + (s_ring_to - s_ring_from) * ease_out(progress) / 1000;
  }

  if (s_state == STATE_OPENING || s_state == STATE_SELECTING) {
    const int top = top_plate();
    if (top != s_shown) {
      s_shown = top;
      s_pop_ms = 0;
      if (s_state == STATE_SELECTING) {
        PLAY(SFX_STEPS[top], SOUND_AVATARS);
      }
    }
  }

  // Two pips rising over the last seconds of the charge, however long it is.
  if (s_state == STATE_SELECTING) {
    const int32_t left = s_settings.select_seconds * 1000 - s_idle_ms;
    const int pips = left <= 1000 ? 2 : left <= 2000 ? 1 : 0;
    if (pips > s_pips && !ring_moving()) {
      PLAY(pips == 1 ? &SFX_PIP_1 : &SFX_PIP_2, SOUND_AVATARS);
    }
    s_pips = pips;
  }

  switch (s_state) {
    case STATE_POWER_UP:
      if (s_state_ms >= POWER_UP_MS) {
        enter_state(STATE_TIME);
      }
      break;
    case STATE_CHASE:
      if (s_state_ms >= CHASE_MS) {
        enter_state(STATE_TIME);
      }
      break;
    case STATE_OPENING:
      if (s_state_ms >= OPENING_MS) {
        enter_state(STATE_SELECTING);
      }
      break;
    case STATE_SELECTING:
      if (s_idle_ms >= s_settings.select_seconds * 1000 && !ring_moving()) {
        transform();
      }
      break;
    case STATE_TRANSFORMING:
      if (s_state_ms >= TRANSFORMING_MS) {
        enter_state(STATE_AVATAR);
        PLAY(SFX_CALLS[s_avatar], SOUND_AVATARS);
      }
      break;
    case STATE_AVATAR:
      if (s_state_ms >= AVATAR_MS) {
        close_to_time();
      }
      break;
    case STATE_CLOSING:
      if (s_state_ms >= CLOSING_MS) {
        s_ring = 0;
        enter_state(STATE_TIME);
      }
      break;
    default:
      break;
  }

  if (s_state != STATE_TIME && !s_frame_timer) {
    const bool calm = s_state == STATE_AVATAR
      || (s_state == STATE_SELECTING && !ring_moving() && s_pop_ms >= POP_MS);
    s_frame_timer = app_timer_register(calm ? SLOW_FRAME_MS : FRAME_MS, next_frame, NULL);
  }
  layer_mark_dirty(s_canvas);
}

static bool touch_in_use(void);

// A flick opens the selector. On watches without a touchscreen it also steps
// through the avatars and goes back to choosing from the avatar; touch does
// those on the others, so flicks don't get in the way of swiping.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_settings.animation) {
    return;
  }
  if (s_state == STATE_TIME) {
    open_selector();
    return;
  }
  if (touch_in_use()) {
    return;
  }
  switch (s_state) {
    case STATE_SELECTING: step_avatar(true); break;
    case STATE_AVATAR: back_to_selecting(); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// Touch, on the Pebble Time 2 and Pebble Round 2
//
// A long press opens the selector. In the selector, a swipe across the core
// steps to the next avatar (left or up) or the one before (right or down), a
// tap on the core picks the avatar, and dragging round the plates turns the
// ring, snapping to the nearest plate when let go. A tap or drag on the avatar
// goes back to choosing.

#if defined(_PBL_API_EXISTS_touch_service_subscribe)
#define DRAG_START_PIXELS 6
// How far a swipe must go to step to another avatar.
#define SWIPE_PIXELS 20
// How far a finger can wander and still be a long press.
#define LONG_PRESS_SLOP_PIXELS 12

static bool s_touch_subscribed;
static AppTimer *s_press_timer;
static bool s_touching;
static bool s_dragging;
static bool s_on_plates;    // The touch started on the plates, not the core.
static GPoint s_touch_start;
static int32_t s_touch_angle;

static GPoint dial_centre(void) {
  const GRect bounds = layer_get_unobstructed_bounds(s_canvas);
  return grect_center_point(&bounds);
}

static int32_t touch_angle(GPoint at) {
  const GPoint centre = dial_centre();
  return atan2_lookup(at.y - centre.y, at.x - centre.x);
}

static bool in_core(GPoint at) {
  const GPoint centre = dial_centre();
  const GRect bounds = layer_get_unobstructed_bounds(s_canvas);
  const int16_t short_side = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  const int16_t inner = (short_side / 2 - 2) * CORE_INNER / 100;
  const int32_t dx = at.x - centre.x;
  const int32_t dy = at.y - centre.y;
  return dx * dx + dy * dy <= inner * inner;
}

static void cancel_long_press(void) {
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
    s_press_timer = NULL;
  }
}

static void long_press_held(void *context) {
  s_press_timer = NULL;
  if (s_state == STATE_TIME && s_settings.animation) {
    open_selector();
  }
}

// While the time shows, the only touch that counts is a long press. It is
// deliberate enough to count on the idle face too, which marks its touches
// non-navigational, or it would not work until the watch was woken. Those
// marks are ignored in the selector too, which the long press has opened.
static void touch_on_time(const TouchEvent *event, GPoint at) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      cancel_long_press();
      s_touch_start = at;
      s_press_timer = app_timer_register(s_settings.long_press_tenths * 100, long_press_held,
                                         NULL);
      break;
    case TouchEvent_PositionUpdate:
      if (abs(at.x - s_touch_start.x) + abs(at.y - s_touch_start.y) > LONG_PRESS_SLOP_PIXELS) {
        cancel_long_press();
      }
      break;
    case TouchEvent_Liftoff:
      cancel_long_press();
      break;
  }
}

static void touch_handler(const TouchEvent *event, void *context) {
  const GPoint at = GPoint(event->x, event->y);
  if (s_state == STATE_TIME) {
    touch_on_time(event, at);
    return;
  }
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touching = true;
      s_dragging = false;
      s_touch_start = at;
      s_touch_angle = touch_angle(at);
      s_on_plates = !in_core(at);
      break;
    case TouchEvent_PositionUpdate: {
      if (!s_touching) {
        break;
      }
      if (!s_dragging && abs(at.x - s_touch_start.x) + abs(at.y - s_touch_start.y)
          >= DRAG_START_PIXELS) {
        s_dragging = true;
        if (s_state == STATE_AVATAR) {
          back_to_selecting();
        }
      }
      if (s_dragging && s_state == STATE_SELECTING) {
        s_idle_ms = 0;
      }
      if (s_dragging && s_state == STATE_SELECTING && s_on_plates) {
        // Move the light by how far the finger has gone round the centre.
        const int32_t angle = touch_angle(at);
        int32_t turn = angle - s_touch_angle;
        if (turn > TRIG_MAX_ANGLE / 2) {
          turn -= TRIG_MAX_ANGLE;
        } else if (turn < -TRIG_MAX_ANGLE / 2) {
          turn += TRIG_MAX_ANGLE;
        }
        s_touch_angle = angle;
        s_ring -= turn;
        s_ring_duration = 0;
        layer_mark_dirty(s_canvas);
      }
      break;
    }
    case TouchEvent_Liftoff:
      if (!s_touching) {
        break;
      }
      s_touching = false;
      if (s_dragging) {
        const int16_t dx = at.x - s_touch_start.x;
        const int16_t dy = at.y - s_touch_start.y;
        const int16_t across = abs(dx) > abs(dy) ? dx : dy;
        if (s_state == STATE_SELECTING && !s_on_plates) {
          if (abs(across) >= SWIPE_PIXELS) {
            step_avatar(across < 0);
          }
        } else if (s_state == STATE_SELECTING) {
          // Snap to the nearest plate.
          const int32_t steps = (s_ring + (s_ring >= 0 ? PLATE_ANGLE / 2 : -PLATE_ANGLE / 2))
            / PLATE_ANGLE;
          ring_animate(steps * PLATE_ANGLE, SNAP_MS);
          s_idle_ms = 0;
        }
        break;
      }
      if (s_state == STATE_SELECTING) {
        if (in_core(at)) {
          transform();
        }
      } else if (s_state == STATE_AVATAR) {
        back_to_selecting();
      }
      break;
  }
}
#endif

// Whether this watch opens the selector by touch.
static bool touch_in_use(void) {
#if defined(_PBL_API_EXISTS_touch_service_subscribe)
  return s_touch_subscribed;
#else
  return false;
#endif
}

// Touch is read only while the avatars are turned on, as the touch sensor
// stays on while anything listens to it.
static void update_touch(void) {
#if defined(_PBL_API_EXISTS_touch_service_subscribe)
  const bool wanted = s_settings.animation && touch_service_is_enabled();
  if (wanted == s_touch_subscribed) {
    return;
  }
  s_touch_subscribed = wanted;
  if (wanted) {
    touch_service_subscribe(touch_handler, NULL);
  } else {
    touch_service_unsubscribe();
  }
#endif
}

// ---------------------------------------------------------------------------
// Events

static bool weather_shown(void) {
  return s_settings.top_slot == SLOT_WEATHER || s_settings.bottom_slot == SLOT_WEATHER;
}

static void request_weather(void) {
  DictionaryIterator *iterator;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_WEATHER, 1);
  app_message_outbox_send();
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (weather_shown() && tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
  if (tick_time->tm_min == 0 && s_state == STATE_TIME && s_settings.animation) {
    enter_state(STATE_CHASE);
    PLAY(&SFX_CHASE, SOUND_ALL);
  }
  layer_mark_dirty(s_canvas);
}

// Clay sends dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE))) {
    s_temperature = tuple->value->int32;
    persist_write_int(PERSIST_KEY_TEMPERATURE, s_temperature);
    layer_mark_dirty(s_canvas);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CONDITION)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_condition, tuple->value->cstring, sizeof(s_condition) - 1);
    s_condition[sizeof(s_condition) - 1] = '\0';
    persist_write_string(PERSIST_KEY_CONDITION, s_condition);
    layer_mark_dirty(s_canvas);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_DAYTIME))) {
    s_daytime = tuple_int(tuple) != 0;
    persist_write_bool(PERSIST_KEY_DAYTIME, s_daytime);
    layer_mark_dirty(s_canvas);
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TOP_SLOT))) {
    s_settings.top_slot = tuple_int(tuple) % SLOT_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BOTTOM_SLOT))) {
    s_settings.bottom_slot = tuple_int(tuple) % SLOT_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_STYLE))) {
    s_settings.style = tuple_int(tuple) == STYLE_ANALOG ? STYLE_ANALOG : STYLE_DIGITAL;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_GLOW_COLOR))) {
    s_settings.glow_argb = GColorFromHEX(tuple_int(tuple)).argb;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_ANIMATION))) {
    s_settings.animation = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NAME)) && tuple->type == TUPLE_CSTRING) {
    strncpy(s_settings.name, tuple->value->cstring, sizeof(s_settings.name) - 1);
    s_settings.name[sizeof(s_settings.name) - 1] = '\0';
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT))) {
    const int32_t format = tuple_int(tuple);
    s_settings.time_format = format == TIME_FORMAT_12H || format == TIME_FORMAT_24H
      ? format : TIME_FORMAT_WATCH;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SOUND))) {
    const int32_t sound = tuple_int(tuple);
    s_settings.sound = sound >= SOUND_OFF && sound <= SOUND_ALL ? sound : SOUND_OFF;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_KEEP_LIGHT))) {
    s_settings.keep_light = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SELECT_TIME))) {
    const int32_t seconds = tuple_int(tuple);
    s_settings.select_seconds = seconds >= 3 && seconds <= 60 ? seconds : DEFAULT_SELECT_SECONDS;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_LONG_PRESS))) {
    const int32_t tenths = tuple_int(tuple);
    s_settings.long_press_tenths = tenths >= 3 && tenths <= 50 ? tenths
                                                               : DEFAULT_LONG_PRESS_TENTHS;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SOUND_VOLUME))) {
    const int32_t volume = tuple_int(tuple);
    s_settings.sound_volume = volume >= 0 && volume <= 100 ? volume : VOLUME_MEDIUM;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NAME_SIZE))) {
    s_settings.name_size = tuple_int(tuple) % NAME_SIZE_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_NAME_FONT))) {
    s_settings.name_font = tuple_int(tuple) % FONT_STYLE_COUNT;
    changed = true;
  }
  if (changed) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    load_name_font();
    update_touch();
    update_light();
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

// Powers up whenever the face comes on screen.
static void window_appear(Window *window) {
  if (s_state == STATE_TIME && s_settings.animation) {
    enter_state(STATE_POWER_UP);
    PLAY(&SFX_POWER, SOUND_ALL);
  }
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
    }
  }
  if (persist_exists(PERSIST_KEY_TEMPERATURE)) {
    s_temperature = persist_read_int(PERSIST_KEY_TEMPERATURE);
  }
  if (persist_exists(PERSIST_KEY_CONDITION)) {
    persist_read_string(PERSIST_KEY_CONDITION, s_condition, sizeof(s_condition));
  }
  if (persist_exists(PERSIST_KEY_DAYTIME)) {
    s_daytime = persist_read_bool(PERSIST_KEY_DAYTIME);
  }
  if (persist_exists(PERSIST_KEY_AVATAR)) {
    s_avatar = persist_read_int(PERSIST_KEY_AVATAR) % AVATAR_COUNT;
  }
  apply_palette();
}

static void init(void) {
  load_settings();
  load_name_font();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_tap_service_subscribe(tap_handler);
  update_touch();
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 32);
}

static void deinit(void) {
  if (s_frame_timer) {
    app_timer_cancel(s_frame_timer);
  }
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
#if defined(_PBL_API_EXISTS_touch_service_subscribe)
  if (s_touch_subscribed) {
    touch_service_unsubscribe();
  }
#endif
#if defined(PBL_SPEAKER)
  speaker_stop();
#endif
  // Hands the backlight back if the face closes while it is held.
  s_state = STATE_TIME;
  update_light();
  window_destroy(s_window);
  styled_font_unload(&s_name_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
