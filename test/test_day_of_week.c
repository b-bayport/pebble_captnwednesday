#include <pebble.h>
//#include "../src/c/day_names.h"

#ifndef MESSAGE_KEY_TEMPERATURE
extern uint32_t MESSAGE_KEY_TEMPERATURE;
#endif

static Window *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap *s_bg_bitmap;
static Layer *s_time_ampm_layer;
static TextLayer *s_quote_layer;
static TextLayer *s_date_layer;
static Layer *s_day_layer;
static TextLayer *s_steps_label_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_temp_layer;
static Layer *s_battery_layer;
static Layer *s_steps_line_layer;

static char s_time_buf[6];
static char s_ampm_buf[3];
static char s_date_buf[8];
static char s_day_buf[12];
static char s_steps_buf[8];
static char s_temp_buf[8];

static int s_battery_level;

static GFont s_font_time;
static GFont s_font_large;
static GFont s_font_date;
static GFont s_font_small;

static const char *s_month_names[] = {
  "Jan","Feb","Mar","Apr","May","Jun",
  "Jul","Aug","Sep","Oct","Nov","Dec"
};


// TEMPERATURE HANDLER: receives weather updates from phone; updates temp text layer
static void prv_inbox_handler(DictionaryIterator *iter, void *context) {
  Tuple *temp_t = dict_find(iter, MESSAGE_KEY_TEMPERATURE);
  if (temp_t) {
    snprintf(s_temp_buf, sizeof(s_temp_buf), "%d'F", (int)temp_t->value->int32);
    text_layer_set_text(s_temp_layer, s_temp_buf);
  }
}

// TIME + AM/PM LAYER UPDATE PROC: measures time text width, draws time and AM/PM with 2px spacing
static void prv_time_ampm_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_text_color(ctx, GColorBlack);

  GSize time_size = graphics_text_layout_get_content_size(
    s_time_buf, s_font_time,
    GRect(0, 0, bounds.size.w, bounds.size.h),
    GTextOverflowModeWordWrap, GTextAlignmentLeft);

  int time_x = (bounds.size.w - time_size.w) / 2;
  graphics_draw_text(ctx, s_time_buf, s_font_time,
    GRect(time_x, 0, time_size.w, bounds.size.h),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  int ampm_x = time_x + time_size.w + 2;
  graphics_draw_text(ctx, s_ampm_buf, s_font_small,
    GRect(ampm_x, 0, bounds.size.w - ampm_x, 16),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

// BATTERY LAYER UPDATE PROC: draws 3 horizontal segments; fills segments based on battery level
static void prv_battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int seg_h = bounds.size.h / 3;
  for (int i = 0; i < 3; i++) {
    graphics_context_set_fill_color(ctx, i < s_battery_level ? GColorOrange : GColorWhite);
    graphics_fill_rect(ctx, GRect(0, (2 - i) * seg_h, bounds.size.w, seg_h), 0, GCornerNone);
  }
}

// STEPS LINE LAYER UPDATE PROC: draws a black line to separate steps from time/date
static void prv_steps_line_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// DAY LAYER UPDATE PROC: measures text width, draws white background sized to text, then draws text
static void prv_day_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize text_size = graphics_text_layout_get_content_size(
    s_day_buf, s_font_large, bounds,
    GTextOverflowModeWordWrap, GTextAlignmentLeft);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, 0, text_size.w + 2, bounds.size.h), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_day_buf, s_font_large,
    GRect(0, 0, text_size.w, bounds.size.h),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

// UPDATE ALL: updates time, date, day, and steps based on given tm struct
static void prv_update_all(struct tm *t) {
  // Time
  strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", t);
  strftime(s_ampm_buf, sizeof(s_ampm_buf), "%p", t);
  layer_mark_dirty(s_time_ampm_layer);

  // Date: "1-Jun"
  snprintf(s_date_buf, sizeof(s_date_buf), "%d-%s",
           t->tm_mday, s_month_names[t->tm_mon]);
  text_layer_set_text(s_date_layer, s_date_buf);

  // Day of week: hardcoded to Wednesday for visual verification
  snprintf(s_day_buf, sizeof(s_day_buf), "%s", "Friday");
  layer_mark_dirty(s_day_layer);

  // Steps via Health API
#if defined(PBL_HEALTH)
  HealthValue steps = health_service_sum_today(HealthMetricStepCount);
  snprintf(s_steps_buf, sizeof(s_steps_buf), "%d", (int)steps);
  text_layer_set_text(s_steps_layer, s_steps_buf);
#else
  text_layer_set_text(s_steps_layer, "----");
#endif
}

// BATTERY HANDLER: reads battery percentage, sets s_battery_level (0-3) based on thresholds, and marks battery layer dirty
static void prv_battery_handler(BatteryChargeState state);

// BATTERY INIT CALLBACK: called 100ms after window load to read initial battery state and update battery layer
static void prv_battery_init_callback(void *context) {
  prv_battery_handler(battery_state_service_peek());
}

// TICK HANDLER: called every minute; updates time, date, day, and steps
static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_all(tick_time);
}

