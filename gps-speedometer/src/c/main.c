// GPS Speedometer: your speed, from the phone's GPS, on a colourful dial.
//
// The phone follows its GPS (src/pkjs) and sends the speed about once a
// second. The watch shows it three ways: on a dial, in large digits, and as
// the trip so far. Up and Down, or a swipe, move between them.

#include <pebble.h>

// ---------------------------------------------------------------------------
// Screens

#if PBL_DISPLAY_WIDTH >= 200
#define LARGE 1   // Pebble Time 2 and Pebble Round 2
#else
#define LARGE 0
#endif

// One value for the small screens and another for the large ones.
#define SZ(small, large) (LARGE ? (large) : (small))

// Touch exists on the Pebble Time 2 and Pebble Round 2.
#if defined(_PBL_API_EXISTS_touch_service_subscribe)
#define APP_TOUCH 1
#endif

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// ---------------------------------------------------------------------------
// Units

typedef enum { UNIT_KMH, UNIT_MPH, UNIT_KNOTS, UNIT_COUNT } UnitId;

typedef struct {
  const char *name;        // under the speed
  const char *menu_name;   // in the menu and in messages
  const char *distance;    // the distance that goes with it
  // The speed in tenths of the unit is cm/s x speed_factor / 100,000.
  uint32_t speed_factor;
  // The distance in hundredths of its unit is decimetres x mul / div.
  uint32_t distance_mul;
  uint32_t distance_div;
} Unit;

static const Unit UNITS[UNIT_COUNT] = {
  [UNIT_KMH] = {"KM/H", "km/h", "KM", 36000, 1, 100},
  [UNIT_MPH] = {"MPH", "mph", "MI", 22369, 625, 100584},
  [UNIT_KNOTS] = {"KN", "knots", "NM", 19438, 5, 926},
};

// The speeds the dial can run up to, in the chosen unit, and the spacing of
// its numbered marks. The first, 0, picks one to suit how fast you are going.
static const uint16_t RANGES[] = {0, 10, 20, 40, 60, 80, 120, 160, 200, 240, 320};
static const uint8_t RANGE_STEPS[] = {0, 2, 5, 10, 10, 20, 20, 40, 40, 40, 80};
#define RANGE_COUNT ((int)ARRAY_LENGTH(RANGES))
#define RANGE_AUTO 0

// ---------------------------------------------------------------------------
// Colours

// The dial is lit in bands, from the first colour at rest to the last at the
// top of its range.
#define STOPS 7

#if defined(PBL_COLOR)
typedef struct {
  const char *name;
  uint8_t stops[STOPS];
  uint8_t accent;          // for the unit, captions and the menu
} Theme;

static const Theme THEMES[] = {
  {"Aurora",
   {GColorGreenARGB8, GColorSpringBudARGB8, GColorYellowARGB8, GColorChromeYellowARGB8,
    GColorOrangeARGB8, GColorRedARGB8, GColorFollyARGB8},
   GColorSpringBudARGB8},
  {"Ice",
   {GColorCyanARGB8, GColorVividCeruleanARGB8, GColorBlueMoonARGB8,
    GColorElectricUltramarineARGB8, GColorVividVioletARGB8, GColorMagentaARGB8,
    GColorFashionMagentaARGB8},
   GColorCyanARGB8},
  {"Sunset",
   {GColorIcterineARGB8, GColorYellowARGB8, GColorChromeYellowARGB8, GColorOrangeARGB8,
    GColorSunsetOrangeARGB8, GColorFollyARGB8, GColorFashionMagentaARGB8},
   GColorChromeYellowARGB8},
  {"Neon",
   {GColorMediumSpringGreenARGB8, GColorCyanARGB8, GColorVividCeruleanARGB8,
    GColorBlueMoonARGB8, GColorVividVioletARGB8, GColorMagentaARGB8, GColorFollyARGB8},
   GColorMagentaARGB8},
  {"White",
   {GColorWhiteARGB8, GColorWhiteARGB8, GColorWhiteARGB8, GColorWhiteARGB8,
    GColorWhiteARGB8, GColorWhiteARGB8, GColorWhiteARGB8},
   GColorWhiteARGB8},
};
#define THEME_COUNT ((int)ARRAY_LENGTH(THEMES))
#else
#define THEME_COUNT 1
#endif

#define COLOR_TRACK PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define COLOR_SOFT PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COLOR_NOTICE PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite)
#define COLOR_OVER PBL_IF_COLOR_ELSE(GColorRed, GColorWhite)

// ---------------------------------------------------------------------------
// Settings and the trip, kept between launches

#define STATE_KEY 1
#define STATE_VERSION 1

typedef enum { VIEW_DIAL, VIEW_DIGITS, VIEW_TRIP, VIEW_COUNT } ViewId;

typedef struct {
  uint8_t version;
  uint8_t unit;
  uint8_t range;           // an index into RANGES
  uint8_t theme;
  uint8_t view;
  bool alert_on;
  bool light_on;           // keep the screen lit while the app is open
  uint8_t launches;        // counts the first few, to show the touch hint
  uint16_t alert_speed;    // in the chosen unit
  uint32_t trip_dm;        // distance, in decimetres
  uint32_t trip_seconds;   // time spent moving
  uint32_t max_cms;        // top speed, in cm/s
  uint32_t peak_cms;       // recent top speed, for the automatic dial range
} State;

static State s_state;
static bool s_state_dirty;

static void state_defaults(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.version = STATE_VERSION;
  // Miles for a watch set to American English, kilometres elsewhere.
  s_state.unit = strcmp(i18n_get_system_locale(), "en_US") == 0 ? UNIT_MPH : UNIT_KMH;
  s_state.range = RANGE_AUTO;
  s_state.alert_speed = 50;
  // Starts the automatic dial at 0 to 60.
  s_state.peak_cms = 1400;
}

static void state_load(void) {
  state_defaults();
  State saved;
  if (persist_read_data(STATE_KEY, &saved, sizeof(saved)) == (int)sizeof(saved) &&
      saved.version == STATE_VERSION) {
    s_state = saved;
  }
  if (s_state.unit >= UNIT_COUNT) {
    s_state.unit = UNIT_KMH;
  }
  if (s_state.range >= RANGE_COUNT) {
    s_state.range = RANGE_AUTO;
  }
  if (s_state.theme >= THEME_COUNT) {
    s_state.theme = 0;
  }
  if (s_state.view >= VIEW_COUNT) {
    s_state.view = VIEW_DIAL;
  }
  if (s_state.alert_speed < 5 || s_state.alert_speed > 995) {
    s_state.alert_speed = 50;
  }
}

static void state_save(void) {
  persist_write_data(STATE_KEY, &s_state, sizeof(s_state));
  s_state_dirty = false;
}

// ---------------------------------------------------------------------------
// What the phone last said

// The STATUS key of each message (see src/pkjs/index.js), and LINK_STARTING
// until the first message arrives.
typedef enum {
  LINK_FIX,
  LINK_SEARCHING,
  LINK_DENIED,
  LINK_ERROR,
  LINK_STARTING,
} LinkStatus;

// A fix older than this many seconds is no longer shown.
#define FIX_TIMEOUT 5

static LinkStatus s_link = LINK_STARTING;
static bool s_connected;
static int32_t s_speed_cms;
static int16_t s_heading = -1;       // degrees, or -1 when not known
static int16_t s_accuracy = -1;      // metres, or -1 when not known
static time_t s_fix_time;
// The phone counts the distance travelled since it started; the trip takes
// the difference between one message and the next.
static uint32_t s_odometer;
static bool s_have_odometer;
static bool s_over;                  // going faster than the alert speed

static bool has_fix(void) {
  return s_connected && s_link == LINK_FIX && time(NULL) - s_fix_time < FIX_TIMEOUT;
}

