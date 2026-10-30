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

// Apps on the Pebble 2 Duo and older watches have a 2 KB stack, and the
// system's text drawing takes most of it. Steps that hold buffers are kept
// out of line so their space is given back before the next step, rather than
// all held at once by the function that calls them; the small helpers that
// pass text on to the system are kept in line, so they add nothing to it.
#define OUT_OF_LINE __attribute__((noinline))
#define IN_LINE inline __attribute__((always_inline))

// Sizes, after a photo of the original. The text is set larger than the
// photo's to stay readable, keeping its proportions to the brand.
#if PBL_DISPLAY_WIDTH >= 200
  #define LABEL_FONT FONT_KEY_GOTHIC_14
  #define LABEL_PAD 4
  #define LABEL_HEIGHT 10
  #define BRAND_FONT FONT_KEY_GOTHIC_24_BOLD
  #define BRAND_PAD 7
  #define BRAND_HEIGHT 17
  // The model name (and the WR) is in a bold condensed italic, IBM Plex Sans
  // Condensed.
  #define MODEL_FONT RESOURCE_ID_FONT_MODEL_LARGE_18
  #define MODEL_PAD 5
  #define MODEL_HEIGHT 13
  #define BOTTOM_FONT FONT_KEY_GOTHIC_18_BOLD
  #define BOTTOM_PAD 5
  #define BOTTOM_HEIGHT 13
  #define PM_FONT FONT_KEY_GOTHIC_14_BOLD
  #define PM_PAD 4
  #define PM_HEIGHT 10
  // The face plate: a blue band, a thin silver line inside it, and blue lines
  // across the plate.
  #define BAND_WIDTH 4
  #define SILVER_INSET 5  // From the middle of the band to the silver line.
  #define LINE_WIDTH 2
  // The box round the middle word of the bottom label.
  #define BOX_PAD 8
  #define BOX_BELOW 4
  #define BOX_TOP_RADIUS 2
  #define BOX_BOTTOM_RADIUS 6
  #define WORD_GAP 5
  // The LCD: a thin outline, a dark gap, then the LCD itself.
  #define LCD_RADIUS 4
  #define LCD_GAP 3
  #define LCD_PAD 5
#else
  #define LABEL_FONT FONT_KEY_GOTHIC_09
  #define LABEL_PAD 2
  #define LABEL_HEIGHT 7
  #define BRAND_FONT FONT_KEY_GOTHIC_18_BOLD
  #define BRAND_PAD 5
  #define BRAND_HEIGHT 13
  #define MODEL_FONT RESOURCE_ID_FONT_MODEL_SMALL_14
  #define MODEL_PAD 4
  #define MODEL_HEIGHT 10
  #define BOTTOM_FONT FONT_KEY_GOTHIC_14
  #define BOTTOM_PAD 4
  #define BOTTOM_HEIGHT 10
  #define PM_FONT FONT_KEY_GOTHIC_09
  #define PM_PAD 2
  #define PM_HEIGHT 7
  #define BAND_WIDTH 2
  #define SILVER_INSET 4
  #define LINE_WIDTH 2
  #define BOX_PAD 5
  #define BOX_BELOW 3
  #define BOX_TOP_RADIUS 1
  #define BOX_BOTTOM_RADIUS 4
  #define WORD_GAP 3
  #define LCD_RADIUS 3
  #define LCD_GAP 2
  #define LCD_PAD 3
#endif

// Proportions measured from the photo, in thousandths of the plate's width
// (taken along the middle of the blue band).
#define CORNER_SIZE 235      // How far each corner's curve reaches along the edges.
#define LINE_INSET 65        // The ends of the blue lines, in from the band.
#define LCD_INSET 99         // The LCD's outline, in from the band.
#define BRAND_CENTRE 310     // The middle of the brand, from the left of the plate.
#define MODEL_CENTRE 695     // The middle of the model name.
// The gaps between the rows, from the top of the plate down.
#define GAP_TO_BRAND 43      // Silver line to the brand.
#define GAP_TO_TOP_LINE 43   // Brand to the top blue line.
#define GAP_TO_LABELS 43     // Top line to LIGHT and MODE.
#define GAP_TO_LCD 22        // LIGHT and MODE to the LCD.
#define GAP_FROM_LCD 18      // LCD to ALARM ON/OFF.
#define GAP_TO_BOTTOM_LINE 25
#define GAP_TO_WORDS 22      // Bottom line to WATER, WR and RESIST.
#define GAP_TO_SILVER 40     // The bottom label to the silver line.
// The LCD's outline is at most this many hundredths as tall as it is wide.
#define LCD_MAX_ASPECT 75

// LCD colours; values match the options in src/pkjs/config.js.
typedef enum {
  LCD_CLASSIC,  // Pale grey-green.
  LCD_BACKLIT,
  LCD_BLUE,
  LCD_AMBER,
  LCD_DARK,
  LCD_GREY,     // Plain grey.
  LCD_CUSTOM,   // The wearer's own colours.
  LCD_COUNT,
} LcdColour;

