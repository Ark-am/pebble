#include <pebble.h>

// A digital face after the classic F-91W: a segmented LCD showing the time,
// the seconds and a top row, set in a resin case with printed labels. The
// wearer chooses the LCD and case colours, the brand and label text, whether
// the seconds show, 12- or 24-hour time, an
// hourly chime, and what the top row shows: the day and date, the weather,
// steps or heart rate, or each in turn with a shake of the wrist.

#define PERSIST_KEY_SETTINGS 1
#define PERSIST_KEY_TEMPERATURE 2
#define PERSIST_KEY_CONDITION 3
#define PERSIST_KEY_INFO 4

#define WEATHER_REFRESH_MINUTES 30
#define NO_TEMPERATURE INT32_MIN

#if PBL_DISPLAY_WIDTH >= 200
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define LABEL_PAD 4
  #define LABEL_HEIGHT 10
  #define BRAND_FONT FONT_KEY_GOTHIC_28_BOLD
  #define BRAND_PAD 9
  #define BRAND_HEIGHT 20
  #define MODEL_FONT FONT_KEY_GOTHIC_18_BOLD
  #define MODEL_PAD 5
  #define MODEL_HEIGHT 13
  #define BOTTOM_FONT FONT_KEY_GOTHIC_18_BOLD
  #define BOTTOM_PAD 5
  #define BOTTOM_HEIGHT 13
  #define ROW_GAP 4
  #define FRAME_WIDTH 2
  #define FRAME_PAD 6
  #define TRIM 2
  #define LCD_PAD 5
  #define PM_FONT FONT_KEY_GOTHIC_14_BOLD
  #define PM_PAD 4
  #define PM_HEIGHT 10
#else
  #define LABEL_FONT FONT_KEY_GOTHIC_09
  #define LABEL_PAD 2
  #define LABEL_HEIGHT 7
  #define BRAND_FONT FONT_KEY_GOTHIC_18_BOLD
  #define BRAND_PAD 5
  #define BRAND_HEIGHT 13
  #define MODEL_FONT FONT_KEY_GOTHIC_14_BOLD
  #define MODEL_PAD 4
  #define MODEL_HEIGHT 10
  #define BOTTOM_FONT FONT_KEY_GOTHIC_14_BOLD
  #define BOTTOM_PAD 4
  #define BOTTOM_HEIGHT 10
  #define ROW_GAP 3
  #define FRAME_WIDTH 2
  #define FRAME_PAD 4
  #define TRIM 1
  #define LCD_PAD 3
  #define PM_FONT FONT_KEY_GOTHIC_09
  #define PM_PAD 2
  #define PM_HEIGHT 7
#endif

// LCD colours; values match the options in src/pkjs/config.js.
typedef enum {
  LCD_CLASSIC,
  LCD_BACKLIT,
  LCD_BLUE,
  LCD_AMBER,
  LCD_DARK,
  LCD_COUNT,
} LcdColour;

// Case colours.
typedef enum {
  CASE_BLACK,
  CASE_GOLD,
  CASE_SILVER,
  CASE_COUNT,
} CaseColour;

// What the top row of the LCD shows.
typedef enum {
  TOP_DATE,
  TOP_WEATHER,
  TOP_STEPS,
  TOP_HEART_RATE,
  TOP_CYCLE,  // Each of the others in turn, with a shake.
  TOP_COUNT,
} TopRow;

typedef enum {
  FORMAT_WATCH,  // Whatever the watch is set to.
  FORMAT_12H,
  FORMAT_24H,
  FORMAT_COUNT,
} TimeFormat;

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 2
typedef struct {
  uint8_t version;
  uint8_t lcd;
  uint8_t case_colour;
  char brand[16];
  char model[16];
  char top_label[32];
  char bottom_label[32];
  bool seconds;
  bool chime;
  uint8_t time_format;
  uint8_t top_row;
} Settings;

static Settings s_settings;

typedef struct {
  GColor body;       // The case.
  GColor print;      // Text printed on the case.
  GColor frame;      // The line framing the face plate.
  GColor model;      // The model name beside the brand.
  GColor badge;      // The middle word of the bottom label, in its box.
  GColor trim;       // The thin border round the LCD.
  GColor lcd;        // The LCD background.
  GColor segment;    // Lit segments.
} Palette;

static Palette s_palette;