static const Unit *unit(void) {
  return &UNITS[s_state.unit];
}

// A speed in tenths of the chosen unit.
static int32_t speed_x10(uint32_t cms) {
  if (cms > 50000) {
    cms = 50000;
  }
  return (int32_t)(cms * unit()->speed_factor / 100000);
}

// A distance in hundredths of the chosen unit.
static uint32_t distance_x100(uint32_t dm) {
  return (uint32_t)((uint64_t)dm * unit()->distance_mul / unit()->distance_div);
}

// The top of the dial, in the chosen unit.
static int dial_range(void) {
  if (RANGES[s_state.range]) {
    return RANGES[s_state.range];
  }
  // Automatic: the smallest that holds the recent top speed with a tenth spare.
  const int32_t peak = speed_x10(s_state.peak_cms);
  for (int i = 1; i < RANGE_COUNT; i++) {
    if (RANGES[i] * 10 >= peak + peak / 10) {
      return RANGES[i];
    }
  }
  return RANGES[RANGE_COUNT - 1];
}

static int dial_step(int range) {
  for (int i = 1; i < RANGE_COUNT; i++) {
    if (RANGES[i] == range) {
      return RANGE_STEPS[i];
    }
  }
  return range / 4;
}

// ---------------------------------------------------------------------------
// Fonts

// A font for the speed. Its digits are given equal widths, so the number
// does not jiggle as it changes.
typedef struct {
  GFont font;
  int16_t size;      // the font's size: its baseline is this far below the box's top
  int16_t height;    // of a digit
  int16_t cell;      // the width given to each digit
  int16_t dot;       // the width given to the decimal point
} NumberFont;

static NumberFont s_dial_digits;   // on the dial
static NumberFont s_large_digits;  // on the large-digits view
static GFont s_value_font;         // values and units
static GFont s_caption_font;       // small labels
static GFont s_text_font;          // messages

// The value font, and how far its capitals are from the top of its box.
#define VALUE_SIZE SZ(22, 28)
#define VALUE_CAP SZ(19, 24)
// Likewise for the two system fonts.
#define CAPTION_TOP SZ(5, 7)
#define CAPTION_CAP SZ(9, 11)
#define TEXT_TOP SZ(7, 9)
#define TEXT_LINE SZ(18, 24)

static void fonts_load(void) {
#if defined(PBL_PLATFORM_EMERY)
  s_dial_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_70)),
                               70, 61, 32, 18};
  s_large_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_100)),
                                100, 87, 45, 26};
#elif defined(PBL_PLATFORM_GABBRO)
  s_dial_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_88)),
                               88, 76, 40, 23};
  s_large_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_100)),
                                100, 87, 45, 26};
#elif defined(PBL_PLATFORM_CHALK)
  s_dial_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_56)),
                               56, 50, 26, 15};
  s_large_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_70)),
                                70, 61, 32, 18};
#else
  s_dial_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_46)),
                               46, 40, 21, 12};
  s_large_digits = (NumberFont){fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITS_70)),
                                70, 61, 32, 18};
#endif
#if LARGE
  s_value_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_VALUE_28));
#else
  s_value_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_VALUE_22));
#endif
  s_caption_font = fonts_get_system_font(SZ(FONT_KEY_GOTHIC_14_BOLD, FONT_KEY_GOTHIC_18_BOLD));
  s_text_font = fonts_get_system_font(SZ(FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_24_BOLD));
}

static void fonts_unload(void) {
  fonts_unload_custom_font(s_dial_digits.font);
  fonts_unload_custom_font(s_large_digits.font);
  fonts_unload_custom_font(s_value_font);
}

// ---------------------------------------------------------------------------
// Drawing: the pieces

static Window *s_window;
static Layer *s_canvas;
static GPath *s_arrow;
static char s_toast[28];
static AppTimer *s_toast_timer;
// Where a tap changes the units: over the speed and its unit.
static GRect s_units_zone;
// Where the dial's needle is. It glides towards the speed between fixes.
static int32_t s_shown_cms;
static AppTimer *s_glide_timer;

static void redraw(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

static GColor stop_color(int stop) {
#if defined(PBL_COLOR)
  return (GColor){.argb = THEMES[s_state.theme].stops[stop]};
#else
  return GColorWhite;
#endif
}

static GColor accent_color(void) {
#if defined(PBL_COLOR)
  return (GColor){.argb = THEMES[s_state.theme].accent};
#else
  return GColorWhite;
#endif
}

static void draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                      GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, align, NULL);
}

static int text_width(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 400, 60),
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft).w;
}

// Text in the value font, with the top of its capitals at `top`.
static void draw_value(GContext *ctx, const char *text, int x, int top, int width,
                       GTextAlignment align, GColor color) {
  draw_text(ctx, text, s_value_font,
            GRect(x, top - (VALUE_SIZE - VALUE_CAP), width, VALUE_SIZE + 10), align, color);
}

// Text in the caption font, with the top of its capitals at `top`.
static void draw_caption(GContext *ctx, const char *text, int x, int top, int width,
                         GTextAlignment align, GColor color) {
  draw_text(ctx, text, s_caption_font, GRect(x, top - CAPTION_TOP, width, CAPTION_CAP + 12),
            align, color);
}

static int number_width(const NumberFont *font, const char *text) {
  int width = 0;
  for (const char *c = text; *c; c++) {
    width += *c == '.' ? font->dot : font->cell;
  }
  return width;
}

