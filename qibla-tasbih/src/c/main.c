// Qibla-Tasbih: a dhikr counter.
//
// Each press counts one. The ring fills towards the target, and the watch
// vibrates at the end of each set, so you can count without looking.

#include <pebble.h>

// ---------------------------------------------------------------------------
// Dhikr

typedef enum {
  PHRASE_SUBHANALLAH,
  PHRASE_ALHAMDULILLAH,
  PHRASE_ALLAHU_AKBAR,
  PHRASE_LA_ILAHA_ILLALLAH,
  PHRASE_ASTAGHFIRULLAH,
  PHRASE_SUBHANALLAHI_WA_BIHAMDIHI,
  PHRASE_SALAWAT,
  PHRASE_LA_HAWLA,
  PHRASE_TAHLIL,
  PHRASE_COUNT,
  PHRASE_NONE = PHRASE_COUNT,
} PhraseId;

typedef struct {
  const char *name;      // transliteration, as the screen and the menu show it
  const char *meaning;
  uint32_t image;        // the Arabic, drawn by tools/make_images.py
} Phrase;

static const Phrase PHRASES[PHRASE_COUNT] = {
  [PHRASE_SUBHANALLAH] = {"SubhanAllah", "Glory be to Allah",
                          RESOURCE_ID_IMAGE_SUBHANALLAH},
  [PHRASE_ALHAMDULILLAH] = {"Alhamdulillah", "All praise is for Allah",
                            RESOURCE_ID_IMAGE_ALHAMDULILLAH},
  [PHRASE_ALLAHU_AKBAR] = {"Allahu Akbar", "Allah is the Greatest",
                           RESOURCE_ID_IMAGE_ALLAHU_AKBAR},
  [PHRASE_LA_ILAHA_ILLALLAH] = {"La ilaha illallah", "There is no god but Allah",
                                RESOURCE_ID_IMAGE_LA_ILAHA_ILLALLAH},
  [PHRASE_ASTAGHFIRULLAH] = {"Astaghfirullah", "I seek Allah's forgiveness",
                             RESOURCE_ID_IMAGE_ASTAGHFIRULLAH},
  [PHRASE_SUBHANALLAHI_WA_BIHAMDIHI] = {"SubhanAllahi wa bihamdihi",
                                        "Glory and praise be to Allah",
                                        RESOURCE_ID_IMAGE_SUBHANALLAHI_WA_BIHAMDIHI},
  [PHRASE_SALAWAT] = {"Allahumma salli 'ala Muhammad", "O Allah, bless Muhammad",
                      RESOURCE_ID_IMAGE_SALAWAT},
  [PHRASE_LA_HAWLA] = {"La hawla wa la quwwata illa billah",
                       "No power or strength but through Allah",
                       RESOURCE_ID_IMAGE_LA_HAWLA},
  [PHRASE_TAHLIL] = {"La ilaha illallahu wahdahu la sharika lah",
                     "None is worthy of worship but Allah alone",
                     RESOURCE_ID_IMAGE_TAHLIL},
};

// Targets to choose from; 0 counts on with no target.
static const uint16_t TARGETS[] = {10, 33, 99, 100, 1000, 0};
#define TARGET_COUNT ((int)ARRAY_LENGTH(TARGETS))
enum { T_10, T_33, T_99, T_100, T_1000, T_OFF };

typedef struct {
  const char *title;     // for the menu; NULL uses the phrase's name
  PhraseId phrase;       // PHRASE_NONE for the after-prayer set and free count
  uint8_t target;        // the target it starts with, an index into TARGETS
} Dhikr;

#define AFTER_PRAYER 0
#define DHIKR_COUNT 10