// Case colours.
typedef enum {
  CASE_BLACK,
  CASE_GOLD,
  CASE_SILVER,
  CASE_CUSTOM,  // The wearer's own colours.
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

// What can show in place of the model name, or the bottom label's first or
// last word.
typedef enum {
  WIDGET_TEXT,  // The printed word itself.
  WIDGET_WEATHER,
  WIDGET_STEPS,
  WIDGET_HEART_RATE,
  WIDGET_BATTERY,
  WIDGET_COUNT,
} Widget;

typedef enum {
  FORMAT_WATCH,  // Whatever the watch is set to.
  FORMAT_12H,
  FORMAT_24H,
  FORMAT_COUNT,
} TimeFormat;

// The wearer's own colours, for a custom case or LCD. The first nine are
// shared: printed text, for example, colours the brand, the button labels and
// the bottom label alike. With separate colours on, the rest take over parts
// of those. Indexes match COLOURS[n] in src/pkjs/config.js.
typedef enum {
  COLOUR_CASE,
  COLOUR_BRAND,   // Printed text; separately, the brand.
  COLOUR_BAND,    // The band, lines and box; separately, the band.
  COLOUR_SILVER,
  COLOUR_MODEL,
  COLOUR_BADGE,   // The boxed word and the arrows; separately, the word.
  COLOUR_TRIM,
  COLOUR_LCD,
  COLOUR_DIGITS,  // Everything on the LCD; separately, the time.
  COLOUR_LABELS,  // The rest only with separate colours.
  COLOUR_WORDS,
  COLOUR_LINES,
  COLOUR_BOX,
  COLOUR_ARROWS,
  COLOUR_TOP_ROW,
  COLOUR_BACKGROUND,
  COLOUR_COUNT,
} ColourPart;

// What the chime plays; values match the options in src/pkjs/config.js.
typedef enum {
  CHIME_SHORT,        // One short buzz.
  CHIME_COUNT_HOURS,  // On the hour, a buzz for each hour; a short buzz between.
  CHIME_CUSTOM,       // The wearer's own buzzes and pauses.
  CHIME_PATTERN_COUNT,
} ChimePattern;

// The most buzzes and pauses a custom pattern keeps, and the longest each.
#define CUSTOM_CHIME_MAX 16
#define CUSTOM_CHIME_MAX_MS 3000

// Saved with persist_write_data, so only append fields and bump the version.
#define SETTINGS_VERSION 7
typedef struct {
  uint8_t version;
  uint8_t lcd;
  uint8_t case_colour;
  char brand[16];
  char model[16];
  // The labels by the Back, Up and Down buttons.
  char light_label[16];
  char up_label[16];
  char down_label[24];
  char bottom_label[32];
  bool seconds;
  bool chime;
  uint8_t time_format;
  uint8_t top_row;
  // What shows in place of the bottom label's first and last words (see
  // Widget).
  uint8_t bottom_left;
  uint8_t bottom_right;
  // What shows in place of the model name (see Widget).
  uint8_t model_widget;
  // Added in version 6.
  bool split_colours;
  uint8_t colours[COLOUR_COUNT];  // GColor8 values, by ColourPart.
  // Added in version 7.
  uint8_t chime_pattern;  // See ChimePattern.
  uint8_t chime_every;    // Minutes between chimes: 60 is on the hour only.
  // The hours the chime sounds between, the last one's :00 included; the
  // same hour for both is all day.
  uint8_t chime_from;
  uint8_t chime_until;
  uint8_t custom_chime_length;
  uint16_t custom_chime[CUSTOM_CHIME_MAX];  // Buzz, pause, buzz... in ms.
} Settings;

// Earlier versions end where the next one's fields begin, and keep their
// choices on update.
static const struct {
  uint8_t version;
  uint8_t size;
} OLD_SETTINGS[] = {
  { 5, offsetof(Settings, split_colours) },
  { 6, offsetof(Settings, chime_pattern) },
};

static Settings s_settings;

typedef struct {
  GColor background; // Round the face plate.
  GColor body;       // The case.
  GColor brand;
  GColor labels;     // The labels by the buttons.
  GColor words;      // The bottom label, but for a boxed middle word.
  GColor band;       // The blue band round the face plate.
  GColor lines;      // The lines across the plate.
  GColor box;        // The box round the middle word of the bottom label.
  GColor silver;     // The thin line inside the band.
  GColor model;      // The model name beside the brand.
  GColor badge;      // The middle word of the bottom label, in its box.
  GColor arrows;     // The arrows to the buttons.
  GColor trim;       // The thin border round the LCD.
  GColor lcd;        // The LCD background.
  GColor digits;     // The time.
  GColor top_row;    // The top row of the LCD and the signs beside it.
} Palette;

static Palette s_palette;
// What lit segments are drawn in: the top row's colour, then the time's.
static GColor s_ink;

// Where the parts of the face go, worked out each time it is drawn. Kept
// static rather than on the stack, so each part is drawn from a shallow
// stack: the system's text drawing goes deep.
static struct {
  GRect plate;            // Measured along the middle of the blue band.
  int16_t corner;         // How far the plate's corners curve along its edges.
  int16_t brand_y;        // The top of the brand.
  int16_t top_line_y;
  int16_t line_left;      // The ends of the blue lines.
  int16_t line_right;
  GRect top_labels;       // LIGHT and MODE, across the LCD's width.
  GRect lcd;              // The LCD's outline.
  GRect down_label;       // ALARM ON/OFF.
  int16_t bottom_line_y;
  int16_t words_y;        // The top of the bottom label.
} s_layout;

static Window *s_window;
static Layer *s_canvas;
static GFont s_model_font;

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
    .light_label = "LIGHT",
    .up_label = "MODE",
    .down_label = "ALARM ON/OFF",
    .bottom_label = "WATER WR RESIST",
    .seconds = false,
    .chime = false,
    .time_format = FORMAT_WATCH,
    .top_row = TOP_DATE,
    // The black case and classic LCD, the starting point for custom colours.
    .colours = {
      [COLOUR_CASE] = GColorBlackARGB8,
      [COLOUR_BRAND] = GColorWhiteARGB8,
      [COLOUR_BAND] = GColorBlueARGB8,
      [COLOUR_SILVER] = GColorLightGrayARGB8,
      [COLOUR_MODEL] = GColorLimerickARGB8,
      [COLOUR_BADGE] = GColorDarkCandyAppleRedARGB8,
      [COLOUR_TRIM] = GColorDarkGrayARGB8,
      [COLOUR_LCD] = GColorMintGreenARGB8,
      [COLOUR_DIGITS] = GColorBlackARGB8,
      [COLOUR_LABELS] = GColorWhiteARGB8,
      [COLOUR_WORDS] = GColorWhiteARGB8,
      [COLOUR_LINES] = GColorBlueARGB8,
      [COLOUR_BOX] = GColorBlueARGB8,
      [COLOUR_ARROWS] = GColorDarkCandyAppleRedARGB8,
      [COLOUR_TOP_ROW] = GColorBlackARGB8,
      [COLOUR_BACKGROUND] = GColorBlackARGB8,
    },
    .chime_pattern = CHIME_SHORT,
    .chime_every = 60,
    .custom_chime_length = 3,
    .custom_chime = { 200, 100, 200 },
  };
}

#if defined(PBL_COLOR)
static GColor custom_colour(ColourPart part) {
  return (GColor) { .argb = s_settings.colours[part] };
}
#endif