// A number centred on a point, each digit in a cell of the same width.
static void draw_number(GContext *ctx, const NumberFont *font, const char *text,
                        GPoint centre, GColor color) {
  int x = centre.x - number_width(font, text) / 2;
  const int box_top = centre.y - font->height / 2 - (font->size - font->height);
  graphics_context_set_text_color(ctx, color);
  for (const char *c = text; *c; c++) {
    const int width = *c == '.' ? font->dot : font->cell;
    const char glyph[2] = {*c, '\0'};
    graphics_draw_text(ctx, glyph, font->font,
                       GRect(x - 10, box_top, width + 20, font->size + font->size / 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    x += width;
  }
}

// Two dashes where the speed would be, while there is none to show.
static void draw_no_number(GContext *ctx, const NumberFont *font, GPoint centre) {
  const int width = font->cell * 3 / 4;
  const int height = font->height / 9 + 2;
  const int gap = font->cell / 4;
  graphics_context_set_fill_color(ctx, COLOR_TRACK);
  graphics_fill_rect(ctx, GRect(centre.x - gap / 2 - width, centre.y - height / 2, width, height),
                     height / 2, GCornersAll);
  graphics_fill_rect(ctx, GRect(centre.x + gap - gap / 2, centre.y - height / 2, width, height),
                     height / 2, GCornersAll);
}

// The dial sweeps three quarters of a circle, open at the bottom.
#define DIAL_FROM DEG_TO_TRIGANGLE(-135)
#define DIAL_SWEEP DEG_TO_TRIGANGLE(270)

static int32_t dial_angle(int32_t value, int32_t top) {
  if (value < 0) {
    value = 0;
  }
  if (value > top) {
    value = top;
  }
  return DIAL_FROM + (int32_t)((int64_t)DIAL_SWEEP * value / top);
}

// The point at a distance from a centre, at an angle clockwise from the top.
static GPoint polar(GPoint centre, int radius, int32_t angle) {
  return GPoint(centre.x + sin_lookup(angle) * radius / TRIG_MAX_RATIO,
                centre.y - cos_lookup(angle) * radius / TRIG_MAX_RATIO);
}

typedef struct {
  GPoint centre;
  int16_t radius;     // to the outside of the band
  int16_t band;       // how thick the band is
  bool marks;         // marks and, on the large screens, numbers inside it
} Dial;

static void draw_dial(GContext *ctx, const Dial *dial, bool live) {
  const GPoint c = dial->centre;
  const int r = dial->radius;
  const int band = dial->band;
  const int middle = r - band / 2;
  const GRect box = GRect(c.x - r, c.y - r, 2 * r, 2 * r);
  const int range = dial_range();
  const int32_t top = range * 10;
  const int32_t value = live ? speed_x10(s_shown_cms) : 0;

  // The track.
#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, COLOR_TRACK);
  graphics_fill_radial(ctx, box, GOvalScaleModeFitCircle, band, DIAL_FROM,
                       DIAL_FROM + DIAL_SWEEP);
  graphics_fill_circle(ctx, polar(c, middle, DIAL_FROM), band / 2 - 1);
  graphics_fill_circle(ctx, polar(c, middle, DIAL_FROM + DIAL_SWEEP), band / 2 - 1);
#else
  // Black and white has no grey, so the track is a thin line.
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_arc(ctx, grect_inset(box, GEdgeInsets(band / 2)), GOvalScaleModeFitCircle,
                    DIAL_FROM, DIAL_FROM + DIAL_SWEEP);
#endif

  // The lit part. Each colour is drawn from the start, the furthest first,
  // so the nearer ones cover it and the joins stay clean.
  const int32_t end = dial_angle(value, top);
  int lit_stop = 0;
  if (value > 0) {
    lit_stop = (int)((int64_t)STOPS * value / top);
    if (lit_stop >= STOPS) {
      lit_stop = STOPS - 1;
    }
#if defined(PBL_COLOR)
    for (int i = lit_stop; i >= 0; i--) {
      const int32_t stop_end = DIAL_FROM + DIAL_SWEEP * (i + 1) / STOPS;
      graphics_context_set_fill_color(ctx, stop_color(i));
      graphics_fill_radial(ctx, box, GOvalScaleModeFitCircle, band, DIAL_FROM,
                           MIN(end, stop_end));
    }
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_radial(ctx, box, GOvalScaleModeFitCircle, band, DIAL_FROM, end);
#endif
    graphics_context_set_fill_color(ctx, stop_color(0));
    graphics_fill_circle(ctx, polar(c, middle, DIAL_FROM), band / 2 - 1);
  }

  // Marks, and numbers by the larger ones.
  if (dial->marks) {
    const int step = dial_step(range);
    const int count = range / step;
    const int outer = r - band - SZ(3, 4);
    graphics_context_set_stroke_width(ctx, SZ(1, 2));
    for (int i = 0; i <= count * 2; i++) {
      const int32_t angle = dial_angle(i, count * 2);
      const bool major = i % 2 == 0;
#if !defined(PBL_COLOR)
      if (!major) {
        continue;
      }
#endif
      graphics_context_set_stroke_color(ctx, major ? COLOR_SOFT : COLOR_TRACK);
      graphics_draw_line(ctx, polar(c, outer, angle),
                         polar(c, outer - (major ? SZ(4, 6) : SZ(2, 3)), angle));
#if LARGE
      if (major) {
        char label[6];
        snprintf(label, sizeof(label), "%d", i / 2 * step);
        const GPoint at = polar(c, outer - 18, angle);
        draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                  GRect(at.x - 20, at.y - 10, 40, 16), GTextAlignmentCenter, COLOR_SOFT);
      }
#endif
    }
  }

  // A notch across the band at the alert speed.
  if (s_state.alert_on && s_state.alert_speed <= range) {
    const int32_t angle = dial_angle(s_state.alert_speed * 10, top);
    const GPoint from = polar(c, r, angle);
    const GPoint to = polar(c, r - band, angle);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, SZ(5, 7));
    graphics_draw_line(ctx, from, to);
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_context_set_stroke_width(ctx, SZ(1, 3));
    graphics_draw_line(ctx, from, to);
  }

  // The knob at the end of the lit part.
  if (live) {
    const GPoint knob = polar(c, middle, end);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, knob, band / 2 + 3);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, knob, band / 2 + 1);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(stop_color(lit_stop), GColorBlack));
    graphics_fill_circle(ctx, knob, band / 2 - 3);
  }
}

// How good the fix is, from 0 (none) to 4, by how many metres it may be out.
static int signal_level(void) {
  if (!has_fix()) {
    return 0;
  }
  if (s_accuracy < 0 || s_accuracy <= 6) {
    return 4;
  }
  return s_accuracy <= 12 ? 3 : s_accuracy <= 25 ? 2 : 1;
}

#define BARS_WIDTH SZ(15, 22)
#define BARS_HEIGHT SZ(10, 15)

// Four rising bars for the GPS signal, with their top left corner at `at`.
static void draw_signal(GContext *ctx, GPoint at) {
  const int level = signal_level();
  const int width = SZ(3, 4);
  const int gap = SZ(1, 2);
  const int unit_height = SZ(2, 3);
#if defined(PBL_COLOR)
  const GColor lit = level >= 3 ? GColorGreen : level == 2 ? GColorYellow : GColorOrange;
#else
  const GColor lit = GColorWhite;
#endif
  for (int i = 0; i < 4; i++) {
    const int height = (i + 2) * unit_height;
    const int x = at.x + i * (width + gap);
    if (i < level) {
      graphics_context_set_fill_color(ctx, lit);
      graphics_fill_rect(ctx, GRect(x, at.y + BARS_HEIGHT - height, width, height), 0, GCornerNone);
    } else {
#if defined(PBL_COLOR)
      graphics_context_set_fill_color(ctx, COLOR_TRACK);
      graphics_fill_rect(ctx, GRect(x, at.y + BARS_HEIGHT - height, width, height), 0, GCornerNone);
#else
      // Unlit bars are just their feet.
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(x, at.y + BARS_HEIGHT - 1, width, 1), 0, GCornerNone);
#endif
    }
  }
}

static void clock_text(char *out, size_t size) {
  time_t now = time(NULL);
  strftime(out, size, clock_is_24h_style() ? "%H:%M" : "%I:%M", localtime(&now));
  // 12-hour time without its leading zero.
  if (!clock_is_24h_style() && out[0] == '0') {
    memmove(out, out + 1, strlen(out));
  }
}

// The signal and the time. On the rectangular screens they sit in the top
// corners; on the round ones, side by side around `centre`.
static void draw_status(GContext *ctx, GRect bounds, GPoint centre) {
  char clock[8];
  clock_text(clock, sizeof(clock));
#if defined(PBL_ROUND)
  const int gap = SZ(6, 8);
  const int width = BARS_WIDTH + gap + text_width(clock, s_caption_font);
  const int left = centre.x - width / 2;
  draw_signal(ctx, GPoint(left, centre.y - BARS_HEIGHT / 2));
  draw_caption(ctx, clock, left + BARS_WIDTH + gap, centre.y - CAPTION_CAP / 2, 80,
               GTextAlignmentLeft, GColorWhite);
#else
  draw_signal(ctx, GPoint(SZ(5, 7), SZ(3, 4)));
  draw_caption(ctx, clock, bounds.size.w - 70 - SZ(4, 6), SZ(4, 6), 70, GTextAlignmentRight,
               GColorWhite);
#endif
}

// Why there is no speed to show, or NULL when there is.
static const char *notice_text(void) {
  if (!s_connected) {
    return "Phone not connected";
  }
  switch (s_link) {
    case LINK_STARTING:
      return "Starting GPS";
    case LINK_SEARCHING:
      return "Finding GPS";
    case LINK_DENIED:
      return "Location is off";
    case LINK_ERROR:
      return "No GPS signal";
    case LINK_FIX:
      break;
  }
  return has_fix() ? NULL : "GPS signal lost";
}