static const Dhikr DHIKRS[DHIKR_COUNT] = {
  {"After prayer", PHRASE_NONE, T_OFF},
  {NULL, PHRASE_SUBHANALLAH, T_33},
  {NULL, PHRASE_ALHAMDULILLAH, T_33},
  {NULL, PHRASE_ALLAHU_AKBAR, T_33},
  {NULL, PHRASE_LA_ILAHA_ILLALLAH, T_100},
  {NULL, PHRASE_ASTAGHFIRULLAH, T_100},
  {NULL, PHRASE_SUBHANALLAHI_WA_BIHAMDIHI, T_100},
  {"Salawat", PHRASE_SALAWAT, T_100},
  {NULL, PHRASE_LA_HAWLA, T_100},
  {"Free count", PHRASE_NONE, T_OFF},
};

// How the after-prayer set ends: Allahu Akbar a 34th time, or 33 times and
// then La ilaha illallahu wahdahu... once. Both make 100.
enum { ENDING_TAKBIR, ENDING_TAHLIL };

#define MAX_STAGES 4

// The stages of a dhikr: the after-prayer set has three or four, the others
// one. Returns how many.
static int stages(int dhikr, uint8_t ending, const uint8_t *targets,
                  PhraseId *phrases, uint32_t *counts) {
  if (dhikr == AFTER_PRAYER) {
    phrases[0] = PHRASE_SUBHANALLAH;
    phrases[1] = PHRASE_ALHAMDULILLAH;
    phrases[2] = PHRASE_ALLAHU_AKBAR;
    counts[0] = counts[1] = 33;
    if (ending == ENDING_TAHLIL) {
      counts[2] = 33;
      phrases[3] = PHRASE_TAHLIL;
      counts[3] = 1;
      return 4;
    }
    counts[2] = 34;
    return 3;
  }
  phrases[0] = DHIKRS[dhikr].phrase;
  counts[0] = TARGETS[targets[dhikr]];
  return 1;
}

// ---------------------------------------------------------------------------
// State

#define STATE_KEY 1
#define STATE_VERSION 1

typedef struct {
  uint8_t version;
  uint8_t dhikr;
  uint8_t ending;
  bool tick;                       // a light tap on every count
  uint8_t targets[DHIKR_COUNT];
  // Presses since the last reset, for each dhikr. Everything on screen is
  // worked out from this, so undo is simply one fewer.
  uint32_t presses[DHIKR_COUNT];
  uint32_t today;
  int32_t today_key;               // the day `today` counts
  uint32_t total;
} State;

static State s_state;
// The presses a reset cleared, so holding Up can bring them back.
static uint32_t s_reset_presses;

static void state_defaults(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.version = STATE_VERSION;
  s_state.dhikr = AFTER_PRAYER;
  s_state.ending = ENDING_TAKBIR;
  for (int i = 0; i < DHIKR_COUNT; i++) {
    s_state.targets[i] = DHIKRS[i].target;
  }
}

static void state_load(void) {
  state_defaults();
  State saved;
  if (persist_read_data(STATE_KEY, &saved, sizeof(saved)) == (int)sizeof(saved) &&
      saved.version == STATE_VERSION) {
    s_state = saved;
  }
  if (s_state.dhikr >= DHIKR_COUNT) {
    s_state.dhikr = AFTER_PRAYER;
  }
  for (int i = 0; i < DHIKR_COUNT; i++) {
    if (s_state.targets[i] >= TARGET_COUNT) {
      s_state.targets[i] = DHIKRS[i].target;
    }
  }
}

static void state_save(void) {
  persist_write_data(STATE_KEY, &s_state, sizeof(s_state));
}

// Saving on every press would wear the flash, so wait for a pause.
static AppTimer *s_save_timer;

static void save_timer_fired(void *data) {
  s_save_timer = NULL;
  state_save();
}

static void schedule_save(void) {
  if (s_save_timer) {
    app_timer_reschedule(s_save_timer, 2000);
  } else {
    s_save_timer = app_timer_register(2000, save_timer_fired, NULL);
  }
}

static int32_t day_key(void) {
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  return local->tm_year * 1000 + local->tm_yday;
}

// Starts today's count afresh after midnight.
static void refresh_today(void) {
  int32_t key = day_key();
  if (key != s_state.today_key) {
    s_state.today_key = key;
    s_state.today = 0;
  }
}