// BATTERY HANDLER: reads battery percentage, sets s_battery_level (0-3) based on thresholds, and marks battery layer dirty
static void prv_battery_handler(BatteryChargeState state) {
  int pct = state.charge_percent;
  if (pct >= 90) {
    s_battery_level = 3;
  } else if (pct >= 70) {
    s_battery_level = 2;
  } else if (pct >= 40) {
    s_battery_level = 1;
  } else {
    s_battery_level = 0;
  }
  layer_mark_dirty(s_battery_layer);
}

// WINDOW LOAD: creates layers, loads fonts and images, sets initial text, and seeds time/battery data
static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  s_font_time  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TINTIN_TIME_36));
  s_font_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TINTIN_LARGE_16));
  s_font_date  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TINTIN_DATE_16));
  s_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TINTIN_SMALL_14));

  // Background image: full 200×228 emery screen
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_bg_layer = bitmap_layer_create(GRect(0, 0, 200, 228));
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpAssign);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));


  // Quote: full width, centered
  s_quote_layer = text_layer_create(GRect(0, 22, 200, 20));
  text_layer_set_font(s_quote_layer, s_font_date);
  text_layer_set_text(s_quote_layer, "What a week, huh?");
  text_layer_set_text_alignment(s_quote_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_quote_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_quote_layer));

  // Day-of-week: custom layer; update proc draws white background sized to text then draws text
  s_day_layer = layer_create(GRect(80, 54, 120, 19));
  layer_set_update_proc(s_day_layer, prv_day_update_proc);
  layer_add_child(root, s_day_layer);

  // Battery: custom layer with 3 segments, right-aligned, right edge 2px from screen edge
  s_battery_layer = layer_create(GRect(160, 151, 10, 15));
  layer_set_update_proc(s_battery_layer, prv_battery_update_proc);
  layer_add_child(root, s_battery_layer);

  // Steps label: "steps" right-aligned, right edge 2px from screen edge
  s_steps_label_layer = text_layer_create(GRect(118, 208, 80, 20));
  text_layer_set_font(s_steps_label_layer, s_font_date);
  text_layer_set_text(s_steps_label_layer, "steps");
  text_layer_set_text_alignment(s_steps_label_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_steps_label_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_steps_label_layer));

  // Line above Steps label to separate it from time/date; same width and alignment as label
  s_steps_line_layer = layer_create(GRect(162, 211, 36, 1));
  layer_set_update_proc(s_steps_line_layer, prv_steps_line_update_proc);
  layer_add_child(root, s_steps_line_layer);

  // Steps counter: right-aligned, right edge 2px from screen edge
  s_steps_layer = text_layer_create(GRect(118, 194, 80, 16));
  text_layer_set_font(s_steps_layer, s_font_small);
  text_layer_set_text_alignment(s_steps_layer, GTextAlignmentRight);
  text_layer_set_background_color(s_steps_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_steps_layer));

  // Date: left-aligned, 2px from left edge
  s_date_layer = text_layer_create(GRect(2, 178, 100, 20));
  text_layer_set_font(s_date_layer, s_font_date);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_date_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  // Temperature: left-aligned, 2px from date (so 13px from left edge)
  s_temp_layer = text_layer_create(GRect(13, 198, 100, 20));
  text_layer_set_font(s_temp_layer, s_font_date);
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentLeft);
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_text(s_temp_layer, "--F");
  layer_add_child(root, text_layer_get_layer(s_temp_layer));

  // Time + AM/PM: single custom layer; draw proc measures time width and places AM/PM after it
  s_time_ampm_layer = layer_create(GRect(0, 178, 200, 36));
  layer_set_update_proc(s_time_ampm_layer, prv_time_ampm_update_proc);
  layer_add_child(root, s_time_ampm_layer);

  window_set_background_color(s_window, GColorWhite);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_text_color(s_quote_layer, GColorBlack);
  text_layer_set_text_color(s_steps_label_layer, GColorBlack);
  text_layer_set_text_color(s_steps_layer, GColorBlack);
  text_layer_set_text_color(s_temp_layer, GColorBlack);

  // Seed with current time; defer battery read 100ms to let the service initialize
  time_t now = time(NULL);
  prv_update_all(localtime(&now));
  app_timer_register(100, prv_battery_init_callback, NULL);
}

// WINDOW UNLOAD: destroys layers, unloads fonts and images
static void prv_window_unload(Window *window) {
  bitmap_layer_destroy(s_bg_layer);
  gbitmap_destroy(s_bg_bitmap);
  layer_destroy(s_time_ampm_layer);
  text_layer_destroy(s_quote_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_day_layer);
  text_layer_destroy(s_steps_label_layer);
  text_layer_destroy(s_steps_layer);
  text_layer_destroy(s_temp_layer);
  layer_destroy(s_battery_layer);
  layer_destroy(s_steps_line_layer);
  fonts_unload_custom_font(s_font_time);
  fonts_unload_custom_font(s_font_large);
  fonts_unload_custom_font(s_font_date);
  fonts_unload_custom_font(s_font_small);
}

// INIT: creates window, sets handlers, pushes to stack, subscribes to services, and opens app message inbox
static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  battery_state_service_subscribe(prv_battery_handler);
  app_message_register_inbox_received(prv_inbox_handler);
  app_message_open(64, 64);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