static void speed_text(char *out, size_t size) {
  const int32_t x10 = speed_x10(s_speed_cms);
  // Tenths help at walking and sailing speeds, on the slower dials.
  if (x10 < 100 && dial_range() <= 20) {
    snprintf(out, size, "%d.%d", (int)(x10 / 10), (int)(x10 % 10));
  } else {
    snprintf(out, size, "%d", (int)((x10 + 5) / 10));
  }
}

// The speed with its unit below, or dashes and the reason there is none.
// `width` is the room for the reason's text.
static void draw_speed(GContext *ctx, const NumberFont *font, GPoint centre, int unit_top,
                       int width) {
  const char *notice = notice_text();
  const int digits_top = centre.y - font->height / 2;
  s_units_zone = GRect(centre.x - font->cell * 2, digits_top - 4, font->cell * 4,
                       unit_top + VALUE_CAP + 8 - digits_top);
  if (notice) {
    draw_no_number(ctx, font, centre);
    draw_text(ctx, notice, s_text_font,
              GRect(centre.x - width / 2, unit_top - TEXT_TOP, width, 2 * TEXT_LINE + 8),
              GTextAlignmentCenter, COLOR_NOTICE);
    return;
  }

  char number[8];
  speed_text(number, sizeof(number));
  draw_number(ctx, font, number, centre, s_over ? COLOR_OVER : GColorWhite);

  const char *name = unit()->name;
  if (s_over) {
    // Over the alert speed: the unit stands out on a filled plate.
    const int plate = text_width(name, s_value_font) + SZ(12, 16);
    graphics_context_set_fill_color(ctx, COLOR_OVER);
    graphics_fill_rect(ctx, GRect(centre.x - plate / 2, unit_top - SZ(3, 4), plate,
                                  VALUE_CAP + SZ(6, 8)),
                       SZ(3, 4), GCornersAll);
    draw_value(ctx, name, centre.x - 60, unit_top, 120, GTextAlignmentCenter,
               PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  } else {
    draw_value(ctx, name, centre.x - 60, unit_top, 120, GTextAlignmentCenter, accent_color());
  }
}

static const char *const CARDINALS[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

static const GPoint ARROW_POINTS[] = {
  {0, -SZ(7, 9)}, {SZ(5, 7), SZ(6, 8)}, {0, SZ(3, 4)}, {-SZ(5, 7), SZ(6, 8)},
};
static const GPathInfo ARROW_INFO = {
  .num_points = ARRAY_LENGTH(ARROW_POINTS),
  .points = (GPoint *)ARROW_POINTS,
};

// An arrow pointing the way you are heading, with north at the top.
static void draw_arrow(GContext *ctx, GPoint centre) {
  if (s_heading < 0 || !has_fix()) {
    graphics_context_set_fill_color(ctx, COLOR_TRACK);
    graphics_fill_circle(ctx, centre, SZ(2, 3));
    return;
  }
  gpath_rotate_to(s_arrow, DEG_TO_TRIGANGLE(s_heading));
  gpath_move_to(s_arrow, centre);
  graphics_context_set_fill_color(ctx, accent_color());
  gpath_draw_filled(ctx, s_arrow);
}

static const char *heading_name(void) {
  if (s_heading < 0 || !has_fix()) {
    return "--";
  }
  return CARDINALS[((s_heading * 2 + 45) / 90) % 8];
}

// A distance to three figures: 0.42, 9.87, 12.4, 123.
static void short_distance_text(char *out, size_t size, uint32_t dm) {
  const uint32_t x100 = distance_x100(dm);
  if (x100 < 1000) {
    snprintf(out, size, "%lu.%02lu", (unsigned long)(x100 / 100), (unsigned long)(x100 % 100));
  } else if (x100 < 10000) {
    snprintf(out, size, "%lu.%lu", (unsigned long)(x100 / 100),
             (unsigned long)(x100 % 100 / 10));
  } else {
    snprintf(out, size, "%lu", (unsigned long)(x100 / 100));
  }
}

static void draw_page_dots(GContext *ctx, GPoint centre) {
  const int spacing = SZ(8, 10);
  for (int i = 0; i < VIEW_COUNT; i++) {
    const GPoint at = GPoint(centre.x + (i - 1) * spacing, centre.y);
    if (i == s_state.view) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, at, SZ(2, 3));
    } else {
      graphics_context_set_fill_color(ctx, COLOR_TRACK);
      graphics_fill_circle(ctx, at, PBL_IF_COLOR_ELSE(SZ(2, 3), 1));
    }
  }
}

// A short message on a plate, centred on a point.
static void draw_toast(GContext *ctx, GPoint centre) {
  if (!s_toast[0]) {
    return;
  }
  const int width = text_width(s_toast, s_caption_font) + SZ(16, 22);
  const int height = CAPTION_CAP + SZ(12, 16);
  const GRect plate = GRect(centre.x - width / 2, centre.y - height / 2, width, height);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, grect_inset(plate, GEdgeInsets(-3)), height / 2 + 3, GCornersAll);
  graphics_context_set_fill_color(ctx, accent_color());
  graphics_fill_rect(ctx, plate, height / 2, GCornersAll);
  draw_caption(ctx, s_toast, plate.origin.x, centre.y - CAPTION_CAP / 2, width,
               GTextAlignmentCenter, GColorBlack);
}

// ---------------------------------------------------------------------------
// Drawing: the views

#if defined(PBL_ROUND)

// Round screens: the dial runs around the edge, with everything inside it.

static void draw_round_extras(GContext *ctx, GPoint c, int heading_y, bool trip) {
  // The heading: its arrow and name, side by side.
  const char *name = heading_name();
  const int gap = SZ(6, 8);
  const int arrow = SZ(12, 16);
  const int width = arrow + gap + text_width(name, s_caption_font);
  draw_arrow(ctx, GPoint(c.x - width / 2 + arrow / 2, heading_y));
  draw_caption(ctx, name, c.x - width / 2 + arrow + gap, heading_y - CAPTION_CAP / 2, 60,
               GTextAlignmentLeft, GColorWhite);
  if (trip) {
    char distance[20];
    short_distance_text(distance, sizeof(distance), s_state.trip_dm);
    const size_t used = strlen(distance);
    snprintf(distance + used, sizeof(distance) - used, " %s", unit()->distance);
    draw_caption(ctx, distance, c.x - 60, heading_y + SZ(12, 17), 120, GTextAlignmentCenter,
                 COLOR_SOFT);
  }
}

static void draw_dial_view(GContext *ctx, GRect bounds) {
  const GPoint c = grect_center_point(&bounds);
  const Dial dial = {c, bounds.size.w / 2 - SZ(3, 4), SZ(10, 14), true};
  draw_dial(ctx, &dial, has_fix());
  draw_status(ctx, bounds, GPoint(c.x, c.y - SZ(45, 66)));
  draw_speed(ctx, &s_dial_digits, GPoint(c.x, c.y - SZ(2, 2)), c.y + SZ(30, 44), SZ(110, 160));
  if (!notice_text()) {
    draw_round_extras(ctx, c, c.y + SZ(56, 82), true);
  }
}

static void draw_digits_view(GContext *ctx, GRect bounds) {
  const GPoint c = grect_center_point(&bounds);
  const Dial dial = {c, bounds.size.w / 2 - SZ(3, 4), SZ(5, 7), false};
  draw_dial(ctx, &dial, has_fix());
  draw_status(ctx, bounds, GPoint(c.x, c.y - SZ(52, 74)));
  draw_speed(ctx, &s_large_digits, GPoint(c.x, c.y - SZ(4, 6)), c.y + SZ(33, 46), SZ(120, 170));
  if (!notice_text()) {
    draw_round_extras(ctx, c, c.y + SZ(64, 90), false);
  }
}

#else

// Rectangular screens: the signal and time across the top, and the trip,
// heading and top speed across the bottom.