static void apply_palette(void) {
#if defined(PBL_COLOR)
  switch (s_settings.case_colour) {
    case CASE_GOLD:
      s_palette.body = GColorChromeYellow;
      s_palette.brand = GColorBlack;
      s_palette.band = GColorBlack;
      s_palette.silver = GColorBlack;
      s_palette.model = GColorBlack;
      s_palette.badge = GColorDarkCandyAppleRed;
      s_palette.trim = GColorBlack;
      break;
    case CASE_SILVER:
      s_palette.body = GColorLightGray;
      s_palette.brand = GColorBlack;
      s_palette.band = GColorDukeBlue;
      s_palette.silver = GColorDarkGray;
      s_palette.model = GColorDukeBlue;
      s_palette.badge = GColorDarkCandyAppleRed;
      s_palette.trim = GColorDarkGray;
      break;
    case CASE_CUSTOM:
      s_palette.body = custom_colour(COLOUR_CASE);
      s_palette.brand = custom_colour(COLOUR_BRAND);
      s_palette.band = custom_colour(COLOUR_BAND);
      s_palette.silver = custom_colour(COLOUR_SILVER);
      s_palette.model = custom_colour(COLOUR_MODEL);
      s_palette.badge = custom_colour(COLOUR_BADGE);
      s_palette.trim = custom_colour(COLOUR_TRIM);
      break;
    default:
      // The colours of the original, chosen by how each looks on the
      // screen next to a photo of it: a blue band and lines, a silver line, an
      // olive-gold model name, and a deep red WR and arrows.
      s_palette.body = GColorBlack;
      s_palette.brand = GColorWhite;
      s_palette.band = GColorBlue;
      s_palette.silver = GColorLightGray;
      s_palette.model = GColorLimerick;
      s_palette.badge = GColorDarkCandyAppleRed;
      s_palette.trim = GColorDarkGray;
      break;
  }
  switch (s_settings.lcd) {
    case LCD_BACKLIT:
      s_palette.lcd = GColorSpringBud;
      s_palette.digits = GColorBlack;
      break;
    case LCD_BLUE:
      s_palette.lcd = GColorCeleste;
      s_palette.digits = GColorOxfordBlue;
      break;
    case LCD_AMBER:
      s_palette.lcd = GColorRajah;
      s_palette.digits = GColorBlack;
      break;
    case LCD_DARK:
      s_palette.lcd = GColorBlack;
      s_palette.digits = GColorWhite;
      break;
    case LCD_GREY:
      s_palette.lcd = GColorLightGray;
      s_palette.digits = GColorBlack;
      break;
    case LCD_CUSTOM:
      s_palette.lcd = custom_colour(COLOUR_LCD);
      s_palette.digits = custom_colour(COLOUR_DIGITS);
      break;
    default:
      // The pale grey-green of the original's LCD.
      s_palette.lcd = GColorMintGreen;
      s_palette.digits = GColorBlack;
      break;
  }
#else
  // Black and white: a black case with white print, and a white LCD (black
  // for the dark LCD).
  s_palette.body = GColorBlack;
  s_palette.brand = GColorWhite;
  s_palette.band = GColorWhite;
  s_palette.silver = GColorWhite;
  s_palette.model = GColorWhite;
  s_palette.badge = GColorWhite;
  s_palette.trim = GColorWhite;
  const bool dark = s_settings.lcd == LCD_DARK;
  s_palette.lcd = dark ? GColorBlack : GColorWhite;
  s_palette.digits = dark ? GColorWhite : GColorBlack;
#endif
  // Parts that share a colour unless the wearer separates them.
  s_palette.background = GColorBlack;
  s_palette.labels = s_palette.brand;
  s_palette.words = s_palette.brand;
  s_palette.lines = s_palette.band;
  s_palette.box = s_palette.band;
  s_palette.arrows = s_palette.badge;
  s_palette.top_row = s_palette.digits;
#if defined(PBL_COLOR)
  if (s_settings.split_colours && s_settings.case_colour == CASE_CUSTOM) {
    s_palette.background = custom_colour(COLOUR_BACKGROUND);
    s_palette.labels = custom_colour(COLOUR_LABELS);
    s_palette.words = custom_colour(COLOUR_WORDS);
    s_palette.lines = custom_colour(COLOUR_LINES);
    s_palette.box = custom_colour(COLOUR_BOX);
    s_palette.arrows = custom_colour(COLOUR_ARROWS);
  }
  if (s_settings.split_colours && s_settings.lcd == LCD_CUSTOM) {
    s_palette.top_row = custom_colour(COLOUR_TOP_ROW);
  }
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
  return s_settings.top_row == TOP_WEATHER || s_settings.top_row == TOP_CYCLE
    || s_settings.bottom_left == WIDGET_WEATHER || s_settings.bottom_right == WIDGET_WEATHER
    || s_settings.model_widget == WIDGET_WEATHER;
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
    graphics_context_set_stroke_color(ctx, s_ink);
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
  graphics_context_set_fill_color(ctx, s_ink);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Each segment's ends, as places across and down a character's box. Across:
// the left, the left after a gap, the centre less a gap, the centre, the
// centre after a gap, the right less a gap, and the right. Down, the same
// from the top through the middle to the bottom.
enum { AT_EDGE, AT_EDGE_GAP, BEFORE_MID, AT_MID, AFTER_MID, AT_END_GAP, AT_END };

// Draws the lit segments of a character in a box. Only lit segments are
// drawn; unlit ones leave the LCD blank.
static OUT_OF_LINE void draw_segments(GContext *ctx, const uint8_t (*ends)[4], int count,
                                      uint16_t lit, GRect box, int16_t thickness,
                                      int16_t gap) {
  const int16_t h = thickness / 2;
  const int16_t left = box.origin.x + h;
  const int16_t right = box.origin.x + box.size.w - 1 - h;
  const int16_t top = box.origin.y + h;
  const int16_t bottom = box.origin.y + box.size.h - 1 - h;
  const int16_t middle = (top + bottom) / 2;
  const int16_t centre = (left + right) / 2;
  const int16_t across[] = {
    left, left + gap, centre - gap, centre, centre + gap, right - gap, right,
  };
  const int16_t down[] = {
    top, top + gap, middle - gap, middle, middle + gap, bottom - gap, bottom,
  };
  for (int i = 0; i < count; i++) {
    if (lit & (1 << i)) {
      draw_lit_segment(ctx, GPoint(across[ends[i][0]], down[ends[i][1]]),
                       GPoint(across[ends[i][2]], down[ends[i][3]]), thickness);
    }
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
  static const uint8_t ENDS[7][4] = {
    { AT_EDGE_GAP, AT_EDGE, AT_END_GAP, AT_EDGE },
    { AT_END, AT_EDGE_GAP, AT_END, BEFORE_MID },
    { AT_END, AFTER_MID, AT_END, AT_END_GAP },
    { AT_EDGE_GAP, AT_END, AT_END_GAP, AT_END },
    { AT_EDGE, AFTER_MID, AT_EDGE, AT_END_GAP },
    { AT_EDGE, AT_EDGE_GAP, AT_EDGE, BEFORE_MID },
    { AT_EDGE_GAP, AT_MID, AT_END_GAP, AT_MID },
  };
  draw_segments(ctx, ENDS, 7, seven_segments(c), box, thickness, thickness / 3 + 1);
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
  static const uint8_t ENDS[14][4] = {
    { AT_EDGE_GAP, AT_EDGE, AT_END_GAP, AT_EDGE },
    { AT_END, AT_EDGE_GAP, AT_END, BEFORE_MID },
    { AT_END, AFTER_MID, AT_END, AT_END_GAP },
    { AT_EDGE_GAP, AT_END, AT_END_GAP, AT_END },
    { AT_EDGE, AFTER_MID, AT_EDGE, AT_END_GAP },
    { AT_EDGE, AT_EDGE_GAP, AT_EDGE, BEFORE_MID },
    { AT_EDGE_GAP, AT_MID, BEFORE_MID, AT_MID },
    { AFTER_MID, AT_MID, AT_END_GAP, AT_MID },
    { AT_EDGE_GAP, AT_EDGE_GAP, BEFORE_MID, BEFORE_MID },
    { AT_MID, AT_EDGE_GAP, AT_MID, BEFORE_MID },
    { AT_END_GAP, AT_EDGE_GAP, AFTER_MID, BEFORE_MID },
    { BEFORE_MID, AFTER_MID, AT_EDGE_GAP, AT_END_GAP },
    { AT_MID, AFTER_MID, AT_MID, AT_END_GAP },
    { AFTER_MID, AFTER_MID, AT_END_GAP, AT_END_GAP },
  };
  draw_segments(ctx, ENDS, 14, fourteen_segments(c), box, thickness, 1);
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

static IN_LINE void draw_text_in(GContext *ctx, const char *text, GFont font, GRect box,
                                  int16_t pad, GTextAlignment alignment, GColor colour) {
  graphics_context_set_text_color(ctx, colour);
  graphics_draw_text(ctx, text, font,
                     GRect(box.origin.x, box.origin.y - pad, box.size.w, box.size.h + pad * 2),
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static IN_LINE void draw_label(GContext *ctx, const char *text, const char *font_key,
                                GRect box, int16_t pad, GTextAlignment alignment,
                                GColor colour) {
  draw_text_in(ctx, text, fonts_get_system_font(font_key), box, pad, alignment, colour);
}

// The hourly signal's sign, a dot between two pairs of brackets: (( . )).
static void draw_chime_sign(GContext *ctx, GPoint at, int16_t size) {
  graphics_context_set_stroke_color(ctx, s_palette.top_row);
  graphics_context_set_fill_color(ctx, s_palette.top_row);
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

// The LCD: a thin outline, a dark gap, then the LCD itself. On it, a narrow
// column of signs at the left (the hourly signal and PM), the top row beside
// it, and the time below across the whole width, with the seconds smaller at
// the bottom right.
static OUT_OF_LINE void draw_lcd(GContext *ctx, const struct tm *t) {
  graphics_context_set_stroke_color(ctx, s_palette.trim);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, s_layout.lcd, LCD_RADIUS);
  const GRect glass = grect_inset(s_layout.lcd, GEdgeInsets(1 + LCD_GAP));
  graphics_context_set_fill_color(ctx, s_palette.lcd);
  graphics_fill_rect(ctx, glass, 2, GCornersAll);

  const GRect area = grect_inset(glass, GEdgeInsets(LCD_PAD));
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
               PM_PAD, GTextAlignmentLeft, s_palette.top_row);
  }

  // The top row: two letters after the signs, and the value at the right.
  const GSize glyph = GSize(row_h * 62 / 100, row_h);
  const int16_t glyph_gap = glyph.w * 30 / 100;
  const int16_t glyph_t = row_h / 8 > 2 ? row_h / 8 : 2;
  // Static, as are the other buffers drawn from, to spare the stack.
  static char letters[4];
  static char value[VALUE_SIZE];
  top_row_text(t, letters, value);
  s_ink = s_palette.top_row;
  draw_fourteen_text(ctx, letters, GPoint(x0 + column_w, y0), glyph, glyph_gap, glyph_t);
  // At least two places, as for the date, and up to five.
  int places = strlen(value);
  places = places < 2 ? 2 : places > 5 ? 5 : places;
  static char shown[VALUE_SIZE];
  snprintf(shown, sizeof(shown), "%*.*s", places, places, value);
  draw_seven_text(ctx, shown, GPoint(x0 + w - places * glyph.w - (places - 1) * glyph_gap, y0),
                  glyph, glyph_gap, glyph_t);

  // The time. Sizes are in 100ths of a large digit's width.
  s_ink = s_palette.digits;
  const bool twenty_four = use_24h();
  const bool seconds = s_settings.seconds;
  const int32_t parts = seconds ? 622 : 502;
  // Digits are up to twice as tall as wide, like the original's, where there
  // is room, and never squatter than 1.75 to 1.
  int16_t digit_w = w * 100 / parts;
  int16_t digit_h = digit_w * 2;
  if (digit_h > time_h) {
    digit_h = time_h;
  }
  if (digit_w * 175 / 100 > digit_h) {
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
  static char digits[5];
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
      graphics_context_set_fill_color(ctx, s_palette.digits);
      graphics_fill_rect(ctx, GRect(cx, y + digit_h * 30 / 100 - dot / 2, dot, dot), 0,
                         GCornerNone);
      graphics_fill_rect(ctx, GRect(cx, y + digit_h * 70 / 100 - dot / 2, dot, dot), 0,
                         GCornerNone);
      x += colon_w;
    }
  }
  if (seconds) {
    static char secs[3];
    snprintf(secs, sizeof(secs), "%02d", t->tm_sec);
    const int16_t small_t = thickness * 70 / 100 > 2 ? thickness * 70 / 100 : 2;
    draw_seven_text(ctx, secs, GPoint(x, y + digit_h - small_h), GSize(small_w, small_h),
                    spacing / 2, small_t);
  }
}

// Fills in what a widget shows in place of a printed word, such as 21°,
// 5,432 STEPS, 72 BPM or 80%. Returns false for the printed word itself.
static bool widget_text(Widget widget, char *out, size_t size) {
  switch (widget) {
    case WIDGET_WEATHER:
      if (s_temperature == NO_TEMPERATURE) {
        snprintf(out, size, "--\xc2\xb0");
      } else {
        snprintf(out, size, "%d\xc2\xb0", (int)s_temperature);
      }
      return true;
#if defined(PBL_HEALTH)
    case WIDGET_STEPS: {
      const int steps = health_service_sum_today(HealthMetricStepCount);
      if (steps >= 1000) {
        snprintf(out, size, "%d,%03d", steps / 1000, steps % 1000);
      } else {
        snprintf(out, size, "%d", steps);
      }
      return true;
    }
    case WIDGET_HEART_RATE: {
      const HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
      if (bpm > 0) {
        snprintf(out, size, "%d BPM", (int)bpm);
      } else {
        snprintf(out, size, "-- BPM");
      }
      return true;
    }
#endif
    case WIDGET_BATTERY:
      snprintf(out, size, "%d%%", battery_state_service_peek().charge_percent);
      return true;
    default:
      return false;
  }
}

// A blue line across the plate, from one x to another.
static void draw_line_across(GContext *ctx, int16_t left, int16_t right, int16_t y) {
  graphics_context_set_fill_color(ctx, s_palette.lines);
  graphics_fill_rect(ctx, GRect(left, y, right - left, LINE_WIDTH), 0, GCornerNone);
}

// A slim solid arrow pointing left or right, its tip at a point, the way the
// original points its labels at the buttons.
static void draw_arrow(GContext *ctx, GPoint tip, bool left, int16_t length, int16_t width) {
  const int16_t back = tip.x + (left ? 1 : -1) * length;
  GPoint points[] = {
    tip, GPoint(back, tip.y - width / 2), GPoint(back, tip.y + width / 2),
  };
  GPathInfo info = { .num_points = ARRAY_LENGTH(points), .points = points };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, s_palette.arrows);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// A button's label in a row of small print, at the left or right edge, with
// an arrow pointing past it to the button on that side.
static void draw_button_label(GContext *ctx, const char *text, GRect row, bool left) {
  if (!text[0]) {
    return;
  }
  // Longer than the text is tall, and about half as wide.
  const int16_t length = LABEL_HEIGHT * 13 / 10;
  const int16_t width = LABEL_HEIGHT * 6 / 10 > 4 ? LABEL_HEIGHT * 6 / 10 : 4;
  const int16_t centre_y = row.origin.y + LABEL_HEIGHT / 2;
  const int16_t text_inset = length + 3;
  if (left) {
    draw_arrow(ctx, GPoint(row.origin.x, centre_y), true, length, width);
    draw_label(ctx, text, LABEL_FONT, GRect(row.origin.x + text_inset, row.origin.y,
                                            row.size.w - text_inset, LABEL_HEIGHT),
               LABEL_PAD, GTextAlignmentLeft, s_palette.labels);
  } else {
    draw_arrow(ctx, GPoint(row.origin.x + row.size.w - 1, centre_y), false, length, width);
    draw_label(ctx, text, LABEL_FONT, GRect(row.origin.x, row.origin.y,
                                            row.size.w - text_inset, LABEL_HEIGHT),
               LABEL_PAD, GTextAlignmentRight, s_palette.labels);
  }
}

// The corner curve of the face plate, measured from a photo of the original:
// a quarter of a "squircle" (|x|^2.65 + |y|^2.65 = 1), from the side of the
// plate round to its top. Each pair is how far a point lies in from the side
// and down from the top, in thousandths of the corner's size.
static const int16_t CORNER_CURVE[][2] = {
  { 0, 1000 }, { 9, 753 }, { 37, 588 }, { 83, 449 }, { 148, 330 }, { 230, 230 },
  { 330, 148 }, { 449, 83 }, { 588, 37 }, { 753, 9 }, { 1000, 0 },
};

// A quarter circle, the same way round, for the WR box's corners.
static const int16_t QUARTER_CIRCLE[][2] = {
  { 0, 1000 }, { 76, 617 }, { 293, 293 }, { 617, 76 }, { 1000, 0 },
};

// How far in from the side a corner of the given size curves, at a distance
// down from the top (or up from the bottom).
static int16_t corner_in(int16_t size, int16_t down) {
  if (size <= 0 || down >= size) {
    return 0;
  }
  const int32_t d = down * 1000 / size;
  for (int i = 1; i < (int)ARRAY_LENGTH(CORNER_CURVE); i++) {
    const int32_t d0 = CORNER_CURVE[i - 1][1];
    const int32_t d1 = CORNER_CURVE[i][1];
    if (d <= d0 && d >= d1) {
      const int32_t x0 = CORNER_CURVE[i - 1][0];
      const int32_t x1 = CORNER_CURVE[i][0];
      return (x0 + (x1 - x0) * (d0 - d) / (d0 - d1)) * size / 1000;
    }
  }
  return size;
}

#define MAX_CURVE_STEPS 11

// Outlines a rectangle whose top and bottom corners follow a curve, each of
// its own size, along the middle of a line of the given width, and fills it
// first unless the fill is clear.
static void draw_rounded_shape(GContext *ctx, GRect rect, const int16_t (*curve)[2],
                               int steps, int16_t top, int16_t bottom, int16_t width,
                               GColor fill, GColor colour) {
  const int16_t l = rect.origin.x;
  const int16_t t = rect.origin.y;
  const int16_t r = rect.origin.x + rect.size.w - 1;
  const int16_t b = rect.origin.y + rect.size.h - 1;
  // Static rather than on the stack, which is short on older watches.
  static GPoint points[4 * MAX_CURVE_STEPS];
  int n = 0;
  // Clockwise from the left side: the top left corner, then the top right,
  // bottom right and bottom left.
  for (int i = 0; i < steps; i++) {
    points[n++] = GPoint(l + curve[i][0] * top / 1000, t + curve[i][1] * top / 1000);
  }
  for (int i = steps - 1; i >= 0; i--) {
    points[n++] = GPoint(r - curve[i][0] * top / 1000, t + curve[i][1] * top / 1000);
  }
  for (int i = 0; i < steps; i++) {
    points[n++] = GPoint(r - curve[i][0] * bottom / 1000, b - curve[i][1] * bottom / 1000);
  }
  for (int i = steps - 1; i >= 0; i--) {
    points[n++] = GPoint(l + curve[i][0] * bottom / 1000, b - curve[i][1] * bottom / 1000);
  }
  GPathInfo info = { .num_points = n, .points = points };
  GPath *path = gpath_create(&info);
  if (!gcolor_equal(fill, GColorClear)) {
    graphics_context_set_fill_color(ctx, fill);
    gpath_draw_filled(ctx, path);
  }
  graphics_context_set_stroke_color(ctx, colour);
  graphics_context_set_stroke_width(ctx, width);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

// Draws text centred on an x, kept between two limits.
static IN_LINE void draw_centred_on(GContext *ctx, const char *text, GFont font,
                                     int16_t centre, int16_t min_x, int16_t max_x, int16_t y,
                                     int16_t height, int16_t pad, GColor colour) {
  const int16_t width = graphics_text_layout_get_content_size(
    text, font, GRect(0, 0, max_x - min_x, height + pad * 2), GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft).w;
  int16_t x = centre - width / 2;
  if (x + width > max_x) {
    x = max_x - width;
  }
  if (x < min_x) {
    x = min_x;
  }
  draw_text_in(ctx, text, font, GRect(x, y, max_x - x, height), pad, GTextAlignmentLeft, colour);
}

static OUT_OF_LINE void lay_out(GRect bounds) {
  // The face plate is measured along the middle of its blue band. It fills
  // the screen, its band a pixel from the edges, or on a round screen the
  // largest square that its curved corners let fit inside the circle.
#if defined(PBL_ROUND)
  const int16_t side = bounds.size.w * 77 / 100;
  const GRect area = GRect(bounds.origin.x + (bounds.size.w - side) / 2,
                           bounds.origin.y + (bounds.size.h - side) / 2, side, side);
#else
  const GRect area = grect_inset(bounds, GEdgeInsets(1 + BAND_WIDTH / 2));
#endif
  const int16_t pw = area.size.w;
  // A proportion of the plate's width, in thousandths, and at least 2 pixels.
  #define SCALED(thousandths) (pw * (thousandths) / 1000 > 2 ? pw * (thousandths) / 1000 : 2)
  int16_t gap_brand = SCALED(GAP_TO_BRAND);
  int16_t gap_top_line = SCALED(GAP_TO_TOP_LINE);
  int16_t gap_labels = SCALED(GAP_TO_LABELS);
  int16_t gap_lcd = SCALED(GAP_TO_LCD);
  int16_t gap_from_lcd = SCALED(GAP_FROM_LCD);
  int16_t gap_bottom_line = SCALED(GAP_TO_BOTTOM_LINE);
  int16_t gap_words = SCALED(GAP_TO_WORDS);
  int16_t gap_silver = SCALED(GAP_TO_SILVER);
  const int16_t lcd_inset = SCALED(LCD_INSET);
  const int16_t line_inset = SCALED(LINE_INSET);
  const int16_t corner = SCALED(CORNER_SIZE);
  #undef SCALED

  // The rows have fixed heights and the LCD takes the rest, up to a limit.
  // Any height beyond that is shared among the gaps, so the plate always fills
  // its space.
  const int16_t rows = SILVER_INSET * 2 + BRAND_HEIGHT + LINE_WIDTH * 2 + LABEL_HEIGHT * 2
    + BOTTOM_HEIGHT;
  int16_t *const gaps[] = {
    &gap_brand, &gap_top_line, &gap_labels, &gap_lcd, &gap_from_lcd, &gap_bottom_line,
    &gap_words, &gap_silver,
  };
  int16_t gap_total = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(gaps); i++) {
    gap_total += *gaps[i];
  }
  const int16_t lcd_w = pw - lcd_inset * 2;
  int16_t lcd_h = area.size.h - rows - gap_total;
  const int16_t lcd_max = lcd_w * LCD_MAX_ASPECT / 100;
  if (lcd_h > lcd_max) {
    const int16_t spare = lcd_h - lcd_max;
    int16_t shared = 0;
    for (size_t i = 0; i < ARRAY_LENGTH(gaps); i++) {
      const int16_t share = spare * *gaps[i] / gap_total;
      *gaps[i] += share;
      shared += share;
    }
    // What the shares leave over, from rounding, stays with the LCD.
    lcd_h -= shared;
  }

  // Down the plate: the brand, the top line, the labels by Back and Up, the
  // LCD, the label by Down, the bottom line and the label below it.
  const int16_t left = area.origin.x;
  s_layout.plate = area;
  s_layout.corner = corner;
  int16_t y = area.origin.y + SILVER_INSET + gap_brand;
  s_layout.brand_y = y;
  y += BRAND_HEIGHT + gap_top_line;
  s_layout.top_line_y = y;
  s_layout.line_left = left + line_inset;
  s_layout.line_right = left + pw - line_inset;
  y += LINE_WIDTH + gap_labels;
  s_layout.top_labels = GRect(left + lcd_inset, y, lcd_w, LABEL_HEIGHT);
  y += LABEL_HEIGHT + gap_lcd;
  s_layout.lcd = GRect(left + lcd_inset, y, lcd_w, lcd_h);
  y += lcd_h + gap_from_lcd;
  s_layout.down_label = GRect(left + lcd_inset, y, lcd_w, LABEL_HEIGHT);
  y += LABEL_HEIGHT + gap_bottom_line;
  s_layout.bottom_line_y = y;
  s_layout.words_y = y + LINE_WIDTH + gap_words;
}

// The case inside the blue band, the band, and the thin silver line inside
// it.
static OUT_OF_LINE void draw_case(GContext *ctx) {
  const GRect plate = s_layout.plate;
  const int16_t corner = s_layout.corner;
  draw_rounded_shape(ctx, plate, CORNER_CURVE, ARRAY_LENGTH(CORNER_CURVE), corner, corner,
                     BAND_WIDTH, s_palette.body, s_palette.band);
  draw_rounded_shape(ctx, grect_inset(plate, GEdgeInsets(SILVER_INSET)), CORNER_CURVE,
                     ARRAY_LENGTH(CORNER_CURVE), corner - SILVER_INSET, corner - SILVER_INSET,
                     1, GColorClear, s_palette.silver);
}

// The brand over the left half and the model name (or a widget in its
// place) over the right, each centred where the original has them, clear
// of the curved corners.
static OUT_OF_LINE void draw_brand_row(GContext *ctx) {
  const GRect plate = s_layout.plate;
  const int16_t left = plate.origin.x;
  const int16_t right = left + plate.size.w;
  const int16_t middle = left + plate.size.w / 2;
  const int16_t y = s_layout.brand_y;
  const int16_t clear = corner_in(s_layout.corner, y - plate.origin.y) + SILVER_INSET + 3;
  draw_centred_on(ctx, s_settings.brand, fonts_get_system_font(BRAND_FONT),
                  left + plate.size.w * BRAND_CENTRE / 1000, left + clear, middle - 2, y,
                  BRAND_HEIGHT, BRAND_PAD, s_palette.brand);
  static char model[16];
  if (!widget_text(s_settings.model_widget, model, sizeof(model))) {
    strncpy(model, s_settings.model, sizeof(model));
  }
  draw_centred_on(ctx, model, s_model_font, left + plate.size.w * MODEL_CENTRE / 1000,
                  middle + 2, right - clear, y + (BRAND_HEIGHT - MODEL_HEIGHT) / 2, MODEL_HEIGHT,
                  MODEL_PAD, s_palette.model);
}

// The bottom row: a blue line across the plate with the bottom label below it.
// Three words are set out as on the original: the middle one, in the italic,
// sits in a box whose top meets the line, which runs into the box's sides,
// and the first and last sit either side of the box. Anything else is centred
// under an unbroken line. The first and last places can show a widget instead
// of their word.
static OUT_OF_LINE void draw_bottom_row(GContext *ctx) {
  const GRect plate = s_layout.plate;
  const int16_t line_left = s_layout.line_left;
  const int16_t line_right = s_layout.line_right;
  const int16_t line_y = s_layout.bottom_line_y;
  const int16_t words_y = s_layout.words_y;
  const char *text = s_settings.bottom_label;
  static char words[3][16];
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
  static char left[16];
  static char right[16];
  const bool left_widget = widget_text(s_settings.bottom_left, left, sizeof(left));
  const bool right_widget = widget_text(s_settings.bottom_right, right, sizeof(right));
  if (count != 3) {
    const GRect row = GRect(line_left, words_y, line_right - line_left, BOTTOM_HEIGHT);
    draw_line_across(ctx, line_left, line_right, line_y);
    draw_label(ctx, text, BOTTOM_FONT, row, BOTTOM_PAD, GTextAlignmentCenter, s_palette.words);
    if (left_widget) {
      draw_label(ctx, left, BOTTOM_FONT, row, BOTTOM_PAD, GTextAlignmentLeft, s_palette.words);
    }
    if (right_widget) {
      draw_label(ctx, right, BOTTOM_FONT, row, BOTTOM_PAD, GTextAlignmentRight, s_palette.words);
    }
    return;
  }

  // The box: its top border just above the line, its bottom a little below
  // the words, with small corners at the top and larger ones at the bottom.
  const int16_t word_w = graphics_text_layout_get_content_size(
    words[1], s_model_font, GRect(0, 0, plate.size.w, MODEL_HEIGHT * 3),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter).w;
  const int16_t centre = plate.origin.x + plate.size.w / 2;
  const int16_t box_top = line_y - 1;
  const GRect box = GRect(centre - word_w / 2 - BOX_PAD, box_top, word_w + BOX_PAD * 2,
                          words_y + BOTTOM_HEIGHT + BOX_BELOW - box_top);
  const int16_t box_right = box.origin.x + box.size.w;
  draw_line_across(ctx, line_left, box.origin.x, line_y);
  draw_line_across(ctx, box_right, line_right, line_y);
  draw_rounded_shape(ctx, box, QUARTER_CIRCLE, ARRAY_LENGTH(QUARTER_CIRCLE), BOX_TOP_RADIUS,
                     BOX_BOTTOM_RADIUS, 1, GColorClear, s_palette.box);
  draw_text_in(ctx, words[1], s_model_font, GRect(box.origin.x, words_y, box.size.w, BOTTOM_HEIGHT),
               MODEL_PAD, GTextAlignmentCenter, s_palette.badge);

  // The words either side, close to the box.
  const int16_t plate_right = plate.origin.x + plate.size.w;
  draw_label(ctx, left_widget ? left : words[0], BOTTOM_FONT,
             GRect(plate.origin.x, words_y, box.origin.x - WORD_GAP - plate.origin.x,
                   BOTTOM_HEIGHT),
             BOTTOM_PAD, GTextAlignmentRight, s_palette.words);
  draw_label(ctx, right_widget ? right : words[2], BOTTOM_FONT,
             GRect(box_right + WORD_GAP, words_y, plate_right - box_right - WORD_GAP,
                   BOTTOM_HEIGHT),
             BOTTOM_PAD, GTextAlignmentLeft, s_palette.words);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const time_t now = time(NULL);
  const struct tm *t = localtime(&now);
  lay_out(layer_get_unobstructed_bounds(layer));

  graphics_context_set_antialiased(ctx, true);
  // Round the face plate: black, unless the wearer chose otherwise.
  graphics_context_set_fill_color(ctx, s_palette.background);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  draw_case(ctx);
  draw_brand_row(ctx);

  // The top blue line.
  draw_line_across(ctx, s_layout.line_left, s_layout.line_right, s_layout.top_line_y);

  // Labels by the Pebble's buttons, their arrows level with the LCD's
  // outline: LIGHT by Back at the top left, and MODE by Up at the top right.
  draw_button_label(ctx, s_settings.light_label, s_layout.top_labels, true);
  draw_button_label(ctx, s_settings.up_label, s_layout.top_labels, false);

  draw_lcd(ctx, t);

  // ALARM ON/OFF by Down at the bottom right.
  draw_button_label(ctx, s_settings.down_label, s_layout.down_label, false);

  // The bottom blue line and the label below it.
  draw_bottom_row(ctx);
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

// Plays the chime for a time: the chosen pattern, except that counting the
// hours only counts on the hour (in 12-hour time, so at most 12 buzzes) and
// gives one short buzz between.
static void play_chime(const struct tm *t) {
  static uint32_t durations[2 * 12];
  uint32_t length = 0;
  if (s_settings.chime_pattern == CHIME_COUNT_HOURS && t->tm_min == 0) {
    const int hours = t->tm_hour % 12 ? t->tm_hour % 12 : 12;
    for (int i = 0; i < hours; i++) {
      durations[length++] = 150;
      durations[length++] = 300;
    }
    length--;  // No pause after the last buzz.
  } else if (s_settings.chime_pattern == CHIME_CUSTOM && s_settings.custom_chime_length) {
    for (int i = 0; i < s_settings.custom_chime_length; i++) {
      durations[length++] = s_settings.custom_chime[i];
    }
  }
  if (length) {
    vibes_enqueue_custom_pattern((VibePattern) { .durations = durations, .num_segments = length });
  } else {
    vibes_short_pulse();
  }
}

// Whether the chime sounds at this time: on its interval, within its hours,
// and not during Quiet Time.
static bool chime_due(const struct tm *t) {
  if (!s_settings.chime || t->tm_min % s_settings.chime_every != 0 || quiet_time_is_active()) {
    return false;
  }
  const int now = t->tm_hour * 60 + t->tm_min;
  const int from = s_settings.chime_from * 60;
  const int until = s_settings.chime_until * 60;
  if (from == until) {
    return true;
  }
  // A range such as 22:00 to 07:00 runs past midnight.
  return from < until ? now >= from && now <= until : now >= from || now <= until;
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
  if (!(units_changed & MINUTE_UNIT)) {
    return;
  }
  if (chime_due(tick_time)) {
    play_chime(tick_time);
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

static void battery_handler(BatteryChargeState state) {
  if (s_settings.bottom_left == WIDGET_BATTERY || s_settings.bottom_right == WIDGET_BATTERY
      || s_settings.model_widget == WIDGET_BATTERY) {
    layer_mark_dirty(s_canvas);
  }
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

// Reads a pattern typed as numbers separated by anything else, such as
// "200, 100, 200": buzz, pause, buzz, in milliseconds. Returns whether it
// changed.
static bool read_custom_chime(const char *text) {
  uint16_t pattern[CUSTOM_CHIME_MAX];
  int length = 0;
  for (const char *p = text; *p && length < CUSTOM_CHIME_MAX; ) {
    if (*p < '0' || *p > '9') {
      p++;
      continue;
    }
    int32_t ms = 0;
    while (*p >= '0' && *p <= '9') {
      if (ms <= CUSTOM_CHIME_MAX_MS) {
        ms = ms * 10 + (*p - '0');
      }
      p++;
    }
    pattern[length++] = ms < 10 ? 10 : ms > CUSTOM_CHIME_MAX_MS ? CUSTOM_CHIME_MAX_MS : ms;
  }
  // A pause at the end does nothing.
  if (length % 2 == 0 && length > 0) {
    length--;
  }
  if (length == s_settings.custom_chime_length &&
      memcmp(pattern, s_settings.custom_chime, length * sizeof(pattern[0])) == 0) {
    return false;
  }
  memcpy(s_settings.custom_chime, pattern, length * sizeof(pattern[0]));
  s_settings.custom_chime_length = length;
  return true;
}

// Settings that change how the chime feels, so it plays once when one does.
static bool s_chime_changed;

static bool read_settings(DictionaryIterator *iterator) {
  bool changed = false;
  s_chime_changed = false;
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
    const bool chime = tuple_int(tuple) != 0;
    s_chime_changed |= chime && !s_settings.chime;
    s_settings.chime = chime;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT))) {
    s_settings.time_format = tuple_int(tuple) % FORMAT_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_MODEL_WIDGET))) {
    s_settings.model_widget = tuple_int(tuple) % WIDGET_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BOTTOM_LEFT))) {
    s_settings.bottom_left = tuple_int(tuple) % WIDGET_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_BOTTOM_RIGHT))) {
    s_settings.bottom_right = tuple_int(tuple) % WIDGET_COUNT;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_TOP_ROW))) {
    s_settings.top_row = tuple_int(tuple) % TOP_COUNT;
    changed = true;
  }
