#include <pebble.h>

#define SIZZLE_REEL 0

static Window *s_window;

static TextLayer *s_time_layer;

int sky_x_offset = 0;
static GDrawCommandImage *s_sky_image;
static Layer *s_sky_layer;

int clouds_x_offset = 0;
static GDrawCommandImage *s_clouds_image;
static Layer *s_clouds_layer;

static GPath *s_mountain_path;
static Layer *s_mountain_layer;

static Layer *s_boulder_layer;

// We have to manage this externally because we are using sub-second "frames" for the "fall".
static int boulder_position_index;

static TextLayer *s_date_layer;

// Hacked copy with minimal data points.
static const GPathInfo MOUNTAIN_PATH_INFO = {
  // This is the amount of points
  116,
  // A path can be concave, but it should not twist on itself
  // The points should be defined in clockwise order due to the rendering
  // implementation. Counter-clockwise will work in older firmwares, but
  // it is not officially supported
  (GPoint []) {
    // Bottom left corner.
    { 0, 255}, { 0, 190}, { 19, 190},

    { 20, 189}, { 21, 185}, { 22, 182}, { 23, 179}, { 24, 176},
    { 25, 175}, { 26, 173}, { 27, 170}, { 28, 167}, { 29, 164},
    { 30, 162}, { 31, 160}, { 32, 159}, { 33, 157}, { 34, 154},
    { 35, 151}, { 36, 149}, { 37, 148}, { 38, 147}, { 39, 145},
    { 40, 143}, { 41, 140}, { 42, 138}, { 43, 137}, { 44, 136},
    { 45, 135}, { 46, 134}, { 47, 131}, { 48, 129}, { 49, 128},
    { 50, 128}, { 51, 127}, { 52, 126}, { 53, 124}, { 54, 122},
    { 55, 121}, { 56, 121}, { 57, 121}, { 58, 120}, { 59, 119},
    { 60, 117}, { 61, 116}, { 62, 116}, { 63, 116}, { 64, 116},
    { 65, 115}, { 66, 114}, { 67, 113}, { 68, 113}, { 69, 113},

    { 70, 114}, { 71, 113}, { 72, 112}, { 73, 111}, { 74, 111}, // Peak

    { 75, 112}, { 76, 113}, { 77, 113}, { 78, 112}, { 79, 112},
    { 80, 112}, { 81, 113}, { 82, 114}, { 83, 115}, { 84, 114},
    { 85, 114}, { 86, 114}, { 87, 115}, { 88, 117}, { 89, 118},
    { 90, 118}, { 91, 118}, { 92, 118}, { 93, 119}, { 94, 121},
    { 95, 123}, { 96, 124}, { 97, 124}, { 98, 124}, { 99, 125},
    { 100, 127}, { 101, 130}, { 102, 131}, { 103, 132}, { 104, 132},
    { 105, 134}, { 106, 136}, { 107, 138}, { 108, 140}, { 109, 141},
    { 110, 142}, { 111, 143}, { 112, 146}, { 113, 148}, { 114, 151},
    { 115, 153}, { 116, 154}, { 117, 155}, { 118, 157}, { 119, 160},
    { 120, 163}, { 121, 166}, { 122, 167}, { 123, 169}, { 124, 171},
    { 125, 174}, { 126, 178}, { 127, 181}, { 128, 183}, { 129, 184},

    // Bottom right corner, extended for compatibility with round watches.
    // {129, 190}, {148, 190}, {148, 255}
    {129, 190}, {250, 190}, {250, 255}

  }
};

static int ridge_y_by_second[110] = {
  189,185,182,179,176,175,173,170,167,164,162,160,159,157,154,151,149,148,147,145,143,140,138,137,136,135,134,131,129,128,128,127,126,124,122,121,121,121,120,119,117,116,116,116,116,115,114,113,113,113,114,113,112,111,111,112,113,113,112,112,112,113,114,115,114,114,114,115,117,118,118,118,118,119,121,123,124,124,124,125,127,130,131,132,132,134,136,138,140,141,142,143,146,148,151,153,154,155,157,160,163,166,167,169,171,174,178,181,183,184
};

// The sky offsets are negative, hence all the comparisons are flipped.
#if defined(PBL_COLOR)
  #define DAY_START_SKY_OFFSET -400
  #define DAY_END_SKY_OFFSET -1040
#elif defined(PBL_BW)
  #define DAY_START_SKY_OFFSET -385
  #define DAY_END_SKY_OFFSET -950
#endif

static bool is_day() {
  return ((sky_x_offset < DAY_START_SKY_OFFSET) && (sky_x_offset > DAY_END_SKY_OFFSET));
}