// ---------------------------------------------------------------------------
// What to show for a number of presses

typedef struct {
  PhraseId phrase;
  uint32_t count;      // within the stage
  uint32_t target;     // 0 when there is none
  int stage;
  int stage_count;
  uint32_t round;
  bool complete;       // the last press finished the whole set
} Progress;

static Progress progress_for(int dhikr, uint32_t presses) {
  PhraseId phrases[MAX_STAGES];
  uint32_t counts[MAX_STAGES];
  int n = stages(dhikr, s_state.ending, s_state.targets, phrases, counts);
  Progress p = {.phrase = phrases[0], .target = counts[0], .stage_count = n, .round = 1};

  uint32_t total = 0;
  for (int i = 0; i < n; i++) {
    total += counts[i];
  }
  if (total == 0) {
    p.count = presses;
    return p;
  }
  if (presses == 0) {
    return p;
  }

  p.round = (presses - 1) / total + 1;
  uint32_t within = (presses - 1) % total + 1;
  uint32_t start = 0;
  for (int i = 0; i < n; i++) {
    uint32_t end = start + counts[i];
    if (i == n - 1 || within < end) {
      p.stage = i;
      p.phrase = phrases[i];
      p.target = counts[i];
      p.count = within - start;
      p.complete = within == total;
      return p;
    }
    if (within == end) {
      // This press finished the stage, so show the next one, waiting.
      p.stage = i + 1;
      p.phrase = phrases[i + 1];
      p.target = counts[i + 1];
      p.count = 0;
      return p;
    }
    start = end;
  }
  return p;
}

static Progress current_progress(void) {
  return progress_for(s_state.dhikr, s_state.presses[s_state.dhikr]);
}

// ---------------------------------------------------------------------------
// Layout

typedef struct {
  GRect arabic;        // the Arabic phrase is centred in this
  GRect name;          // the transliteration, on up to two lines
  GRect ring;          // the progress ring
  uint8_t ring_width;
  GRect count;         // the number, centred in this
  GRect bottom;        // set dots or the round, and today's count
} Layout;

static Layout s_layout;
static GFont s_name_font;
static GFont s_name_small_font;
static GFont s_label_font;
static GFont s_bottom_font;

#if PBL_DISPLAY_WIDTH >= 200
#define LARGE_SCREEN 1
#else
#define LARGE_SCREEN 0
#endif

static void layout_init(GRect bounds) {
  const int w = bounds.size.w;
#if defined(PBL_ROUND)
  // The ring runs around the edge, and the rest sits inside it.
  const int inset = LARGE_SCREEN ? 4 : 3;
  s_layout.ring_width = LARGE_SCREEN ? 8 : 6;
  s_layout.ring = grect_inset(bounds, GEdgeInsets(inset));
  s_layout.arabic = LARGE_SCREEN ? GRect((w - 176) / 2, 34, 176, 42)
                                 : GRect((w - 124) / 2, 22, 124, 30);
  s_layout.name = LARGE_SCREEN ? GRect(30, 78, w - 60, 52) : GRect(22, 50, w - 44, 34);
  s_layout.count = LARGE_SCREEN ? GRect(40, 124, w - 80, 56) : GRect(30, 82, w - 60, 46);
  s_layout.bottom = LARGE_SCREEN ? GRect(50, 192, w - 100, 44) : GRect(40, 132, w - 80, 34);
#else
  const int h = bounds.size.h;
  if (LARGE_SCREEN) {
    s_layout.arabic = GRect(6, 3, w - 12, 42);
    s_layout.name = GRect(6, 44, w - 12, 48);
    const int r = 56;
    s_layout.ring = GRect(w / 2 - r, 145 - r, 2 * r, 2 * r);
    s_layout.ring_width = 7;
    s_layout.bottom = GRect(6, h - 26, w - 12, 24);
  } else {
    s_layout.arabic = GRect(4, 2, w - 8, 30);
    s_layout.name = GRect(4, 30, w - 8, 34);
    const int r = 41;
    s_layout.ring = GRect(w / 2 - r, 104 - r, 2 * r, 2 * r);
    s_layout.ring_width = 5;
    s_layout.bottom = GRect(4, h - 20, w - 8, 20);
  }
  // The number sits inside the ring.
  const int inner = s_layout.ring.size.w - 2 * s_layout.ring_width - 6;
  s_layout.count = GRect(w / 2 - inner / 2, s_layout.ring.origin.y + s_layout.ring_width + 3,
                         inner, inner);
#endif

  s_name_font = fonts_get_system_font(LARGE_SCREEN ? FONT_KEY_GOTHIC_24_BOLD
                                                   : FONT_KEY_GOTHIC_18_BOLD);
  s_name_small_font = fonts_get_system_font(LARGE_SCREEN ? FONT_KEY_GOTHIC_18_BOLD
                                                         : FONT_KEY_GOTHIC_14_BOLD);
  s_label_font = fonts_get_system_font(LARGE_SCREEN ? FONT_KEY_GOTHIC_18
                                                    : FONT_KEY_GOTHIC_14);
  s_bottom_font = fonts_get_system_font(LARGE_SCREEN ? FONT_KEY_GOTHIC_18_BOLD
                                                     : FONT_KEY_GOTHIC_14_BOLD);
}