#if defined(PBL_COLOR)
  if ((tuple = dict_find(iterator, MESSAGE_KEY_SPLIT_COLOURS))) {
    s_settings.split_colours = tuple_int(tuple) != 0;
    changed = true;
  }
  // Clay sends each colour as 0xRRGGBB.
  for (int part = 0; part < COLOUR_COUNT; part++) {
    if ((tuple = dict_find(iterator, MESSAGE_KEY_COLOURS + part))) {
      s_settings.colours[part] = GColorFromHEX(tuple_int(tuple)).argb;
      changed = true;
    }
  }
#endif
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME_PATTERN))) {
    const uint8_t pattern = tuple_int(tuple) % CHIME_PATTERN_COUNT;
    s_chime_changed |= pattern != s_settings.chime_pattern;
    s_settings.chime_pattern = pattern;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME_CUSTOM)) && tuple->type == TUPLE_CSTRING) {
    s_chime_changed |= read_custom_chime(tuple->value->cstring) &&
                       s_settings.chime_pattern == CHIME_CUSTOM;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME_EVERY))) {
    const int32_t every = tuple_int(tuple);
    // Only steps that divide the hour, so each hour chimes alike.
    s_settings.chime_every = every > 0 && every <= 60 && 60 % every == 0 ? every : 60;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME_FROM))) {
    s_settings.chime_from = tuple_int(tuple) % 24;
    changed = true;
  }
  if ((tuple = dict_find(iterator, MESSAGE_KEY_CHIME_UNTIL))) {
    s_settings.chime_until = tuple_int(tuple) % 24;
    changed = true;
  }
  read_string(iterator, MESSAGE_KEY_BRAND, s_settings.brand, sizeof(s_settings.brand), &changed);
  read_string(iterator, MESSAGE_KEY_MODEL, s_settings.model, sizeof(s_settings.model), &changed);
  read_string(iterator, MESSAGE_KEY_LIGHT_LABEL, s_settings.light_label,
              sizeof(s_settings.light_label), &changed);
  read_string(iterator, MESSAGE_KEY_UP_LABEL, s_settings.up_label,
              sizeof(s_settings.up_label), &changed);
  read_string(iterator, MESSAGE_KEY_DOWN_LABEL, s_settings.down_label,
              sizeof(s_settings.down_label), &changed);
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
    if (s_settings.chime && s_chime_changed) {
      // Play a new pattern once, on the hour's form, so the wearer can feel it.
      const time_t now = time(NULL);
      struct tm hour = *localtime(&now);
      hour.tm_min = 0;
      play_chime(&hour);
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

// Out of line, so the copy of the settings read here is given back before
// the face runs, rather than held by main the whole time.
static OUT_OF_LINE void load_saved_state(void) {
  settings_set_defaults(&s_settings);
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings saved;
    const int size = persist_read_data(PERSIST_KEY_SETTINGS, &saved, sizeof(saved));
    if (size == (int)sizeof(Settings) && saved.version == SETTINGS_VERSION) {
      s_settings = saved;
    }
    for (size_t i = 0; i < ARRAY_LENGTH(OLD_SETTINGS); i++) {
      if (size == OLD_SETTINGS[i].size && saved.version == OLD_SETTINGS[i].version) {
        // Keep the earlier choices; the newer fields keep their defaults.
        memcpy(&s_settings, &saved, size);
        s_settings.version = SETTINGS_VERSION;
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
  if (persist_exists(PERSIST_KEY_INFO)) {
    s_info = persist_read_int(PERSIST_KEY_INFO) % TOP_CYCLE;
  }
}

static void init(void) {
  load_saved_state();
  s_model_font = fonts_load_custom_font(resource_get_handle(MODEL_FONT));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  subscribe_ticks();
  accel_tap_service_subscribe(tap_handler);
  battery_state_service_subscribe(battery_handler);
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
  battery_state_service_unsubscribe();
  window_destroy(s_window);
  fonts_unload_custom_font(s_model_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