static Window *s_window;
static Layer *s_canvas;

static int32_t s_temperature = NO_TEMPERATURE;
static char s_condition[4];
// What the top row shows when it changes with a shake.
static TopRow s_info = TOP_DATE;

// ---------------------------------------------------------------------------
// Settings

static void settings_set_defaults(Settings *settings) {
  *settings = (Settings) {
    .version = SETTINGS_VERSION,
    .lcd = LCD_CLASSIC,
    .case_colour = CASE_BLACK,
    .brand = "PEBBLE",
    .model = "F-91W",
    .top_label = "ALARM CHRONOGRAPH",
    .bottom_label = "WATER WR RESIST",
    .seconds = false,
    .chime = false,
    .time_format = FORMAT_WATCH,
    .top_row = TOP_DATE,
  };
}

static void apply_palette(void) {
#if defined(PBL_COLOR)
  switch (s_settings.case_colour) {
    case CASE_GOLD:
      s_palette.body = GColorChromeYellow;
      s_palette.print = GColorBlack;
      s_palette.frame = GColorBlack;
      s_palette.model = GColorBlack;
      s_palette.badge = GColorDarkCandyAppleRed;
      s_palette.trim = GColorBlack;
      break;
    case CASE_SILVER:
      s_palette.body = GColorLightGray;
      s_palette.print = GColorBlack;
      s_palette.frame = GColorDukeBlue;
      s_palette.model = GColorOrange;
      s_palette.badge = GColorRed;
      s_palette.trim = GColorDarkGray;
      break;
    default:
      // As the original: a blue frame, an orange model name and a red badge.
      s_palette.body = GColorBlack;
      s_palette.print = GColorWhite;
      s_palette.frame = GColorBlueMoon;
      s_palette.model = GColorChromeYellow;
      s_palette.badge = GColorRed;
      s_palette.trim = GColorDarkGray;
      break;
  }
  switch (s_settings.lcd) {
    case LCD_BACKLIT:
      s_palette.lcd = GColorSpringBud;
      s_palette.segment = GColorBlack;
      break;
    case LCD_BLUE:
      s_palette.lcd = GColorCeleste;
      s_palette.segment = GColorOxfordBlue;
      break;
    case LCD_AMBER:
      s_palette.lcd = GColorRajah;
      s_palette.segment = GColorBlack;
      break;
    case LCD_DARK:
      s_palette.lcd = GColorBlack;
      s_palette.segment = GColorWhite;
      break;
    default:
      s_palette.lcd = GColorLightGray;
      s_palette.segment = GColorBlack;
      break;
  }
#else
  // Black and white: a black case with white print, and a white LCD (black
  // for the dark LCD).
  s_palette.body = GColorBlack;
  s_palette.print = GColorWhite;
  s_palette.frame = GColorWhite;
  s_palette.model = GColorWhite;
  s_palette.badge = GColorWhite;
  s_palette.trim = GColorWhite;
  const bool dark = s_settings.lcd == LCD_DARK;
  s_palette.lcd = dark ? GColorBlack : GColorWhite;
  s_palette.segment = dark ? GColorWhite : GColorBlack;
#endif
}

static bool use_24h(void) {
  switch (s_settings.time_format) {
    case FORMAT_12H: return false;
    case FORMAT_24H: return true;
    default: return clock_is_24h_style();
  }
}

static bool weather_wanted(void) {
  return s_settings.top_row == TOP_WEATHER || s_settings.top_row == TOP_CYCLE;
}

// What the top row shows right now.
static TopRow top_row_shown(void) {
  return s_settings.top_row == TOP_CYCLE ? s_info : (TopRow)s_settings.top_row;
}

// ---------------------------------------------------------------------------
// Segments
//
// Every segment is a bar from one point to another, cut to a point at each
// end the way LCD segments are, and filled solid.