static void tile_image_on_layer(Layer *layer, GContext *ctx, GDrawCommandImage *image, int base_x_offset) {
  // APP_LOG(APP_LOG_LEVEL_INFO, "base_x: %d\n", base_x_offset);

  GRect layer_rect = layer_get_bounds(layer);
  GSize image_size = gdraw_command_image_get_bounds_size(image);

  bool primary_is_onscreen = (base_x_offset >= 0 && (base_x_offset < layer_rect.size.w) ) || ((base_x_offset < 0) && (base_x_offset + image_size.w) > 0);

  if (primary_is_onscreen) {
      // APP_LOG(APP_LOG_LEVEL_INFO, "Drawing Primary Image at %d", base_x_offset);

      GPoint primary_origin = GPoint(base_x_offset, 0);
      gdraw_command_image_draw(ctx, image, primary_origin);
  }

  // TODO: This logic will need to be updated if we ever work with loops smaller than the screen.
  if (base_x_offset != 0) {
    int tile_offset = image_size.w;

    int secondary_x = base_x_offset > 0 ? (base_x_offset - image_size.w) - 2: (base_x_offset + image_size.w) - 2;

    // APP_LOG(APP_LOG_LEVEL_INFO, "Drawing Secondary Image at %d", secondary_x);
    GPoint secondary_origin = GPoint(secondary_x, 0);
    gdraw_command_image_draw(ctx, image, secondary_origin);
  }
}

static void update_sky(Layer *layer, GContext *ctx) {
  tile_image_on_layer(layer, ctx, s_sky_image, sky_x_offset);
}

static void update_clouds(Layer *layer, GContext *ctx) {
  tile_image_on_layer(layer, ctx, s_clouds_image, clouds_x_offset);
}

static void update_mountain(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, s_mountain_path);

  graphics_context_set_stroke_color(ctx, GColorWhite);
  gpath_draw_outline(ctx, s_mountain_path);
}

int base_boulder_x_offset = 19;
int base_boulder_y_offset = -60;

static void update_boulder(Layer *layer, GContext *ctx) {
    int boulder_y_offset = base_boulder_y_offset + ridge_y_by_second[boulder_position_index];
    #if defined(PBL_COLOR)
      graphics_context_set_fill_color(ctx, GColorRed);
    #elif defined(PBL_BW)
      graphics_context_set_fill_color(ctx, GColorLightGray);
    #endif

    graphics_fill_circle(ctx, GPoint(base_boulder_x_offset + boulder_position_index, boulder_y_offset), 6);

    #if defined(PBL_BW)
      GColor stroke_colour = is_day() ? GColorBlack : GColorWhite;
      graphics_context_set_stroke_color(ctx, stroke_colour);
      graphics_draw_circle(ctx, GPoint(base_boulder_x_offset + boulder_position_index, boulder_y_offset), 6);
    #endif

}

// https://github.com/google/pebble/blob/4051c5bb97377f1215bbd26094c1a157532c55fe/src/libc/include/sys/types.h#L27
// `time_t` is just a signed long int;
time_t sizzle_reel_seconds = 1740787200; // 00:00, March 1st, 2025

static struct tm *get_time() {
  struct tm *t;
  if (SIZZLE_REEL) {
    t = localtime(&sizzle_reel_seconds);
  }
  else {
      time_t now = time(NULL);
      t = localtime(&now);
  }
  return t;
}

static void update_time(bool force_update) {
  struct tm *t = get_time();

  // We could use time, but really it only matters if we match the background, so
  // we use sky_x_offset, which also works for the "sizzle reel".
  if (is_day()) {
    text_layer_set_text_color(s_time_layer, GColorBlack);
  }
  else {
    text_layer_set_text_color(s_time_layer, GColorWhite);
  }

  // Only update on the minute.
  if (t->tm_sec == 0 || force_update || SIZZLE_REEL) {
    // Write the current hours and minutes into a buffer
    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer), "%H:%M", t);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, s_buffer);
  }
}

static void update_date(bool force_update) {
  struct tm *t = get_time();

  // Only update on the minute.
  if (t->tm_sec == 0 || force_update || SIZZLE_REEL) {
    // Write the current hours and minutes into a buffer
    static char s_buffer[15];
    strftime(s_buffer, sizeof(s_buffer), "%a, %b %d", t);

    // Display this time on the TextLayer
    text_layer_set_text(s_date_layer, s_buffer);
  }
}

// As this is a large image, we need a more precise percentage to avoid
// "jumping" when we round to an integer.
#define LOOP_PROGRESS_PRECISION 10000