// Number fonts, largest first, with how far below the top of the text box
// their digits start, so the digits can be centred.
typedef struct {
  const char *key;
  int8_t top;
  int8_t height;
} CountFont;

static const CountFont COUNT_FONTS[] = {
#if LARGE_SCREEN
  {FONT_KEY_ROBOTO_BOLD_SUBSET_49, 11, 35},
#endif
  {FONT_KEY_LECO_42_NUMBERS, 9, 30},
  {FONT_KEY_LECO_36_BOLD_NUMBERS, 8, 26},
  {FONT_KEY_LECO_28_LIGHT_NUMBERS, 7, 20},
  {FONT_KEY_LECO_20_BOLD_NUMBERS, 5, 14},
};

// ---------------------------------------------------------------------------
// Drawing

static Window *s_window;
static Layer *s_canvas;
static GBitmap *s_phrase_bitmap;
static PhraseId s_bitmap_phrase = PHRASE_NONE;
static char s_toast[40];
static AppTimer *s_toast_timer;

#define COLOR_RING_TRACK PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define COLOR_RING PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorWhite)
#define COLOR_RING_DONE PBL_IF_COLOR_ELSE(GColorGreen, GColorWhite)
#define COLOR_SOFT PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)

static void load_phrase_bitmap(PhraseId phrase) {
  if (phrase == s_bitmap_phrase) {
    return;
  }
  if (s_phrase_bitmap) {
    gbitmap_destroy(s_phrase_bitmap);
    s_phrase_bitmap = NULL;
  }
  s_bitmap_phrase = phrase;
  if (phrase != PHRASE_NONE) {
    s_phrase_bitmap = gbitmap_create_with_resource(PHRASES[phrase].image);
  }
}

static void draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                      GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, align, NULL);
}

static void draw_phrase(GContext *ctx, const Progress *p) {
  load_phrase_bitmap(p->phrase);
  if (s_phrase_bitmap) {
    GSize size = gbitmap_get_bounds(s_phrase_bitmap).size;
    GRect area = s_layout.arabic;
    GRect at = GRect(area.origin.x + (area.size.w - size.w) / 2,
                     area.origin.y + (area.size.h - size.h) / 2, size.w, size.h);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, s_phrase_bitmap, at);
  }

  const char *name = p->phrase == PHRASE_NONE ? DHIKRS[s_state.dhikr].title
                                              : PHRASES[p->phrase].name;
  // One line in the larger font if it fits, otherwise two in the smaller.
  GRect box = s_layout.name;
  GFont font = s_name_font;
  GSize one_line = graphics_text_layout_get_content_size(
      name, font, GRect(0, 0, box.size.w, 200), GTextOverflowModeWordWrap,
      GTextAlignmentCenter);
  if (one_line.h > box.size.h / 2 + 4) {
    font = s_name_small_font;
  }
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, name, font, box, GTextOverflowModeWordWrap, GTextAlignmentCenter,
                     NULL);
}