static int32_t isqrt(int32_t n) {
  if (n <= 0) {
    return 0;
  }
  int32_t x = n;
  int32_t y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

static void draw_lit_segment(GContext *ctx, GPoint a, GPoint b, int16_t thickness) {
  const int32_t dx = b.x - a.x;
  const int32_t dy = b.y - a.y;
  const int32_t length = isqrt(dx * dx + dy * dy);
  const int16_t half = thickness / 2;
  if (length == 0 || half < 2) {
    // Too thin to cut to a point; a plain bar reads the same.
    graphics_context_set_stroke_color(ctx, s_palette.segment);
    graphics_context_set_stroke_width(ctx, thickness);
    graphics_draw_line(ctx, a, b);
    return;
  }
  // Along and across the bar, each half the thickness long.
  const int16_t ux = dx * half / length;
  const int16_t uy = dy * half / length;
  const int16_t nx = -uy;
  const int16_t ny = ux;
  GPoint points[] = {
    a,
    GPoint(a.x + ux + nx, a.y + uy + ny),
    GPoint(b.x - ux + nx, b.y - uy + ny),
    b,
    GPoint(b.x - ux - nx, b.y - uy - ny),
    GPoint(a.x + ux - nx, a.y + uy - ny),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, s_palette.segment);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Only lit segments are drawn; unlit ones leave the LCD blank.
static void draw_segment(GContext *ctx, GPoint a, GPoint b, int16_t thickness, bool lit) {
  if (lit) {
    draw_lit_segment(ctx, a, b, thickness);
  }
}

// Seven-segment characters: digits, a minus sign, a degree sign and a blank.
// Bits run a (top), b, c, d, e, f clockwise, then g (middle).
static uint8_t seven_segments(char c) {
  static const uint8_t DIGITS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
  };
  if (c >= '0' && c <= '9') {
    return DIGITS[c - '0'];
  }
  switch (c) {
    case '-': return 0x40;
    case '*': return 0x63;  // The degree sign: a small box at the top.
    default: return 0;
  }
}

static void draw_seven(GContext *ctx, char c, GRect box, int16_t thickness) {
  const uint8_t lit = seven_segments(c);
  const int16_t h = thickness / 2;
  const int16_t gap = thickness / 3 + 1;
  const int16_t left = box.origin.x + h;
  const int16_t right = box.origin.x + box.size.w - 1 - h;
  const int16_t top = box.origin.y + h;
  const int16_t bottom = box.origin.y + box.size.h - 1 - h;
  const int16_t middle = (top + bottom) / 2;
  const GPoint ends[7][2] = {
    { GPoint(left + gap, top), GPoint(right - gap, top) },
    { GPoint(right, top + gap), GPoint(right, middle - gap) },
    { GPoint(right, middle + gap), GPoint(right, bottom - gap) },
    { GPoint(left + gap, bottom), GPoint(right - gap, bottom) },
    { GPoint(left, middle + gap), GPoint(left, bottom - gap) },
    { GPoint(left, top + gap), GPoint(left, middle - gap) },
    { GPoint(left + gap, middle), GPoint(right - gap, middle) },
  };
  for (int i = 0; i < 7; i++) {
    draw_segment(ctx, ends[i][0], ends[i][1], thickness, lit & (1 << i));
  }
}

// Fourteen-segment letters for the day names and labels. Bits run as for
// seven segments (with the middle split into a left and right half), then the
// upper-left diagonal, upper centre, upper-right diagonal, lower-left
// diagonal, lower centre and lower-right diagonal.
enum {
  S_A = 1 << 0, S_B = 1 << 1, S_C = 1 << 2, S_D = 1 << 3, S_E = 1 << 4, S_F = 1 << 5,
  S_G1 = 1 << 6, S_G2 = 1 << 7, S_H = 1 << 8, S_I = 1 << 9, S_J = 1 << 10,
  S_K = 1 << 11, S_L = 1 << 12, S_M = 1 << 13,
};

static uint16_t fourteen_segments(char c) {
  switch (c) {
    case 'A': return S_A | S_B | S_C | S_E | S_F | S_G1 | S_G2;
    case 'C': return S_A | S_D | S_E | S_F;
    case 'D': return S_A | S_B | S_C | S_D | S_I | S_L;
    case 'E': return S_A | S_D | S_E | S_F | S_G1;
    case 'F': return S_A | S_E | S_F | S_G1;
    case 'G': return S_A | S_C | S_D | S_E | S_F | S_G2;
    case 'H': return S_B | S_C | S_E | S_F | S_G1 | S_G2;
    case 'I': return S_A | S_D | S_I | S_L;
    case 'L': return S_D | S_E | S_F;
    case 'M': return S_B | S_C | S_E | S_F | S_H | S_J;
    case 'N': return S_B | S_C | S_E | S_F | S_H | S_M;
    case 'O': return S_A | S_B | S_C | S_D | S_E | S_F;
    case 'P': return S_A | S_B | S_E | S_F | S_G1 | S_G2;
    case 'R': return S_A | S_B | S_E | S_F | S_G1 | S_G2 | S_M;
    case 'S': return S_A | S_C | S_D | S_F | S_G1 | S_G2;
    case 'T': return S_A | S_I | S_L;
    case 'U': return S_B | S_C | S_D | S_E | S_F;
    case 'W': return S_B | S_C | S_E | S_F | S_K | S_M;
    case 'Z': return S_A | S_D | S_J | S_K;
    case '-': return S_G1 | S_G2;
    default: return 0;
  }
}

static void draw_fourteen(GContext *ctx, char c, GRect box, int16_t thickness) {
  const uint16_t lit = fourteen_segments(c);
  const int16_t h = thickness / 2;
  const int16_t gap = 1;
  const int16_t left = box.origin.x + h;
  const int16_t right = box.origin.x + box.size.w - 1 - h;
  const int16_t top = box.origin.y + h;
  const int16_t bottom = box.origin.y + box.size.h - 1 - h;
  const int16_t middle = (top + bottom) / 2;
  const int16_t centre = (left + right) / 2;
  const GPoint ends[14][2] = {
    { GPoint(left + gap, top), GPoint(right - gap, top) },
    { GPoint(right, top + gap), GPoint(right, middle - gap) },
    { GPoint(right, middle + gap), GPoint(right, bottom - gap) },
    { GPoint(left + gap, bottom), GPoint(right - gap, bottom) },
    { GPoint(left, middle + gap), GPoint(left, bottom - gap) },
    { GPoint(left, top + gap), GPoint(left, middle - gap) },
    { GPoint(left + gap, middle), GPoint(centre - gap, middle) },
    { GPoint(centre + gap, middle), GPoint(right - gap, middle) },
    { GPoint(left + gap, top + gap), GPoint(centre - gap, middle - gap) },
    { GPoint(centre, top + gap), GPoint(centre, middle - gap) },
    { GPoint(right - gap, top + gap), GPoint(centre + gap, middle - gap) },
    { GPoint(centre - gap, middle + gap), GPoint(left + gap, bottom - gap) },
    { GPoint(centre, middle + gap), GPoint(centre, bottom - gap) },
    { GPoint(centre + gap, middle + gap), GPoint(right - gap, bottom - gap) },
  };
  for (int i = 0; i < 14; i++) {
    draw_segment(ctx, ends[i][0], ends[i][1], thickness, lit & (1 << i));
  }
}

// A row of characters, each in a box of the given size with a space after it.
static void draw_seven_text(GContext *ctx, const char *text, GPoint origin, GSize size,
                            int16_t spacing, int16_t thickness) {
  for (int i = 0; text[i]; i++) {
    draw_seven(ctx, text[i], GRect(origin.x + i * (size.w + spacing), origin.y,
                                   size.w, size.h), thickness);
  }
}

static void draw_fourteen_text(GContext *ctx, const char *text, GPoint origin, GSize size,
                               int16_t spacing, int16_t thickness) {
  for (int i = 0; text[i]; i++) {
    draw_fourteen(ctx, text[i], GRect(origin.x + i * (size.w + spacing), origin.y,
                                      size.w, size.h), thickness);
  }
}

// ---------------------------------------------------------------------------
// Top row

#define VALUE_SIZE 16

// Fills in the top row: two letters on the left and up to five characters,
// right-aligned, on the right. The value buffer holds VALUE_SIZE characters.
static void top_row_text(const struct tm *t, char *letters, char *value) {
  static const char *const DAYS[7] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
  switch (top_row_shown()) {
    case TOP_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        strcpy(letters, "--");
        strcpy(value, "--*");
      } else {
        strncpy(letters, s_condition[0] ? s_condition : "--", 3);
        letters[2] = '\0';
        snprintf(value, VALUE_SIZE, "%d*", (int)s_temperature);
      }
      break;
#if defined(PBL_HEALTH)
    case TOP_STEPS:
      strcpy(letters, "ST");
      snprintf(value, VALUE_SIZE, "%d", (int)(health_service_sum_today(HealthMetricStepCount) % 100000));
      break;
    case TOP_HEART_RATE: {
      strcpy(letters, "HR");
      const HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
      if (bpm > 0) {
        snprintf(value, VALUE_SIZE, "%d", (int)bpm);
      } else {
        strcpy(value, "--");
      }
      break;
    }
#endif
    default: {
      strcpy(letters, DAYS[t->tm_wday]);
      snprintf(value, VALUE_SIZE, "%d", t->tm_mday);
      break;
    }
  }
}

