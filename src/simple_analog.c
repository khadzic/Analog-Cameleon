#include "simple_analog.h"
#include "pebble.h"

static Window *window;
static Layer *s_date_layer, *s_hands_layer;
static BitmapLayer *s_background_layer;

static GBitmap *s_background_bitmap;
static TextLayer *s_day_label, *s_num_label;

//Weather
static TextLayer *s_temperature_layer;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
    WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
};

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
    switch (key) {
            
        case WEATHER_TEMPERATURE_KEY:
            // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
            text_layer_set_text(s_temperature_layer, new_tuple->value->cstring);
            break;
    }
}
static void request_weather(void) {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    
    if (!iter) {
        // Error creating outbound message
        return;
    }
    
    int value = 1;
    dict_write_int(iter, 1, &value, sizeof(int), true);
    dict_write_end(iter);
    
    app_message_outbox_send();
}

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[4], s_day_buffer[6];

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int16_t second_hand_length = bounds.size.w / 2;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };

  // minute/hour hand
    graphics_context_set_fill_color(ctx, GColorWhite);

    gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
    gpath_draw_filled(ctx, s_minute_arrow);

    gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
    gpath_draw_filled(ctx, s_hour_arrow);
    
    // second hand
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, second_hand, center);
  
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(window));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
    
    // Create GBitmap, then set to created BitmapLayer
    s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
    s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
    bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));

  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);
    

  s_day_label = text_layer_create(GRect(4, 146, 27, 20));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorClear);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(GRect(30, 146, 18, 20));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorClear);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    
    s_temperature_layer = text_layer_create(GRect(60, 146, bounds.size.w, 68));
    text_layer_set_text_color(s_temperature_layer, GColorWhite);
    text_layer_set_background_color(s_temperature_layer, GColorClear);
    text_layer_set_font(s_temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_temperature_layer));


  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
    
    Tuplet initial_values[] = {
        TupletCString(WEATHER_TEMPERATURE_KEY, "1234\u00B0C"),
    };
    
    app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
                  initial_values, ARRAY_LENGTH(initial_values),
                  sync_tuple_changed_callback, sync_error_callback, NULL
                  );
    
    request_weather();
    
}

static void window_unload(Window *window) {

    gbitmap_destroy(s_background_bitmap);
    bitmap_layer_destroy(s_background_layer);
    layer_destroy(s_date_layer);
    text_layer_destroy(s_day_label);
    text_layer_destroy(s_num_label);
    layer_destroy(s_hands_layer);
    text_layer_destroy(s_temperature_layer);
}

static void init() {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }

  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
    app_message_open(64, 64);

}

static void deinit() {
    
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(window);
    app_sync_deinit(&s_sync);

}

int main() {
  init();
  app_event_loop();
  deinit();
}