static void draw_ring(GContext *ctx, const Progress *p) {
  GRect ring = s_layout.ring;
#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, COLOR_RING_TRACK);
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, s_layout.ring_width, 0,
                       TRIG_MAX_ANGLE);
#else
  // Black and white has no grey, so the track is a thin line.
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_arc(ctx, grect_inset(ring, GEdgeInsets(s_layout.ring_width / 2)),
                    GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);
#endif
  if (p->target == 0 || p->count == 0) {
    return;
  }
  uint32_t count = p->count < p->target ? p->count : p->target;
  int32_t angle = (int32_t)((int64_t)TRIG_MAX_ANGLE * count / p->target);
  graphics_context_set_fill_color(ctx, p->complete ? COLOR_RING_DONE : COLOR_RING);
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, s_layout.ring_width, 0, angle);
}

static void draw_count(GContext *ctx, const Progress *p) {
  static char number[12];
  snprintf(number, sizeof(number), "%lu", (unsigned long)p->count);

  char label[24] = "";
  if (p->complete) {
    snprintf(label, sizeof(label), "Complete");
  } else if (p->target) {
    snprintf(label, sizeof(label), "of %lu", (unsigned long)p->target);
  }

  GRect box = s_layout.count;
  const CountFont *chosen = &COUNT_FONTS[ARRAY_LENGTH(COUNT_FONTS) - 1];
  for (unsigned i = 0; i < ARRAY_LENGTH(COUNT_FONTS); i++) {
    GFont font = fonts_get_system_font(COUNT_FONTS[i].key);
    GSize size = graphics_text_layout_get_content_size(
        number, font, GRect(0, 0, 400, 100), GTextOverflowModeFill, GTextAlignmentCenter);
    if (size.w <= box.size.w) {
      chosen = &COUNT_FONTS[i];
      break;
    }
  }

  // The digits and the label below them, centred together in the box.
  const int label_height = label[0] ? (LARGE_SCREEN ? 20 : 16) : 0;
  const int gap = label[0] ? (LARGE_SCREEN ? 6 : 4) : 0;
  const int block = chosen->height + gap + label_height;
  const int digits_top = box.origin.y + (box.size.h - block) / 2;
  draw_text(ctx, number, fonts_get_system_font(chosen->key),
            GRect(box.origin.x - 20, digits_top - chosen->top, box.size.w + 40, 70),
            GTextAlignmentCenter, GColorWhite);
  if (label[0]) {
    // Gothic's letters start about a third of the way down its box.
    const int label_top = digits_top + chosen->height + gap - (LARGE_SCREEN ? 5 : 4);
    draw_text(ctx, label, s_label_font, GRect(box.origin.x - 20, label_top, box.size.w + 40, 24),
              GTextAlignmentCenter, p->complete ? COLOR_RING_DONE : COLOR_SOFT);
  }
}

// The stages of the after-prayer set as dots: filled once done.
static void draw_dots(GContext *ctx, const Progress *p, GPoint centre) {
  const int radius = LARGE_SCREEN ? 4 : 3;
  const int spacing = radius * 2 + (LARGE_SCREEN ? 7 : 5);
  const int width = (p->stage_count - 1) * spacing;
  for (int i = 0; i < p->stage_count; i++) {
    GPoint at = GPoint(centre.x - width / 2 + i * spacing, centre.y);
    const bool done = p->complete || i < p->stage;
    if (done) {
      graphics_context_set_fill_color(ctx, COLOR_RING);
      graphics_fill_circle(ctx, at, radius);
    } else if (i == p->stage) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_circle(ctx, at, radius);
    } else {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite));
      graphics_fill_circle(ctx, at, PBL_IF_COLOR_ELSE(radius, 1));
    }
  }
}