// The next thing the top row shows with a shake, skipping what this watch
// cannot show.
static TopRow next_info(TopRow from) {
  for (int i = 1; i <= TOP_CYCLE; i++) {
    const TopRow candidate = (from + i) % TOP_CYCLE;
#if !defined(PBL_HEALTH)
    if (candidate == TOP_STEPS || candidate == TOP_HEART_RATE) {
      continue;
    }
#endif
    return candidate;
  }
  return TOP_DATE;
}

// ---------------------------------------------------------------------------
// Drawing

static void draw_label(GContext *ctx, const char *text, const char *font_key, GRect box,
                       int16_t pad, GTextAlignment alignment, GColor colour) {
  graphics_context_set_text_color(ctx, colour);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key),
                     GRect(box.origin.x, box.origin.y - pad, box.size.w, box.size.h + pad * 2),
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static int16_t text_width(const char *text, const char *font_key, int16_t height) {
  return graphics_text_layout_get_content_size(
    text, fonts_get_system_font(font_key), GRect(0, 0, 400, height * 3),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w;
}

// The hourly signal's sign, a dot between two pairs of brackets: (( . )).
static void draw_chime_sign(GContext *ctx, GPoint at, int16_t size) {
  graphics_context_set_stroke_color(ctx, s_palette.segment);
  graphics_context_set_fill_color(ctx, s_palette.segment);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_fill_circle(ctx, at, size / 6 > 1 ? size / 6 : 1);
  for (int side = -1; side <= 1; side += 2) {
    for (int ring = 1; ring <= 2; ring++) {
      const int16_t x = at.x + side * ring * size / 4;
      const int16_t reach = size * ring / 6;
      graphics_draw_line(ctx, GPoint(x, at.y - reach), GPoint(x + side, at.y));
      graphics_draw_line(ctx, GPoint(x + side, at.y), GPoint(x, at.y + reach));
    }
  }
}

// The LCD's contents inside the given area: a narrow column of signs at the
// left (the hourly signal and PM), the top row beside it, and the time below
// across the whole width, with the seconds smaller at the bottom right.
static void draw_lcd(GContext *ctx, GRect area, const struct tm *t) {
  const int16_t x0 = area.origin.x;
  const int16_t y0 = area.origin.y;
  const int16_t w = area.size.w;
  const int16_t h = area.size.h;

  const int16_t row_h = h * 32 / 100;
  const int16_t column_w = w * 20 / 100;
  const int16_t time_top = y0 + row_h + h * 8 / 100;
  const int16_t time_h = y0 + h - time_top;

  // The signs.
  if (s_settings.chime) {
    draw_chime_sign(ctx, GPoint(x0 + column_w / 3, y0 + row_h / 4), row_h / 2);
  }
  if (!use_24h() && t->tm_hour >= 12) {
    draw_label(ctx, "PM", PM_FONT, GRect(x0, y0 + row_h - PM_HEIGHT, column_w, PM_HEIGHT),
               PM_PAD, GTextAlignmentLeft, s_palette.segment);
  }

  // The top row: two letters after the signs, and the value at the right.
  const GSize glyph = GSize(row_h * 62 / 100, row_h);
  const int16_t glyph_gap = glyph.w * 30 / 100;
  const int16_t glyph_t = row_h / 8 > 2 ? row_h / 8 : 2;
  char letters[4];
  char value[VALUE_SIZE];
  top_row_text(t, letters, value);
  draw_fourteen_text(ctx, letters, GPoint(x0 + column_w, y0), glyph, glyph_gap, glyph_t);
  // At least two places, as for the date, and up to five.
  int places = strlen(value);
  places = places < 2 ? 2 : places > 5 ? 5 : places;
  char shown[VALUE_SIZE];
  snprintf(shown, sizeof(shown), "%*.*s", places, places, value);
  draw_seven_text(ctx, shown, GPoint(x0 + w - places * glyph.w - (places - 1) * glyph_gap, y0),
                  glyph, glyph_gap, glyph_t);

  // The time. Sizes are in 100ths of a large digit's width.
  const bool twenty_four = use_24h();
  const bool seconds = s_settings.seconds;
  const int32_t parts = seconds ? 622 : 502;
  int16_t digit_w = w * 100 / parts;
  int16_t digit_h = digit_w * 175 / 100;
  if (digit_h > time_h) {
    digit_h = time_h;
    digit_w = digit_h * 100 / 175;
  }
  const int16_t thickness = digit_w * 20 / 100 > 2 ? digit_w * 20 / 100 : 2;
  const int16_t spacing = digit_w * 18 / 100;
  const int16_t colon_w = digit_w * 30 / 100;
  const int16_t small_w = digit_w * 55 / 100;
  const int16_t small_h = digit_h * 58 / 100;
  const int16_t total = 4 * digit_w + 3 * spacing + colon_w
    + (seconds ? spacing + 2 * small_w + spacing / 2 : 0);
  int16_t x = x0 + (w - total) / 2;
  const int16_t y = time_top + (time_h - digit_h) / 2;

  int hour = t->tm_hour;
  if (!twenty_four) {
    hour = hour % 12 ? hour % 12 : 12;
  }
  char digits[5];
  snprintf(digits, sizeof(digits), "%02d%02d", hour, t->tm_min);
  if (!twenty_four && digits[0] == '0') {
    digits[0] = ' ';  // 12-hour time leaves the first place blank.
  }
  for (int i = 0; i < 4; i++) {
    draw_seven(ctx, digits[i], GRect(x, y, digit_w, digit_h), thickness);
    x += digit_w + spacing;
    if (i == 1) {
      // The colon: two small squares.
      const int16_t dot = thickness;
      const int16_t cx = x - spacing + (colon_w + spacing - dot) / 2;
      graphics_context_set_fill_color(ctx, s_palette.segment);
      graphics_fill_rect(ctx, GRect(cx, y + digit_h * 30 / 100 - dot / 2, dot, dot), 0,
                         GCornerNone);
      graphics_fill_rect(ctx, GRect(cx, y + digit_h * 70 / 100 - dot / 2, dot, dot), 0,
                         GCornerNone);
      x += colon_w;
    }
  }
  if (seconds) {
    char secs[3];
    snprintf(secs, sizeof(secs), "%02d", t->tm_sec);
    const int16_t small_t = thickness * 70 / 100 > 2 ? thickness * 70 / 100 : 2;
    draw_seven_text(ctx, secs, GPoint(x, y + digit_h - small_h), GSize(small_w, small_h),
                    spacing / 2, small_t);
  }
}

// The bottom label. Three words are set out as on the original: the first at
// the left, the middle one in a box at the centre in the badge colour, and
// the last at the right. Anything else is centred.
static void draw_bottom_label(GContext *ctx, GRect box) {
  const char *text = s_settings.bottom_label;
  char words[3][16];
  int count = 0;
  const char *p = text;
  while (*p && count <= 3) {
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    int length = 0;
    while (p[length] && p[length] != ' ') {
      length++;
    }
    if (count < 3) {
      const int copy = length < 15 ? length : 15;
      memcpy(words[count], p, copy);
      words[count][copy] = '\0';
    }
    count++;
    p += length;
  }
  if (count != 3) {
    draw_label(ctx, text, BOTTOM_FONT, box, BOTTOM_PAD, GTextAlignmentCenter, s_palette.print);
    return;
  }
  draw_label(ctx, words[0], BOTTOM_FONT, box, BOTTOM_PAD, GTextAlignmentLeft, s_palette.print);
  draw_label(ctx, words[2], BOTTOM_FONT, box, BOTTOM_PAD, GTextAlignmentRight, s_palette.print);
  const int16_t width = text_width(words[1], BOTTOM_FONT, BOTTOM_HEIGHT);
  const int16_t centre = box.origin.x + box.size.w / 2;
  const GRect badge = GRect(centre - width / 2 - 4, box.origin.y - 2, width + 8,
                            box.size.h + 4);
  graphics_context_set_stroke_color(ctx, s_palette.badge);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, badge, 2);
  draw_label(ctx, words[1], BOTTOM_FONT, box, BOTTOM_PAD, GTextAlignmentCenter, s_palette.badge);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_unobstructed_bounds(layer);
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, s_palette.body);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // Everything sits in a rectangle: the screen less a margin, or the
  // rectangle that fits inside a round screen.