static void draw_footer(GContext *ctx, GRect bounds) {
  const int w = bounds.size.w;
  const int h = bounds.size.h;
  const int value_top = h - SZ(40, 48);
  const int caption_top = value_top + VALUE_CAP + SZ(3, 4);
  const int side = SZ(4, 8);
  const int column = SZ(52, 70);

  char distance[12];
  short_distance_text(distance, sizeof(distance), s_state.trip_dm);
  draw_value(ctx, distance, side, value_top, column, GTextAlignmentLeft, GColorWhite);
  draw_caption(ctx, unit()->distance, side, caption_top, column, GTextAlignmentLeft,
               accent_color());

  draw_arrow(ctx, GPoint(w / 2, value_top + VALUE_CAP / 2));
  draw_caption(ctx, heading_name(), w / 2 - 30, caption_top, 60, GTextAlignmentCenter,
               GColorWhite);

  char top_speed[8];
  snprintf(top_speed, sizeof(top_speed), "%d", (int)((speed_x10(s_state.max_cms) + 5) / 10));
  draw_value(ctx, top_speed, w - side - column, value_top, column, GTextAlignmentRight,
             GColorWhite);
  draw_caption(ctx, "MAX", w - side - column, caption_top, column, GTextAlignmentRight,
               accent_color());
}

static void draw_dial_view(GContext *ctx, GRect bounds) {
  const int w = bounds.size.w;
  const int r = SZ(60, 86);
  const GPoint c = GPoint(w / 2, SZ(17, 22) + r);
  const Dial dial = {c, r, SZ(10, 14), true};
  draw_status(ctx, bounds, GPointZero);
  draw_dial(ctx, &dial, has_fix());
  draw_speed(ctx, &s_dial_digits, GPoint(c.x, c.y - SZ(4, 4)), c.y + SZ(22, 34), SZ(92, 130));
  draw_footer(ctx, bounds);
}

// A row of blocks that light up with the speed, like the dial laid flat.
static void draw_bar(GContext *ctx, GRect box, bool live) {
  const int blocks = SZ(16, 20);
  const int gap = 2;
  const int width = (box.size.w + gap) / blocks - gap;
  const int left = box.origin.x + (box.size.w - (blocks * (width + gap) - gap)) / 2;
  const int32_t top = dial_range() * 10;
  int32_t value = live ? speed_x10(s_shown_cms) : 0;
  if (value > top) {
    value = top;
  }
  // The first block lights as soon as you move.
  const int lit = value > 0 ? (int)(value * (blocks - 1) / top) + 1 : 0;
  for (int i = 0; i < blocks; i++) {
    const GRect block = GRect(left + i * (width + gap), box.origin.y, width, box.size.h);
    if (i < lit) {
      graphics_context_set_fill_color(ctx, stop_color(i * STOPS / blocks));
      graphics_fill_rect(ctx, block, 1, GCornersAll);
    } else {
#if defined(PBL_COLOR)
      graphics_context_set_fill_color(ctx, COLOR_TRACK);
      graphics_fill_rect(ctx, block, 1, GCornersAll);
#else
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(block.origin.x, block.origin.y + box.size.h - 1, width, 1), 0,
                         GCornerNone);
#endif
    }
  }
  // The alert speed is a gap in the row with a mark above it.
  if (s_state.alert_on && s_state.alert_speed * 10 <= top) {
    const int x = left + (int)((int64_t)(blocks * (width + gap) - gap) * s_state.alert_speed * 10 /
                               top);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
    graphics_fill_rect(ctx, GRect(x - 1, box.origin.y - 3, 3, box.size.h + 6), 0, GCornerNone);
  }
}

static void draw_digits_view(GContext *ctx, GRect bounds) {
  const int w = bounds.size.w;
  draw_status(ctx, bounds, GPointZero);
  draw_bar(ctx, GRect(SZ(6, 10), SZ(21, 28), w - 2 * SZ(6, 10), SZ(8, 12)), has_fix());
  const int centre_y = SZ(66, 92);
  draw_speed(ctx, &s_large_digits, GPoint(w / 2, centre_y),
             centre_y + s_large_digits.height / 2 + SZ(7, 10), w - 16);
  draw_footer(ctx, bounds);
}

#endif

static void time_text(char *out, size_t size, uint32_t seconds) {
  const unsigned long hours = seconds / 3600;
  const unsigned long minutes = seconds / 60 % 60;
  if (hours) {
    snprintf(out, size, "%lu:%02lu:%02lu", hours, minutes, (unsigned long)(seconds % 60));
  } else {
    snprintf(out, size, "%lu:%02lu", minutes, (unsigned long)(seconds % 60));
  }
}

// A distance in full: 0.42, 12.47, 123.4, 1234.
static void distance_text(char *out, size_t size, uint32_t dm) {
  const uint32_t x100 = distance_x100(dm);
  if (x100 < 10000) {
    snprintf(out, size, "%lu.%02lu", (unsigned long)(x100 / 100), (unsigned long)(x100 % 100));
  } else if (x100 < 100000) {
    snprintf(out, size, "%lu.%lu", (unsigned long)(x100 / 100),
             (unsigned long)(x100 % 100 / 10));
  } else {
    snprintf(out, size, "%lu", (unsigned long)(x100 / 100));
  }
}

#define TRIP_ROWS 4

// The caption and value of each row of the trip.
static void trip_row_text(int row, const char **caption, char *value, size_t size) {
  switch (row) {
    case 0: {
      *caption = "DIST";
      distance_text(value, size, s_state.trip_dm);
      const size_t used = strlen(value);
      snprintf(value + used, size - used, " %s", unit()->distance);
      break;
    }
    case 1:
      *caption = "TIME";
      time_text(value, size, s_state.trip_seconds);
      break;
    case 2: {
      *caption = "AVG";
      // Over the time spent moving, so stops do not pull it down.
      const uint32_t cms = s_state.trip_seconds
          ? (uint32_t)((uint64_t)s_state.trip_dm * 10 / s_state.trip_seconds) : 0;
      const int32_t x10 = speed_x10(cms);
      snprintf(value, size, "%d.%d %s", (int)(x10 / 10), (int)(x10 % 10), unit()->name);
      break;
    }
    default:
      *caption = "MAX";
      snprintf(value, size, "%d %s", (int)((speed_x10(s_state.max_cms) + 5) / 10), unit()->name);
      break;
  }
}

static const char *trip_hint(void) {
#if defined(APP_TOUCH)
  return "Hold to reset";
#else
  return "Hold Select to reset";
#endif
}

static void draw_trip_view(GContext *ctx, GRect bounds) {
  const int w = bounds.size.w;
  const char *caption;
  char value[24];
#if defined(PBL_ROUND)
  const GPoint c = grect_center_point(&bounds);
  draw_status(ctx, bounds, GPoint(c.x, SZ(24, 36)));
  const int pitch = SZ(26, 38);
  for (int row = 0; row < TRIP_ROWS; row++) {
    trip_row_text(row, &caption, value, sizeof(value));
    // Caption and value side by side, centred together.
    const int gap = SZ(6, 8);
    const int caption_width = text_width(caption, s_caption_font);
    const int width = caption_width + gap + text_width(value, s_value_font);
    const int left = c.x - width / 2;
    const int middle = c.y + SZ(-2, -2) + (2 * row - 3) * pitch / 2;
    draw_caption(ctx, caption, left, middle - CAPTION_CAP / 2, caption_width + 4,
                 GTextAlignmentLeft, stop_color(row * 2));
    draw_value(ctx, value, left + caption_width + gap, middle - VALUE_CAP / 2, 200,
               GTextAlignmentLeft, GColorWhite);
  }
  draw_caption(ctx, trip_hint(), 0, c.y + SZ(56, 82), w, GTextAlignmentCenter, COLOR_SOFT);
#else
  draw_status(ctx, bounds, GPointZero);
  const int top = SZ(19, 26);
  const int pitch = SZ(30, 41);
  const int side = SZ(5, 8);
  for (int row = 0; row < TRIP_ROWS; row++) {
    trip_row_text(row, &caption, value, sizeof(value));
    const int y = top + row * pitch;
    // A stripe of the dial's colours down the left.
    graphics_context_set_fill_color(ctx, stop_color(row * 2));
    graphics_fill_rect(ctx, GRect(side, y + SZ(5, 7), SZ(3, 4), pitch - SZ(10, 14)), 1, GCornersAll);
    draw_caption(ctx, caption, side + SZ(8, 11), y + (pitch - CAPTION_CAP) / 2, 60,
                 GTextAlignmentLeft, COLOR_SOFT);
    draw_value(ctx, value, 0, y + (pitch - VALUE_CAP) / 2, w - side, GTextAlignmentRight,
               GColorWhite);
#if defined(PBL_COLOR)
    if (row > 0) {
      graphics_context_set_stroke_color(ctx, COLOR_TRACK);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(side, y), GPoint(w - side, y));
    }
#endif
  }
  draw_caption(ctx, trip_hint(), 0, top + TRIP_ROWS * pitch + SZ(5, 7), w, GTextAlignmentCenter,
               COLOR_SOFT);