static void format_number(char *out, size_t size, uint32_t value) {
  if (value >= 1000) {
    snprintf(out, size, "%lu,%03lu", (unsigned long)(value / 1000),
             (unsigned long)(value % 1000));
  } else {
    snprintf(out, size, "%lu", (unsigned long)value);
  }
}

static void draw_bottom(GContext *ctx, const Progress *p) {
  GRect box = s_layout.bottom;
  if (s_toast[0]) {
    draw_text(ctx, s_toast, s_bottom_font, box, GTextAlignmentCenter, GColorWhite);
    return;
  }

  char number[16];
  format_number(number, sizeof(number), s_state.today);
  char today[24];
  snprintf(today, sizeof(today), "Today %s", number);
  char round[24] = "";
  if (p->round > 1) {
    snprintf(round, sizeof(round), "Round %lu", (unsigned long)p->round);
  }
  const bool dots = p->stage_count > 1;
  const int dot_y = LARGE_SCREEN ? 12 : 10;

#if defined(PBL_ROUND)
  // Stacked and centred, where the screen narrows.
  const int line = LARGE_SCREEN ? 20 : 16;
  if (dots) {
    draw_dots(ctx, p, GPoint(box.origin.x + box.size.w / 2, box.origin.y + dot_y - 2));
  } else if (round[0]) {
    draw_text(ctx, round, s_bottom_font, GRect(box.origin.x, box.origin.y - 4, box.size.w, line),
              GTextAlignmentCenter, COLOR_SOFT);
  }
  draw_text(ctx, today, s_bottom_font,
            GRect(box.origin.x, box.origin.y + line - 4, box.size.w, line + 4),
            GTextAlignmentCenter, COLOR_SOFT);
#else
  if (dots) {
    GPoint centre = GPoint(box.origin.x + (LARGE_SCREEN ? 30 : 22), box.origin.y + dot_y);
    draw_dots(ctx, p, centre);
  } else if (round[0]) {
    draw_text(ctx, round, s_bottom_font, GRect(box.origin.x, box.origin.y - 2, box.size.w, 24),
              GTextAlignmentLeft, COLOR_SOFT);
  }
  draw_text(ctx, today, s_bottom_font, GRect(box.origin.x, box.origin.y - 2, box.size.w, 24),
            GTextAlignmentRight, COLOR_SOFT);
#endif
}

static void canvas_update(Layer *layer, GContext *ctx) {
  refresh_today();
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_antialiased(ctx, true);

  const Progress p = current_progress();
  draw_ring(ctx, &p);
  draw_phrase(ctx, &p);
  draw_count(ctx, &p);
  draw_bottom(ctx, &p);
}