#if defined(PBL_ROUND)
  const int16_t side = bounds.size.w * 76 / 100;
  const GRect content = GRect(bounds.origin.x + (bounds.size.w - side) / 2,
                              bounds.origin.y + (bounds.size.h - side * 86 / 100) / 2,
                              side, side * 86 / 100);
#else
  const GRect content = grect_inset(bounds, GEdgeInsets(3));
#endif

  // The face plate, framed: the brand and model, a row of small print, the
  // LCD, another row of small print, and the bottom label.
  const int16_t fixed = BRAND_HEIGHT + LABEL_HEIGHT * 2 + BOTTOM_HEIGHT + ROW_GAP * 5
    + FRAME_PAD * 2;
  const int16_t inner_w = content.size.w - FRAME_PAD * 2;
  int16_t lcd_h = content.size.h - fixed;
  if (lcd_h > inner_w * 56 / 100) {
    lcd_h = inner_w * 56 / 100;
  }
  const GRect plate = GRect(content.origin.x,
                            content.origin.y + (content.size.h - fixed - lcd_h) / 2,
                            content.size.w, fixed + lcd_h);
  graphics_context_set_stroke_color(ctx, s_palette.frame);
  graphics_context_set_stroke_width(ctx, FRAME_WIDTH);
  graphics_draw_round_rect(ctx, grect_inset(plate, GEdgeInsets(FRAME_WIDTH / 2)), 5);

  const int16_t x = plate.origin.x + FRAME_PAD;
  int16_t y = plate.origin.y + FRAME_PAD + ROW_GAP / 2;
  const int16_t w = inner_w;

  // The brand at the left and the model at the right, on the same baseline.
  draw_label(ctx, s_settings.brand, BRAND_FONT, GRect(x, y, w, BRAND_HEIGHT), BRAND_PAD,
             GTextAlignmentLeft, s_palette.print);
  draw_label(ctx, s_settings.model, MODEL_FONT,
             GRect(x, y + BRAND_HEIGHT - MODEL_HEIGHT, w, MODEL_HEIGHT), MODEL_PAD,
             GTextAlignmentRight, s_palette.model);
  y += BRAND_HEIGHT + ROW_GAP;

  // LIGHT at the left, by the light button, and the top label centred.
  // The label is centred on the plate where it clears LIGHT, and otherwise in
  // the space after it.
  draw_label(ctx, "LIGHT", LABEL_FONT, GRect(x, y, w, LABEL_HEIGHT), LABEL_PAD,
             GTextAlignmentLeft, s_palette.print);
  const int16_t light_w = text_width("LIGHT", LABEL_FONT, LABEL_HEIGHT) + 4;
  const int16_t label_w = text_width(s_settings.top_label, LABEL_FONT, LABEL_HEIGHT);
  const GRect label_box = label_w / 2 <= w / 2 - light_w
    ? GRect(x, y, w, LABEL_HEIGHT) : GRect(x + light_w, y, w - light_w, LABEL_HEIGHT);
  draw_label(ctx, s_settings.top_label, LABEL_FONT, label_box, LABEL_PAD,
             GTextAlignmentCenter, s_palette.print);
  y += LABEL_HEIGHT + ROW_GAP;

  // The LCD with its thin border.
  const GRect lcd = GRect(x, y, w, lcd_h);
  graphics_context_set_fill_color(ctx, s_palette.trim);
  graphics_fill_rect(ctx, lcd, 2, GCornersAll);
  graphics_context_set_fill_color(ctx, s_palette.lcd);
  graphics_fill_rect(ctx, grect_inset(lcd, GEdgeInsets(TRIM)), 2, GCornersAll);
  draw_lcd(ctx, grect_inset(lcd, GEdgeInsets(TRIM + LCD_PAD)), t);
  y += lcd_h + ROW_GAP;

  // MODE at the left and the alarm button's label at the right.
  draw_label(ctx, "MODE", LABEL_FONT, GRect(x, y, w, LABEL_HEIGHT), LABEL_PAD,
             GTextAlignmentLeft, s_palette.print);
  draw_label(ctx, "ALARM ON-OFF/24HR", LABEL_FONT, GRect(x, y, w, LABEL_HEIGHT), LABEL_PAD,
             GTextAlignmentRight, s_palette.print);
  y += LABEL_HEIGHT + ROW_GAP;

  draw_bottom_label(ctx, GRect(x, y, w, BOTTOM_HEIGHT));
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
  if (!(units_changed & MINUTE_UNIT)) {
    return;
  }
  if (s_settings.chime && tick_time->tm_min == 0 && !quiet_time_is_active()) {
    // The hourly time signal.
    vibes_short_pulse();
  }
  if (weather_wanted() && tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    request_weather();
  }
}