#endif
}

static void canvas_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, true);

  s_units_zone = GRectZero;
  switch (s_state.view) {
    case VIEW_DIGITS:
      draw_digits_view(ctx, bounds);
      break;
    case VIEW_TRIP:
      draw_trip_view(ctx, bounds);
      break;
    default:
      draw_dial_view(ctx, bounds);
      break;
  }

  const int w = bounds.size.w;
  const int h = bounds.size.h;
  draw_page_dots(ctx, GPoint(w / 2, h - PBL_IF_ROUND_ELSE(SZ(9, 13), SZ(4, 5))));
  draw_toast(ctx, GPoint(w / 2, h - PBL_IF_ROUND_ELSE(SZ(40, 58), SZ(24, 30))));
}

static void toast_timer_fired(void *data) {
  s_toast_timer = NULL;
  s_toast[0] = '\0';
  redraw();
}

// A short message over the bottom of the screen.
static void show_toast(const char *text, uint32_t ms) {
  strncpy(s_toast, text, sizeof(s_toast) - 1);
  s_toast[sizeof(s_toast) - 1] = '\0';
  if (s_toast_timer) {
    app_timer_reschedule(s_toast_timer, ms);
  } else {
    s_toast_timer = app_timer_register(ms, toast_timer_fired, NULL);
  }
  redraw();
}

// ---------------------------------------------------------------------------
// The needle's glide

#define GLIDE_MS 50

static void glide_timer_fired(void *data) {
  s_glide_timer = NULL;
  const int32_t target = has_fix() ? s_speed_cms : 0;
  const int32_t gap = target - s_shown_cms;
  if (ABS(gap) < 4) {
    s_shown_cms = target;
  } else {
    // A third of the way there each step: quick at first, then settling.
    s_shown_cms += gap * 3 / 10;
    s_glide_timer = app_timer_register(GLIDE_MS, glide_timer_fired, NULL);
  }
  redraw();
}

static void glide(void) {
  if (!s_glide_timer) {
    s_glide_timer = app_timer_register(GLIDE_MS, glide_timer_fired, NULL);
  }
}

// ---------------------------------------------------------------------------
// The trip and the settings

// A reset trip can be brought back by holding again within this many seconds.
#define UNDO_SECONDS 6

static uint32_t s_undo_dm;
static uint32_t s_undo_seconds;
static uint32_t s_undo_max;
static time_t s_undo_until;

static void reset_trip(void) {
  if (s_undo_until && time(NULL) <= s_undo_until) {
    s_state.trip_dm = s_undo_dm;
    s_state.trip_seconds = s_undo_seconds;
    s_state.max_cms = s_undo_max;
    s_undo_until = 0;
    show_toast("Trip restored", 2000);
  } else if (s_state.trip_dm || s_state.trip_seconds || s_state.max_cms) {
    s_undo_dm = s_state.trip_dm;
    s_undo_seconds = s_state.trip_seconds;
    s_undo_max = s_state.max_cms;
    s_undo_until = time(NULL) + UNDO_SECONDS;
    s_state.trip_dm = 0;
    s_state.trip_seconds = 0;
    s_state.max_cms = 0;
    vibes_short_pulse();
    show_toast("Reset. Hold to undo", UNDO_SECONDS * 1000);
  } else {
    show_toast("Trip is empty", 1500);
  }
  state_save();
}

static void set_view(int view) {
  s_state.view = (view + VIEW_COUNT) % VIEW_COUNT;
  s_state_dirty = true;
  redraw();
}

static void set_unit(int next) {
  // The alert speed stays the same speed: 50 km/h becomes 30 mph.
  const uint32_t before = unit()->speed_factor;
  s_state.unit = next % UNIT_COUNT;
  uint32_t alert = (s_state.alert_speed * unit()->speed_factor + before / 2) / before;
  alert = (alert + 2) / 5 * 5;
  s_state.alert_speed = alert < 5 ? 5 : alert > 995 ? 995 : alert;
  s_over = false;
  s_state_dirty = true;
  redraw();
}

static void apply_light(void) {
  light_enable(s_state.light_on);
}

// Vibrates once on passing the alert speed, and not again until the speed
// has dropped a little below it.
static void check_alert(void) {
  if (!s_state.alert_on) {
    s_over = false;
    return;
  }
  const int32_t x10 = speed_x10(s_speed_cms);
  const int32_t limit = s_state.alert_speed * 10;
  if (!s_over && x10 >= limit + 5) {
    s_over = true;
    vibes_double_pulse();
  } else if (s_over && x10 < limit - 15) {
    s_over = false;
  }
}

// ---------------------------------------------------------------------------
// Menu

static Window *s_menu_window;
static MenuLayer *s_menu;
static NumberWindow *s_alert_window;
static char s_alert_label[24];
static Window *s_help_window;
static ScrollLayer *s_help_scroll;
static TextLayer *s_help_text;

enum { SECTION_TRIP, SECTION_SETTINGS, SECTION_COUNT };

typedef enum {
  ROW_UNITS,
  ROW_RANGE,
  ROW_ALERT,
  ROW_ALERT_SPEED,
  ROW_THEME,
  ROW_LIGHT,
  ROW_HELP,
} SettingsRow;

static const uint8_t SETTINGS_ROWS[] = {
  ROW_UNITS,
  ROW_RANGE,
  ROW_ALERT,
  ROW_ALERT_SPEED,
#if defined(PBL_COLOR)
  ROW_THEME,
#endif
  ROW_LIGHT,
  ROW_HELP,
};

static const char HELP_TEXT[] =
    "The speed comes from your phone's GPS, so keep the phone with you, with "
    "location switched on.\n\n"
    "Up or Down: next view\n"
    "Select: this menu\n"
    "Hold Select: reset the trip\n\n"
#if defined(APP_TOUCH)
    "Swipe left or right: next view\n"
    "Swipe up: this menu\n"
    "Tap the speed: change units\n"
    "Hold on the trip: reset it\n\n"
#endif
    "The bars at the top show the GPS signal. The dial's range can follow "
    "your speed or be fixed, and the speed alert vibrates once when you pass "
    "it.";