static void redraw(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

static void toast_timer_fired(void *data) {
  s_toast_timer = NULL;
  s_toast[0] = '\0';
  redraw();
}

// A short message in place of the bottom row.
static void show_toast(const char *text) {
  strncpy(s_toast, text, sizeof(s_toast) - 1);
  s_toast[sizeof(s_toast) - 1] = '\0';
  if (s_toast_timer) {
    app_timer_reschedule(s_toast_timer, 2000);
  } else {
    s_toast_timer = app_timer_register(2000, toast_timer_fired, NULL);
  }
  redraw();
}

// ---------------------------------------------------------------------------
// Counting

static const uint32_t TICK_SEGMENTS[] = {30};
static const VibePattern TICK = {.durations = TICK_SEGMENTS, .num_segments = 1};

static void count_one(void) {
  refresh_today();
  uint32_t *presses = &s_state.presses[s_state.dhikr];
  (*presses)++;
  s_state.today++;
  s_state.total++;
  s_reset_presses = 0;

  // Vibrate at the end of each set, so the screen need not be watched.
  const Progress p = current_progress();
  if (p.complete) {
    vibes_long_pulse();
  } else if (p.count == 0 && p.target) {
    vibes_double_pulse();
  } else if (p.target == 0 && *presses % 100 == 0) {
    vibes_double_pulse();
  } else if (s_state.tick) {
    vibes_enqueue_custom_pattern(TICK);
  }

  schedule_save();
  redraw();
}

static void undo_one(void) {
  uint32_t *presses = &s_state.presses[s_state.dhikr];
  if (*presses > 0) {
    (*presses)--;
    if (s_state.today > 0) {
      s_state.today--;
    }
    if (s_state.total > 0) {
      s_state.total--;
    }
    show_toast("Undone");
  } else if (s_reset_presses > 0) {
    *presses = s_reset_presses;
    s_reset_presses = 0;
    show_toast("Count restored");
  } else {
    return;
  }
  schedule_save();
  redraw();
}

static void reset_count(void) {
  uint32_t *presses = &s_state.presses[s_state.dhikr];
  if (*presses == 0) {
    return;
  }
  s_reset_presses = *presses;
  *presses = 0;
  vibes_short_pulse();
  show_toast("Reset. Hold Up to undo");
  schedule_save();
  redraw();
}

// ---------------------------------------------------------------------------
// Menu

static Window *s_menu_window;
static MenuLayer *s_menu;

enum { SECTION_DHIKR, SECTION_SETTINGS };
enum { ROW_TARGET, ROW_TICK, ROW_RESET, ROW_TOTALS, SETTINGS_ROWS };

static const char *dhikr_title(int dhikr) {
  return DHIKRS[dhikr].title ? DHIKRS[dhikr].title : PHRASES[DHIKRS[dhikr].phrase].name;
}

static uint16_t menu_sections(MenuLayer *menu, void *data) {
  return 2;
}

static uint16_t menu_rows(MenuLayer *menu, uint16_t section, void *data) {
  return section == SECTION_DHIKR ? DHIKR_COUNT : SETTINGS_ROWS;
}

static int16_t menu_header_height(MenuLayer *menu, uint16_t section, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  const char *title = section == SECTION_DHIKR ? "Dhikr" : "Settings";
#if defined(PBL_ROUND)
  // Centred, as the round screen's edge would cut it at the left.
  GRect bounds = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, COLOR_SOFT);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, -2, bounds.size.w, bounds.size.h + 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  menu_cell_basic_header_draw(ctx, cell, title);
#endif
}

static void target_text(char *out, size_t size, int dhikr) {
  if (dhikr == AFTER_PRAYER) {
    snprintf(out, size, s_state.ending == ENDING_TAKBIR ? "33 + 33 + 34" : "33 + 33 + 33 + 1");
  } else if (TARGETS[s_state.targets[dhikr]] == 0) {
    snprintf(out, size, "No target");
  } else {
    snprintf(out, size, "%u times", TARGETS[s_state.targets[dhikr]]);
  }
}

static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  static char subtitle[64];
  const char *title = "";
  subtitle[0] = '\0';
  const int dhikr = s_state.dhikr;

  if (index->section == SECTION_DHIKR) {
    const int row = index->row;
    title = dhikr_title(row);
    if (DHIKRS[row].phrase != PHRASE_NONE) {
      snprintf(subtitle, sizeof(subtitle), "%s", PHRASES[DHIKRS[row].phrase].meaning);
    } else {
      target_text(subtitle, sizeof(subtitle), row);
    }
  } else {
    switch (index->row) {
      case ROW_TARGET:
        if (dhikr == AFTER_PRAYER) {
          title = "Last of the 100";
          snprintf(subtitle, sizeof(subtitle), "%s",
                   s_state.ending == ENDING_TAKBIR ? "Allahu Akbar again"
                                                   : "La ilaha illallah");
        } else {
          title = "Target";
          target_text(subtitle, sizeof(subtitle), dhikr);
        }
        break;
      case ROW_TICK:
        title = "Vibrate";
        snprintf(subtitle, sizeof(subtitle), "%s",
                 s_state.tick ? "On every count" : "At the end of sets");
        break;
      case ROW_RESET: {
        title = "Reset count";
        char number[16];
        format_number(number, sizeof(number), s_state.presses[dhikr]);
        snprintf(subtitle, sizeof(subtitle), "%s counted", number);
        break;
      }
      case ROW_TOTALS: {
        title = "Totals";
        char today[16], total[16];
        format_number(today, sizeof(today), s_state.today);
        format_number(total, sizeof(total), s_state.total);
        snprintf(subtitle, sizeof(subtitle), "Today %s, all %s", today, total);
        break;
      }
    }
  }
  menu_cell_basic_draw(ctx, cell, title, subtitle, NULL);
}