static void update_loop_offsets() {
  // I need to work with the raw time rather than the tick time to get the
  // seconds since the epoch.
  time_t now;

  if (SIZZLE_REEL) {
    now = sizzle_reel_seconds;
  }
  else {
    now = time(NULL);
  }

  int seconds_today = (intmax_t)now % SECONDS_PER_DAY;

  int percent_day_complete = (seconds_today * LOOP_PROGRESS_PRECISION / SECONDS_PER_DAY);

  // APP_LOG(APP_LOG_LEVEL_INFO, " %d percent of the day is complete.\n", percent_day_complete);

  // Animate based on how far we are in the entire day, even if we just started up.
  int new_sky_x_offset = -1 * (percent_day_complete * 1400) / LOOP_PROGRESS_PRECISION;
  // APP_LOG(APP_LOG_LEVEL_INFO, "New sky offset is %d\n", new_sky_x_offset);

  if (new_sky_x_offset != sky_x_offset) {
    // APP_LOG(APP_LOG_LEVEL_INFO, "Redrawing sky at offset of %d.\n", new_sky_x_offset);

    sky_x_offset = new_sky_x_offset;
    layer_mark_dirty(s_sky_layer);
  }

  int seconds_since_last_even_five_minutes = seconds_today % 120;

  // Loop every two minutes.
  int percent_complete = (seconds_since_last_even_five_minutes * LOOP_PROGRESS_PRECISION / 120);
  int new_clouds_x_offset =  740 - ((percent_complete * 740) / LOOP_PROGRESS_PRECISION);

  if (new_clouds_x_offset != clouds_x_offset) {
    // APP_LOG(APP_LOG_LEVEL_INFO, "Redrawing clouds at offset of %d.\n", new_clouds_x_offset);

    clouds_x_offset = new_clouds_x_offset;
    layer_mark_dirty(s_clouds_layer);
  }
}

int subframe = 0;

static void update_boulder_position_index(tm *t) {
  if (t->tm_sec < 55) {
    boulder_position_index = t->tm_min % 2 ? 109 - t->tm_sec : t->tm_sec;
  }
  else {
    int adjusted_frame_offset = (55 + subframe);
    boulder_position_index = t->tm_min % 2 ? 109 - adjusted_frame_offset: adjusted_frame_offset;
  }
}

static void variable_speed_animation_handler () {
  struct tm *t = get_time();

  update_boulder_position_index(t);

  if (t->tm_sec < 55) {
    subframe = 0;
    app_timer_register(1000, variable_speed_animation_handler, NULL);
  }
  else {
    if (subframe < 55) {
      subframe++;

      // In theory this could be 5000 (5s) / 55 (frames) = 90.9 ms, we use a little less so the gap between the next frame is more even.
      app_timer_register(89, variable_speed_animation_handler, NULL);
    }
    else {
      subframe = 0;
      app_timer_register(1000, variable_speed_animation_handler, NULL);
    }

    layer_mark_dirty(s_boulder_layer);
  }
}

// Refresh in order from background to foreground.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_loop_offsets();

  layer_mark_dirty(s_mountain_layer);

  update_time(false);
  update_date(false);

  // In previous efforks, the animation handler would stall out.  If that crops
  // up again, add a heartbeat and relaunch it if it hasn't run recently.
  // app_timer_register(0, variable_speed_animation_handler, NULL);

  if (SIZZLE_REEL) {
    // Needs to be two minutes and one second so that things progress second by second.
    sizzle_reel_seconds += 121;
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_sky_layer = layer_create(bounds);
  layer_set_update_proc(s_sky_layer, update_sky);
  layer_add_child(window_layer, s_sky_layer);

  s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 50));

  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);

  GFont time_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  text_layer_set_font(s_time_layer, time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  update_time(true);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Layer for "clouds"
  s_clouds_layer = layer_create(bounds);
  layer_set_update_proc(s_clouds_layer, update_clouds);
  layer_add_child(window_layer, s_clouds_layer);

  // Layer for pathed "mountain" image.
  s_mountain_layer = layer_create(bounds);
  layer_set_update_proc(s_mountain_layer, update_mountain);
  layer_add_child(window_layer, s_mountain_layer);

  s_boulder_layer = layer_create(bounds);
  layer_set_update_proc(s_boulder_layer, update_boulder);
  layer_add_child(window_layer, s_boulder_layer);

  s_date_layer = text_layer_create(GRect(0, bounds.size.h - 30, bounds.size.w, bounds.size.h));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);

  GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  text_layer_set_font(s_date_layer, date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  update_date(true);

  // Position the elements before the initial draw to avoid "jumps" when we get the actual time.
  update_loop_offsets();

  struct tm *t = get_time();
  update_boulder_position_index(t);

  layer_mark_dirty(s_mountain_layer);
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_date_layer);

  layer_destroy(s_boulder_layer);

  layer_destroy(s_mountain_layer);
  gpath_destroy(s_mountain_path);

  layer_destroy(s_clouds_layer);
  gdraw_command_image_destroy(s_clouds_image);

  layer_destroy(s_sky_layer);
  gdraw_command_image_destroy(s_sky_image);

  text_layer_destroy(s_time_layer);
}

static void prv_init(void) {
  s_sky_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SKY_ROLL);
  s_clouds_image = gdraw_command_image_create_with_resource(RESOURCE_ID_CLOUD_ROLL);

  s_mountain_path = gpath_create(&MOUNTAIN_PATH_INFO);
  gpath_move_to(s_mountain_path, GPoint(-1, -60));

  s_window = window_create();

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = true;
  window_stack_push(s_window, animated);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Start the separate timing mechanism for the variable speed "roll" and
  // "fall" boulder animations.
  app_timer_register(0, variable_speed_animation_handler, NULL);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  app_event_loop();

  prv_deinit();
}