static void help_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  const int margin = PBL_IF_ROUND_ELSE(SZ(18, 26), SZ(5, 8));
  const GFont font = fonts_get_system_font(SZ(FONT_KEY_GOTHIC_18, FONT_KEY_GOTHIC_24));
  const GRect box = GRect(margin, 0, bounds.size.w - 2 * margin, 2000);
  const GSize size = graphics_text_layout_get_content_size(
      HELP_TEXT, font, box, GTextOverflowModeWordWrap, GTextAlignmentLeft);

  s_help_scroll = scroll_layer_create(bounds);
  s_help_text = text_layer_create(GRect(margin, PBL_IF_ROUND_ELSE(SZ(20, 30), 0), box.size.w,
                                        size.h + 12));
  text_layer_set_text(s_help_text, HELP_TEXT);
  text_layer_set_font(s_help_text, font);
  text_layer_set_background_color(s_help_text, GColorClear);
  text_layer_set_text_color(s_help_text, GColorWhite);
  scroll_layer_add_child(s_help_scroll, text_layer_get_layer(s_help_text));
  scroll_layer_set_content_size(
      s_help_scroll, GSize(bounds.size.w, size.h + 12 + PBL_IF_ROUND_ELSE(SZ(50, 70), 6)));
  scroll_layer_set_click_config_onto_window(s_help_scroll, window);
  layer_add_child(root, scroll_layer_get_layer(s_help_scroll));
}

static void help_window_unload(Window *window) {
  text_layer_destroy(s_help_text);
  scroll_layer_destroy(s_help_scroll);
  s_help_text = NULL;
  s_help_scroll = NULL;
}

static void open_help(void) {
  if (!s_help_window) {
    s_help_window = window_create();
    window_set_background_color(s_help_window, GColorBlack);
    window_set_window_handlers(s_help_window, (WindowHandlers){
      .load = help_window_load,
      .unload = help_window_unload,
    });
  }
  window_stack_push(s_help_window, true);
}

static void alert_selected(NumberWindow *window, void *context) {
  s_state.alert_speed = number_window_get_value(window);
  s_state.alert_on = true;
  s_over = false;
  state_save();
  window_stack_pop(true);
}

static void open_alert_speed(void) {
  snprintf(s_alert_label, sizeof(s_alert_label), "Alert over (%s)", unit()->menu_name);
  if (!s_alert_window) {
    s_alert_window = number_window_create(s_alert_label, (NumberWindowCallbacks){
      .selected = alert_selected,
    }, NULL);
    number_window_set_min(s_alert_window, 5);
    number_window_set_max(s_alert_window, 995);
    number_window_set_step_size(s_alert_window, 5);
  }
  number_window_set_label(s_alert_window, s_alert_label);
  number_window_set_value(s_alert_window, s_state.alert_speed);
  window_stack_push(number_window_get_window(s_alert_window), true);
}

static uint16_t menu_sections(MenuLayer *menu, void *data) {
  return SECTION_COUNT;
}

static uint16_t menu_rows(MenuLayer *menu, uint16_t section, void *data) {
  return section == SECTION_TRIP ? 1 : ARRAY_LENGTH(SETTINGS_ROWS);
}

static int16_t menu_header_height(MenuLayer *menu, uint16_t section, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  const char *title = section == SECTION_TRIP ? "Trip" : "Settings";
#if defined(PBL_ROUND)
  // Centred, as the round screen's edge would cut it at the left.
  const GRect bounds = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, COLOR_SOFT);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, -2, bounds.size.w, bounds.size.h + 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  menu_cell_basic_header_draw(ctx, cell, title);
#endif
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  static char subtitle[40];
  const char *title = "";
  subtitle[0] = '\0';

  if (index->section == SECTION_TRIP) {
    title = "Reset trip";
    if (s_state.trip_dm || s_state.trip_seconds) {
      char distance[12], duration[12];
      distance_text(distance, sizeof(distance), s_state.trip_dm);
      time_text(duration, sizeof(duration), s_state.trip_seconds);
      snprintf(subtitle, sizeof(subtitle), "%s %s in %s", distance,
               s_state.unit == UNIT_KMH ? "km" : s_state.unit == UNIT_MPH ? "mi" : "nm",
               duration);
    } else {
      snprintf(subtitle, sizeof(subtitle), "Nothing to reset yet");
    }
  } else {
    switch (SETTINGS_ROWS[index->row]) {
      case ROW_UNITS:
        title = "Units";
        snprintf(subtitle, sizeof(subtitle), "%s", unit()->menu_name);
        break;
      case ROW_RANGE:
        title = "Dial range";
        if (s_state.range == RANGE_AUTO) {
          snprintf(subtitle, sizeof(subtitle), "Follows your speed");
        } else {
          snprintf(subtitle, sizeof(subtitle), "0 to %d %s", RANGES[s_state.range],
                   unit()->menu_name);
        }
        break;
      case ROW_ALERT:
        title = "Speed alert";
        snprintf(subtitle, sizeof(subtitle), "%s", s_state.alert_on ? "On" : "Off");
        break;
      case ROW_ALERT_SPEED:
        title = "Alert over";
        snprintf(subtitle, sizeof(subtitle), "%d %s", s_state.alert_speed, unit()->menu_name);
        break;
      case ROW_THEME:
        title = "Colours";
#if defined(PBL_COLOR)
        snprintf(subtitle, sizeof(subtitle), "%s", THEMES[s_state.theme].name);
#endif
        break;
      case ROW_LIGHT:
        title = "Screen light";
        snprintf(subtitle, sizeof(subtitle), "%s",
                 s_state.light_on ? "Stays on in the app" : "Automatic");
        break;
      case ROW_HELP:
        title = "How to use";
        snprintf(subtitle, sizeof(subtitle), "Buttons%s", PBL_IF_COLOR_ELSE("", ""));
#if defined(APP_TOUCH)
        snprintf(subtitle, sizeof(subtitle), "Buttons and touch");
#endif
        break;
    }
  }
  menu_cell_basic_draw(ctx, cell, title, subtitle, NULL);
}

static void menu_apply_colors(void) {
#if defined(PBL_COLOR)
  if (s_menu) {
    menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
    menu_layer_set_highlight_colors(s_menu, accent_color(), GColorBlack);
  }
#endif
}

static void menu_select(MenuLayer *menu, MenuIndex *index, void *data) {
  if (index->section == SECTION_TRIP) {
    window_stack_pop(true);
    reset_trip();
    return;
  }
  switch (SETTINGS_ROWS[index->row]) {
    case ROW_UNITS:
      set_unit(s_state.unit + 1);
      break;
    case ROW_RANGE:
      s_state.range = (s_state.range + 1) % RANGE_COUNT;
      break;
    case ROW_ALERT:
      s_state.alert_on = !s_state.alert_on;
      s_over = false;
      break;
    case ROW_ALERT_SPEED:
      open_alert_speed();
      return;
    case ROW_THEME:
      s_state.theme = (s_state.theme + 1) % THEME_COUNT;
      menu_apply_colors();
      break;
    case ROW_LIGHT:
      s_state.light_on = !s_state.light_on;
      apply_light();
      break;
    case ROW_HELP:
      open_help();
      return;
  }
  state_save();
  menu_layer_reload_data(menu);
  redraw();
}