// The seconds cost battery, so the face only ticks every second while they
// show.
static void subscribe_ticks(void) {
  tick_timer_service_subscribe(s_settings.seconds ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_settings.top_row != TOP_CYCLE) {
    return;
  }
  s_info = next_info(s_info);
  persist_write_int(PERSIST_KEY_INFO, s_info);
  layer_mark_dirty(s_canvas);
}

// Clay sends toggles as numbers and dropdown choices as strings.
static int32_t tuple_int(const Tuple *tuple) {
  return tuple->type == TUPLE_CSTRING ? atoi(tuple->value->cstring) : tuple->value->int32;
}

static void read_string(DictionaryIterator *iterator, uint32_t key, char *out, size_t size,
                        bool *changed) {
  const Tuple *tuple = dict_find(iterator, key);
  if (tuple && tuple->type == TUPLE_CSTRING) {
    strncpy(out, tuple->value->cstring, size - 1);
    out[size - 1] = '\0';
    *changed = true;
  }
}

static bool read_settings(DictionaryIterator *iterator) {
  bool changed = false;
  const Tuple *tuple;
  if ((tuple = dict_find(iterator, MESSAGE_KEY_LCD_COLOUR))) {
    s_settings.lcd = tuple_int(tuple) % LCD_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CASE_COLOUR))) {
    s_settings.case_colour = tuple_int(tuple) % CASE_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SECONDS))) {
    s_settings.seconds = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME))) {
    s_settings.chime = tuple_int(tuple) != 0;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT))) {
    s_settings.time_format = tuple_int(tuple) % FORMAT_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TOP_ROW))) {
    s_settings.top_row = tuple_int(tuple) % TOP_COUNT;
    changed = true;
  }
  read_string(iterator, MESSAGE_KEY_BRAND, s_settings.brand, sizeof(s_settings.brand), &changed);
  read_string(iterator, MESSAGE_KEY_MODEL, s_settings.model, sizeof(s_settings.model), &changed);
  read_string(iterator, MESSAGE_KEY_TOP_LABEL, s_settings.top_label,
              sizeof(s_settings.top_label), &changed);
  read_string(iterator, MESSAGE_KEY_BOTTOM_LABEL, s_settings.bottom_label,
              sizeof(s_settings.bottom_label), &changed);
  return changed;
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  const Tuple *temperature = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  const Tuple *condition = dict_find(iterator, MESSAGE_KEY_CONDITION);
  if (temperature) {
    s_temperature = temperature->value->int32;
    persist_write_int(PERSIST_KEY_TEMPERATURE, s_temperature);
  }
  if (condition && condition->type == TUPLE_CSTRING) {
    strncpy(s_condition, condition->value->cstring, sizeof(s_condition) - 1);
    s_condition[sizeof(s_condition) - 1] = '\0';
    persist_write_string(PERSIST_KEY_CONDITION, s_condition);
  }

  const bool wanted_weather = weather_wanted();
  if (read_settings(iterator)) {
    persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
    apply_palette();
    subscribe_ticks();
    if (weather_wanted() && !wanted_weather) {
      request_weather();
    }
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
    if (size == (int)sizeof(Settings) && saved.version == SETTINGS_VERSION) {
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
  if (persist_exists(PERSIST_KEY_INFO)) {
    s_info = persist_read_int(PERSIST_KEY_INFO) % TOP_CYCLE;
  }
}

static void init(void) {
  load_saved_state();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  subscribe_ticks();
  accel_tap_service_subscribe(tap_handler);
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  app_message_register_inbox_received(inbox_received);
  // The phone sends the weather when it starts, if the top row shows it.
  app_message_open(512, 32);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