static void menu_select(MenuLayer *menu, MenuIndex *index, void *data) {
  if (index->section == SECTION_DHIKR) {
    s_state.dhikr = index->row;
    s_reset_presses = 0;
    schedule_save();
    window_stack_pop(true);
    return;
  }
  const int dhikr = s_state.dhikr;
  switch (index->row) {
    case ROW_TARGET:
      if (dhikr == AFTER_PRAYER) {
        s_state.ending = s_state.ending == ENDING_TAKBIR ? ENDING_TAHLIL : ENDING_TAKBIR;
      } else {
        s_state.targets[dhikr] = (s_state.targets[dhikr] + 1) % TARGET_COUNT;
      }
      break;
    case ROW_TICK:
      s_state.tick = !s_state.tick;
      if (s_state.tick) {
        vibes_enqueue_custom_pattern(TICK);
      }
      break;
    case ROW_RESET:
      window_stack_pop(true);
      reset_count();
      return;
    default:
      return;
  }
  schedule_save();
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
#if defined(PBL_COLOR)
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, GColorIslamicGreen, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu, window);
  menu_layer_set_selected_index(s_menu, MenuIndex(SECTION_DHIKR, s_state.dhikr),
                                MenuRowAlignCenter, false);
  layer_add_child(root, menu_layer_get_layer(s_menu));
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
      .unload = menu_window_unload,
    });
  }
  window_stack_push(s_menu_window, true);
}

// ---------------------------------------------------------------------------
// Buttons and touch

static void count_click(ClickRecognizerRef recognizer, void *context) {
  count_one();
}

static void undo_long_click(ClickRecognizerRef recognizer, void *context) {
  undo_one();
}

static void reset_long_click(ClickRecognizerRef recognizer, void *context) {
  reset_count();
}

static void menu_long_click(ClickRecognizerRef recognizer, void *context) {
  open_menu();
}

static void click_config(void *context) {
  // Any of the three buttons counts, so whichever is under your thumb works.
  window_single_click_subscribe(BUTTON_ID_UP, count_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, count_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, count_click);
  window_long_click_subscribe(BUTTON_ID_UP, 600, undo_long_click, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, menu_long_click, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 900, reset_long_click, NULL);
}

#if defined(PBL_TOUCH)
static Recognizer *s_tap;

// A tap anywhere on the screen counts too.
static void tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event == RecognizerEvent_Completed) {
    count_one();
  }
}
#endif

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  layout_init(bounds);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
#if defined(PBL_TOUCH)
  // Taps here are counts, not the system's button presses.
  window_set_touch_bridge_disabled(window, true);
  s_tap = tap_recognizer_create(tap_handler, NULL);
  window_attach_recognizer(window, s_tap);
#endif
}

static void main_window_unload(Window *window) {
#if defined(PBL_TOUCH)
  window_detach_recognizer(window, s_tap);
  recognizer_destroy(s_tap);
  s_tap = NULL;
#endif
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void day_changed(struct tm *tick_time, TimeUnits changed) {
  redraw();
}

static void init(void) {
  state_load();
  refresh_today();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_window, true);

#if defined(PBL_TOUCH)
  // Lets the menu scroll by touch.
  app_touch_navigation_enable(true);
#endif
  // Today's count starts again at midnight.
  tick_timer_service_subscribe(DAY_UNIT, day_changed);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_save_timer) {
    app_timer_cancel(s_save_timer);
  }
  state_save();
  if (s_phrase_bitmap) {
    gbitmap_destroy(s_phrase_bitmap);
  }
  if (s_menu_window) {
    window_destroy(s_menu_window);
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