static void menu_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_sections,
    .get_num_rows = menu_rows,
    .get_header_height = menu_header_height,
    .draw_header = menu_draw_header,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
  });
  menu_apply_colors();
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void menu_window_appear(Window *window) {
  // The alert speed may have changed in its own window.
  menu_layer_reload_data(s_menu);
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

static void open_menu(void) {
  if (!s_menu_window) {
    s_menu_window = window_create();
    window_set_background_color(s_menu_window, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    window_set_window_handlers(s_menu_window, (WindowHandlers){
      .load = menu_window_load,
      .appear = menu_window_appear,
      .unload = menu_window_unload,
    });
  }
  window_stack_push(s_menu_window, true);
}

// ---------------------------------------------------------------------------
// Buttons

static void up_click(ClickRecognizerRef recognizer, void *context) {
  set_view(s_state.view - 1);
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  set_view(s_state.view + 1);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  open_menu();
}

static void select_long_click(ClickRecognizerRef recognizer, void *context) {
  reset_trip();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click, NULL);
}

// ---------------------------------------------------------------------------
// Touch (Pebble Time 2 and Pebble Round 2). The main screen reads the raw
// touch events and works out taps, swipes and holds itself; the menus use
// the system's touch navigation.

#if defined(APP_TOUCH)

// A touch that moves less than this is a tap or a hold.
#define TAP_SLOP 12
// A swipe is at least this long.
#define SWIPE_MIN 40
// A touch kept still for this long is a hold.
#define HOLD_MS 700

static GPoint s_touch_start;
static bool s_touch_active;
static AppTimer *s_hold_timer;

// A brief tick when a touch does something.
static void touch_feedback(void) {
  static const uint32_t segments[] = {30};
  vibes_enqueue_custom_pattern((VibePattern){
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  });
}

static void hold_cancel(void) {
  if (s_hold_timer) {
    app_timer_cancel(s_hold_timer);
    s_hold_timer = NULL;
  }
}

static void hold_timer_fired(void *data) {
  s_hold_timer = NULL;
  if (!s_touch_active) {
    return;
  }
  // The hold has been used; lifting the finger must not also count as a tap.
  s_touch_active = false;
  if (s_state.view == VIEW_TRIP) {
    reset_trip();
  }
}

static void touch_tap(GPoint point) {
  if (grect_contains_point(&s_units_zone, &point)) {
    set_unit(s_state.unit + 1);
    touch_feedback();
    show_toast(unit()->menu_name, 1200);
  }
}

static void touch_event(const TouchEvent *event, void *context) {
  const GPoint point = GPoint(event->x, event->y);
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_active = !event->non_navigational;
      s_touch_start = point;
      hold_cancel();
      if (s_touch_active) {
        s_hold_timer = app_timer_register(HOLD_MS, hold_timer_fired, NULL);
      }
      break;
    case TouchEvent_PositionUpdate:
      if (ABS(point.x - s_touch_start.x) > TAP_SLOP || ABS(point.y - s_touch_start.y) > TAP_SLOP) {
        hold_cancel();
      }
      break;
    case TouchEvent_Liftoff: {
      hold_cancel();
      if (!s_touch_active) {
        break;
      }
      s_touch_active = false;
      const int dx = point.x - s_touch_start.x;
      const int dy = point.y - s_touch_start.y;
      if (ABS(dx) <= TAP_SLOP && ABS(dy) <= TAP_SLOP) {
        touch_tap(s_touch_start);
      } else if (ABS(dx) >= SWIPE_MIN && ABS(dy) < ABS(dx) / 2) {
        // Swiping left brings the next view in from the right.
        set_view(s_state.view + (dx < 0 ? 1 : -1));
      } else if (dy <= -SWIPE_MIN && ABS(dx) < ABS(dy) / 2) {
        open_menu();
      }
      break;
    }
  }
}

#endif

static void main_window_appear(Window *window) {
#if defined(APP_TOUCH)
  s_touch_active = false;
  touch_service_subscribe(touch_event, NULL);
#endif
  redraw();
}

static void main_window_disappear(Window *window) {
#if defined(APP_TOUCH)
  hold_cancel();
  touch_service_unsubscribe();
#endif
}

// ---------------------------------------------------------------------------
// Messages from the phone

static int32_t tuple_int(const Tuple *tuple) {
  if (tuple->type == TUPLE_UINT) {
    return tuple->length == 1 ? tuple->value->uint8
         : tuple->length == 2 ? tuple->value->uint16 : (int32_t)tuple->value->uint32;
  }
  return tuple->length == 1 ? tuple->value->int8
       : tuple->length == 2 ? tuple->value->int16 : tuple->value->int32;
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  const Tuple *status = dict_find(iter, MESSAGE_KEY_STATUS);
  if (!status) {
    return;
  }
  const int32_t link = tuple_int(status);
  s_link = link >= LINK_FIX && link <= LINK_ERROR ? (LinkStatus)link : LINK_ERROR;

  if (s_link == LINK_FIX) {
    const Tuple *speed = dict_find(iter, MESSAGE_KEY_SPEED);
    const Tuple *heading = dict_find(iter, MESSAGE_KEY_HEADING);
    const Tuple *accuracy = dict_find(iter, MESSAGE_KEY_ACCURACY);
    const Tuple *odometer = dict_find(iter, MESSAGE_KEY_ODOMETER);
    const int32_t cms = speed ? tuple_int(speed) : 0;
    s_speed_cms = cms < 0 ? 0 : cms;
    s_heading = heading ? tuple_int(heading) : -1;
    s_accuracy = accuracy ? tuple_int(accuracy) : -1;
    s_fix_time = time(NULL);

    if (odometer) {
      const uint32_t now = (uint32_t)tuple_int(odometer);
      // The phone's count starts again when its side restarts.
      if (s_have_odometer && now >= s_odometer) {
        s_state.trip_dm += now - s_odometer;
      }
      s_odometer = now;
      s_have_odometer = true;
    }
    if ((uint32_t)s_speed_cms > s_state.max_cms) {
      s_state.max_cms = s_speed_cms;
    }
    if ((uint32_t)s_speed_cms > s_state.peak_cms) {
      s_state.peak_cms = s_speed_cms;
    }
    s_state_dirty = true;
    check_alert();
  }
  glide();
  redraw();
}

static void connection_changed(bool connected) {
  s_connected = connected;
  glide();
  redraw();
}

// Once a second: counts the time spent moving, lets the automatic dial range
// come back down, and keeps the clock and the signal up to date.
static void second_tick(struct tm *tick_time, TimeUnits changed) {
  if (has_fix()) {
    if (s_speed_cms > 0) {
      s_state.trip_seconds++;
      // The recent top speed fades while moving, but not below the speed now.
      if (s_state.peak_cms > (uint32_t)s_speed_cms) {
        const uint32_t fade = s_state.peak_cms / 128 + 1;
        s_state.peak_cms -= MIN(fade, s_state.peak_cms - s_speed_cms);
      }
      s_state_dirty = true;
    }
  } else {
    s_over = false;
    glide();
  }
  // Keep the trip safe every two minutes, in case the battery gives out.
  if (s_state_dirty && tick_time->tm_sec == 0 && tick_time->tm_min % 2 == 0) {
    state_save();
  }
  redraw();
}

// ---------------------------------------------------------------------------
// Start and finish

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
#if defined(APP_TOUCH)
  // Touches here are read by touch_event, not turned into button presses.
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  state_load();
  fonts_load();
  s_arrow = gpath_create(&ARROW_INFO);
  s_connected = connection_service_peek_pebble_app_connection();

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 16);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = main_window_load,
    .appear = main_window_appear,
    .disappear = main_window_disappear,
    .unload = main_window_unload,
  });
  window_stack_push(s_window, true);

  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = connection_changed,
  });
  tick_timer_service_subscribe(SECOND_UNIT, second_tick);
  apply_light();

#if defined(APP_TOUCH)
  // Lets the menus scroll by touch.
  app_touch_navigation_enable(true);
  // The first few times, say how to get around by touch.
  if (s_state.launches < 3) {
    s_state.launches++;
    show_toast("Swipe to change view", 4000);
  }
#endif
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  light_enable(false);
  if (s_glide_timer) {
    app_timer_cancel(s_glide_timer);
  }
  if (s_toast_timer) {
    app_timer_cancel(s_toast_timer);
  }
  state_save();
  if (s_alert_window) {
    number_window_destroy(s_alert_window);
  }
  if (s_help_window) {
    window_destroy(s_help_window);
  }
  if (s_menu_window) {
    window_destroy(s_menu_window);
  }
  window_destroy(s_window);
  gpath_destroy(s_arrow);
  fonts_unload();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
