#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;

static GDrawCommandImage *s_mountain_image;
static GDrawCommandImage *s_background_image;

static Layer *s_background_layer;
static Layer *s_mountain_layer;

static int boulder_y_by_second[60] = {
  160, 158, 156, 154, 152, 150, 148, 146, 144, 142,
  140, 137, 134, 131, 129, 126, 124, 122, 120, 118,
  115, 114, 113, 112, 109, 106, 104, 102,  99,  94,
   92,  92,  90,  88,  86,  84,  82,  79,  77,  74,
   71,  68,  62,  59,  56,  52,  51,  50,  49,  48,
   47,  46,  50,  58,  74,  90, 106, 122, 138, 145
};

static void update_background(Layer *layer, GContext *ctx) {
  gdraw_command_image_draw(ctx, s_background_image, GPoint(0,0));
}

static void update_mountain(Layer *layer, GContext *ctx) {
  // //Diagnostics to test positioning of `boulder_by_second` values.
  // GPoint origin = GPoint(0, 0);
  // gdraw_command_image_draw(ctx, s_mountain_image, origin);
  
  // for (int seconds = 0; seconds < 60; seconds++) {
  //   int boulder_y_offset = boulder_y_by_second[seconds];

  //   graphics_context_set_fill_color(ctx, GColorRed);
  //   graphics_fill_circle(ctx, GPoint(seconds * 2.5, boulder_y_offset), 1);
  // }

  GRect layer_bounds = layer_get_bounds(layer);
  int boulder_x_offset = layer_bounds.size.w / 2;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int primary_x  = boulder_x_offset - (t->tm_sec * 2.5);

  GPoint primary_origin = GPoint(primary_x, 0);
  gdraw_command_image_draw(ctx, s_mountain_image, primary_origin);

  // The secondary image is the one that isn't under the ball.  For seconds
  // 0-30, it's on the left side.  For the rest of the minute, it's on the
  // right.
  int secondary_x = t->tm_sec >= 30 ?  primary_x + 149 : primary_x - 149;
  GPoint secondary_origin = GPoint(secondary_x, 0);
  gdraw_command_image_draw(ctx, s_mountain_image, secondary_origin);

  int boulder_y_offset = boulder_y_by_second[t->tm_sec];

  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_circle(ctx, GPoint(boulder_x_offset, boulder_y_offset), 6);
}

static void update_time(bool force_update) {
  // I've never had awesome luck working directly with tick_time, no idea why.
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // Only update on the minute.
  if (t->tm_sec == 0 || force_update) {
    // Write the current hours and minutes into a buffer
    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer), "%H:%M", t);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, s_buffer);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_mountain_layer);

  update_time(false);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Layer for background mountain range, clouds, and sky.
  s_background_layer = layer_create(bounds);
  layer_set_update_proc(s_background_layer, update_background);
  layer_add_child(window_layer, s_background_layer);



  s_time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 50));

  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);

  GFont time_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  text_layer_set_font(s_time_layer, time_font);

  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  update_time(true);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Layer for animated "Mountain".
  s_mountain_layer = layer_create(bounds);
  layer_set_update_proc(s_mountain_layer, update_mountain);
  layer_add_child(window_layer, s_mountain_layer);

  // Initial draw.
  layer_mark_dirty(s_background_layer);
  layer_mark_dirty(s_mountain_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_background_layer);
  gdraw_command_image_destroy(s_background_image);

  text_layer_destroy(s_time_layer);

  layer_destroy(s_mountain_layer);
  gdraw_command_image_destroy(s_mountain_image);
}

static void prv_init(void) {
  s_background_image = gdraw_command_image_create_with_resource(RESOURCE_ID_BACKGROUND);

  s_mountain_image = gdraw_command_image_create_with_resource(RESOURCE_ID_MOUNTAIN);

  // TODO: Confirm whether this is useful.
  gdraw_command_image_set_bounds_size(s_mountain_image, GSize(25,25));

  s_window = window_create();

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = true;
  window_stack_push(s_window, animated);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  app_event_loop();

  prv_deinit();
}
