#include <pebble.h>
#include "pixel_grid.h"
#include "gbitmap_color_palette_manipulator.h"
  
static Window *s_main_window;
static Layer  *s_time_layer, *s_battery_layer, *s_bt_layer, *s_date_layer, *s_temp_layer;

static BitmapLayer *s_bg_layer[2];
static GBitmap *s_bg_bitmap[2];

static BitmapLayer *s_time_digits_layer[7];
static GBitmap *s_time_digits_bitmap[7];

static BitmapLayer *s_date_digits_layer[5];
static GBitmap *s_date_digits_bitmap[5];

static BitmapLayer *s_temp_digits_layer[4];
static GBitmap *s_temp_digits_bitmap[4];

static BitmapLayer *s_day_layer;
static GBitmap *s_day_bitmap;

static BitmapLayer *s_bt_img_layer;
static GBitmap *s_bt_img_bitmap;

static int bt_image_type;
static int temp_scale;
static int date_format;
static bool hide_second_hand;
static bool show_animation;
static int color;
static bool use_large_digits;

static int tap_counter = -1;
static bool clock_ready = false;

const int delta = 40;

static void fillPixel(Layer *layer, int16_t i, int16_t j, GContext *ctx){
  int startX = i * (RECTWIDTH);
  int startY = j * (RECTHEIGHT);

  GRect pixel = GRect(startX, startY, RECTWIDTH-1, RECTHEIGHT-1);
  graphics_fill_rect(ctx, pixel, 0, GCornerNone);
}

static void draw_shape(Layer *layer, GPoint points[], int n, uint8_t startx, uint8_t starty,  GContext *ctx){
  for(int i = 0; i < n; i++){
    fillPixel(layer, startx+points[i].x, starty + points[i].y, ctx);
  }
}

static void setColorShade(float c, uint8_t color, GContext *ctx){ 
  if(c > HI_COLOR_THRESHOLD){
    graphics_context_set_fill_color(ctx, (GColor)COLOR_SETS[color][0]);
  }else if(c > MID_COLOR_THRESHOLD && c <= HI_COLOR_THRESHOLD){
    graphics_context_set_fill_color(ctx, (GColor)COLOR_SETS[color][1]);    
  }else if(c > LO_COLOR_THRESHOLD && c <= MID_COLOR_THRESHOLD){
    graphics_context_set_fill_color(ctx, (GColor)COLOR_SETS[color][2]);
  }else{
    graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  }  
}

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, uint8_t x, uint8_t  y) {
  GBitmap *old_image = *bmp_image;
  *bmp_image = gbitmap_create_with_resource(resource_id);
  
  GPoint origin = { .x = x, .y = y};
  
  GRect frame = (GRect) {
    .origin = origin,
    .size = gbitmap_get_bounds(*bmp_image).size
  };

	bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
	layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);
  
  if (old_image != NULL) {
		gbitmap_destroy(old_image);
		old_image = NULL;
  }        
}

static void set_container_replace_color(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, uint8_t x, uint8_t  y, int color) {

  set_container_image(bmp_image, bmp_layer, resource_id, x, y);
  GColor colors_to_replace[] = {GColorDukeBlue, GColorBlue, GColorBlueMoon};
  GColor replace_colors[] = {(GColor)COLOR_SETS[color][2],(GColor)COLOR_SETS[color][1],(GColor)COLOR_SETS[color][0]};
  replace_gbitmap_colors(colors_to_replace, replace_colors, 3, *bmp_image, bmp_layer);  
}

static BitmapLayer* create_bitmap_layer(GBitmap *bitmap, Layer *window_layer,
                                    int16_t x, int16_t y, int16_t w, int16_t h){
    GRect frame = GRect(x, y, w, h);
    BitmapLayer* bmp_layer = bitmap_layer_create(frame);
    bitmap_layer_set_background_color(bmp_layer,GColorBlack);
    bitmap_layer_set_bitmap(bmp_layer, bitmap);  
    layer_add_child(window_layer, bitmap_layer_get_layer(bmp_layer));  
    return bmp_layer;
}

static void destroy_bitmap_layer( BitmapLayer *layer, GBitmap *bitmap){
    layer_remove_from_parent(bitmap_layer_get_layer(layer));  
    bitmap_layer_destroy(layer);
    if (bitmap != NULL){ 
      gbitmap_destroy(bitmap);
    }
}

static void plot(Layer *layer, uint8_t x, uint8_t y, float c, uint8_t colorset, GContext *ctx){
  setColorShade(c, colorset, ctx);  
  
  if(c > LO_COLOR_THRESHOLD){
    fillPixel(layer, x, y, ctx);
  }  
}

static void swap(uint8_t *i, uint8_t *j) {
   int t = *i;
   *i = *j;
   *j = t;
}

static void update_hours(struct tm* t){
  uint8_t hr = t->tm_hour;

  uint8_t h1 = (((hr+11)%12)+1)/10;
  uint8_t h2 = (((hr+11)%12)+1)%10;
 
  int x = RECTWIDTH;
  int y = 15*RECTWIDTH;
  int n = 6;
  int colonId = RESOURCE_ID_MEDIUMCOLON;

  const int* digitArray  = MED_DIGIT_IMAGE_RESOURCE_IDS;
  if(use_large_digits){
    digitArray = LRG_DIGIT_IMAGE_RESOURCE_IDS;
    n = 7;
    x = 3*RECTWIDTH;
    y = 13*RECTWIDTH;    
    colonId = RESOURCE_ID_LARGECOLON;
  }  
/*	set_container_image(&s_time_digits_bitmap[0], s_time_digits_layer[0], MED_DIGIT_IMAGE_RESOURCE_IDS[color][h1], x, y);  
	set_container_image(&s_time_digits_bitmap[1], s_time_digits_layer[1], MED_DIGIT_IMAGE_RESOURCE_IDS[color][h2], x + 6*RECTWIDTH, y);  
	set_container_image(&s_time_digits_bitmap[2], s_time_digits_layer[2], COLON_RESOURCE_IDS[color], x + 11*RECTWIDTH, y);  
  */
  set_container_replace_color(&s_time_digits_bitmap[0], s_time_digits_layer[0], digitArray[h1], x, y, color);  
	set_container_replace_color(&s_time_digits_bitmap[1], s_time_digits_layer[1], digitArray[h2], x + n*RECTWIDTH, y, color);  
	set_container_replace_color(&s_time_digits_bitmap[2], s_time_digits_layer[2], colonId, x + 2*n*RECTWIDTH, y, color);  	
}

static void update_minutes(struct tm* t){
  uint8_t min = t->tm_min;

  uint8_t m1 = min/10;
  uint8_t m2 = min%10; 
  int x = 16*RECTWIDTH;
  int y = 15*RECTWIDTH;
  int n = 6;
 /* set_container_image(&s_time_digits_bitmap[3], s_time_digits_layer[3], MED_DIGIT_IMAGE_RESOURCE_IDS[color][m1], x + 15*RECTWIDTH, y);  
	set_container_image(&s_time_digits_bitmap[4], s_time_digits_layer[4], MED_DIGIT_IMAGE_RESOURCE_IDS[color][m2], x + 21*RECTWIDTH, y);    
*/
  
  const int* digitArray  = MED_DIGIT_IMAGE_RESOURCE_IDS;
  if(use_large_digits){
    digitArray = LRG_DIGIT_IMAGE_RESOURCE_IDS;
    n = 7;
    x = 21*RECTWIDTH;
    y = 13*RECTWIDTH;    
  }
  set_container_replace_color(&s_time_digits_bitmap[3], s_time_digits_layer[3], digitArray[m1], x, y, color);  
	set_container_replace_color(&s_time_digits_bitmap[4], s_time_digits_layer[4], digitArray[m2], x + n*RECTWIDTH, y, color);       
}


static void time_update_proc(Layer *layer, GContext *ctx) {
 
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  uint8_t sec = t->tm_sec;
  uint8_t s1 = sec/10;
  uint8_t s2 = sec%10;  
  int x = 28*RECTWIDTH;
  int y = 18*RECTWIDTH;
  int n = 4;
  int pm_x = 27;
  int pm_y = 14;
  
  /*set_container_image(&s_time_digits_bitmap[5], s_time_digits_layer[5], SM_DIGIT_IMAGE_RESOURCE_IDS[color][s1], x, y);    
	set_container_image(&s_time_digits_bitmap[6], s_time_digits_layer[6], SM_DIGIT_IMAGE_RESOURCE_IDS[color][s2], x + 4*RECTWIDTH, y);    
  */
  
  const int* digitArray  = SM_DIGIT_IMAGE_RESOURCE_IDS;
  if(use_large_digits){
    digitArray = MED_DIGIT_IMAGE_RESOURCE_IDS;
    n = 6;
    x = 22*RECTWIDTH;
    y = 23*RECTWIDTH;
    pm_x = 23;
    pm_y = 9;
  }  
  
  if(hide_second_hand){
    if(!layer_get_hidden(bitmap_layer_get_layer(s_time_digits_layer[5]))){
      layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layer[5]),true);
      layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layer[6]),true);      
    }
    if(!use_large_digits){
      pm_y += 1;
    }
  }else{
    if(layer_get_hidden(bitmap_layer_get_layer(s_time_digits_layer[5]))){
      layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layer[5]),false);
      layer_set_hidden(bitmap_layer_get_layer(s_time_digits_layer[6]),false);         
    }    
    set_container_replace_color(&s_time_digits_bitmap[5], s_time_digits_layer[5], digitArray[s1], x, y,color);    
	  set_container_replace_color(&s_time_digits_bitmap[6], s_time_digits_layer[6], digitArray[s2], x + n*RECTWIDTH, y,color);    
  }
  
  // Draw PM
  if(t->tm_hour >= 12){  
    graphics_context_set_fill_color(ctx, GColorYellow); 
    draw_shape(layer, PM_POINTS.points, PM_POINTS.num_points, pm_x, pm_y, ctx);  
  }    
  
}

static void battery_update_proc(Layer *layer, GContext *ctx) {  
  BatteryChargeState state = battery_state_service_peek();
  
  uint8_t charge = state.charge_percent;
  GColor charge_color;
  GColor case_color = GColorWhite;  

  if(state.is_plugged){
    case_color = GColorGreen;
  }
  
  if(charge >= BAT_WARN_LEVEL){
    charge_color = GColorGreen;
  }
  else if(charge > BAT_ALERT_LEVEL && charge <BAT_WARN_LEVEL){
    charge_color = GColorYellow;
  }
  else{
    charge_color = GColorRed;
  }    
  
  graphics_context_set_fill_color(ctx, case_color);  
  draw_shape(layer, BAT_CASE_POINTS.points, BAT_CASE_POINTS.num_points, 0, 0, ctx);             

  graphics_context_set_fill_color(ctx, charge_color);         
  for(int i = 0; i < charge/10; i++ ){    
    //battery fill
    fillPixel(layer, i + 1, 1, ctx);    
  }
  
  //Charge icon
  if(state.is_charging){
    graphics_context_set_fill_color(ctx, GColorYellow); 
    draw_shape(layer, CHARGE_POINTS.points, CHARGE_POINTS.num_points, 3, 0, ctx);           
  }
}


static void update_bt_img(bool connected) {  
  if(!connected){
    vibes_short_pulse();
    layer_set_hidden(bitmap_layer_get_layer(s_bt_img_layer), true);      
  }else{
    int bt_id = RESOURCE_ID_BT1;
    if(bt_image_type == BT_IMAGE_LARGE){
      bt_id = RESOURCE_ID_BT2;
    }
    set_container_image(&s_bt_img_bitmap, s_bt_img_layer, bt_id, 0, 0);      
    layer_set_hidden(bitmap_layer_get_layer(s_bt_img_layer), false);      
  }
}

static void update_date(){
  uint8_t x = 0;//(WIDTH-19)*RECTWIDTH;
  uint8_t y = 0;//;(WIDTH + 2)*RECTWIDTH;  
  GPoint origin = layer_get_frame(s_date_layer).origin;
  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  uint8_t month = t->tm_mon;
  uint8_t day = t->tm_mday;
  uint8_t wday = t->tm_wday;

  if(date_format == MMDD_DATE_FORMAT){
    swap(&month,&day);
  }
  uint8_t m1 = (month+1)/10;
  uint8_t m2 = (month+1)%10;
  uint8_t d1 = day/10;
  uint8_t d2 = day%10;
  
	set_container_image(&s_date_digits_bitmap[0], s_date_digits_layer[0], DIGIT_IMAGE_RESOURCE_IDS[d1], x, y);  
	set_container_image(&s_date_digits_bitmap[1], s_date_digits_layer[1], DIGIT_IMAGE_RESOURCE_IDS[d2], x + 4*RECTWIDTH, y);  
	set_container_image(&s_date_digits_bitmap[2], s_date_digits_layer[2], RESOURCE_ID_SLASH, x + 8*RECTWIDTH, y);  
	set_container_image(&s_date_digits_bitmap[3], s_date_digits_layer[3], DIGIT_IMAGE_RESOURCE_IDS[m1], x + 11*RECTWIDTH, y);  
	set_container_image(&s_date_digits_bitmap[4], s_date_digits_layer[4], DIGIT_IMAGE_RESOURCE_IDS[m2], x + 15*RECTWIDTH, y);    

	set_container_image(&s_day_bitmap, s_day_layer, DAY_NAME_IMAGE_RESOURCE_IDS[wday], origin.x+6*RECTWIDTH, origin.y);    
}


static void show_tap_display(bool show){
  //swap visible layers
  layer_set_hidden(bitmap_layer_get_layer(s_day_layer), !show);
  layer_set_hidden(s_date_layer, show); 
  
  layer_set_hidden(s_temp_layer, !show);     
  layer_set_hidden(s_bt_layer, show);  
  layer_set_hidden(s_battery_layer, show);
}

static void request_temperature(){
  // Begin dictionary
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);

  // Send the message!
  app_message_outbox_send();
}


static void bt_handler(bool connected) {
  update_bt_img(connected);
}

static void battery_handler(BatteryChargeState new_state) {
  layer_mark_dirty(s_battery_layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  tap_counter = TAP_DURATION_MED;
  show_tap_display(true);
  layer_mark_dirty(s_time_layer);   
}

static void handle_second_tick(struct tm *t, TimeUnits units_changed) {

  if((!hide_second_hand && (units_changed & SECOND_UNIT))){
    layer_mark_dirty(s_time_layer);
  }
  if(s_time_digits_bitmap[3] == NULL || (units_changed & MINUTE_UNIT)){  
    update_minutes(t);
  }
  if(s_time_digits_bitmap[1] == NULL || (units_changed & HOUR_UNIT)){
    update_hours(t);
  }    
  if(units_changed & DAY_UNIT){
      update_date();
  }
  
  // Get weather update every 30 minutes
  if(t->tm_min % 30 == 0 && t->tm_sec % 60 == 0) {
    request_temperature();
  }  
  
  if(tap_counter >= 0){
    if(tap_counter == 0){
      //switch to timer
      show_tap_display(false);
    }
    tap_counter--;
  }
}

static void parse_config_message(DictionaryIterator *iterator, void *context){
  
  Tuple *t = dict_read_first(iterator);
  
APP_LOG(APP_LOG_LEVEL_ERROR, "configgingg!!");  
    // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_HIDE_SECONDS:
      hide_second_hand = (int)(t->value->int32);
      persist_write_bool(KEY_HIDE_SECONDS,hide_second_hand);
      break;
    case KEY_BT_LOGO_TYPE:
      if((int)t->value->int32){
        bt_image_type = BT_IMAGE_LARGE;
      }else{
        bt_image_type = BT_IMAGE_SMALL;        
      }
      update_bt_img(bluetooth_connection_service_peek());       
      persist_write_int(KEY_BT_LOGO_TYPE, bt_image_type);   
      break;      
    case KEY_TEMP_SCALE:
      if(temp_scale != (int)t->value->int32){
        temp_scale = (int)t->value->int32;
        request_temperature();
      }      
      persist_write_int(KEY_TEMP_SCALE, temp_scale);            
      break;      
    case KEY_LARGE_DIGITS:
      use_large_digits = (int)(t->value->int32);
      persist_write_bool(KEY_LARGE_DIGITS, use_large_digits);
      break;   
    case KEY_SECOND_COLOR:
      color = (int)t->value->int32;
      persist_write_int(KEY_SECOND_COLOR, color);                                       
      break;      
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }
    
    t = dict_read_next(iterator);
  }
  
  time_t now = time(NULL);
  update_minutes(localtime(&now));
  update_hours(localtime(&now));  
  layer_mark_dirty(s_time_layer); 
}

static void parse_weather_message(DictionaryIterator *iterator, void *context){
  int temperature = 0;
  bool got_temperature = false;
  bool neg_temp = false;  
  
      APP_LOG(APP_LOG_LEVEL_ERROR, "Getting weather");
  
  Tuple *t = dict_read_first(iterator);

    // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_TEMPERATURE:
      temperature = (int)t->value->int32;
      if(temp_scale == FAHRENHEIT_SCALE){
        temperature = temperature * 9/5 + 32;
      }
      got_temperature = true;
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }  
  
  APP_LOG(APP_LOG_LEVEL_ERROR, "Temp: %d", temperature);

  if(got_temperature){
    if(temperature < 0){
      temperature = -temperature;
      neg_temp = true;
    }
    
    int t1 = temperature/100;
    int t2 = (temperature%100)/10;
    int t3 = temperature%10;
    
    int x = 0;
    if(t1 != 0){
      set_container_image(&s_temp_digits_bitmap[0], s_temp_digits_layer[0], DIGIT_IMAGE_RESOURCE_IDS[t1], x, 0);  
      x += 4*RECTWIDTH;
    } else if(neg_temp){
      set_container_image(&s_temp_digits_bitmap[0], s_temp_digits_layer[0], RESOURCE_ID_NEGATIVE, x, 0);        
      x += 4*RECTWIDTH;
    } else if(s_temp_digits_bitmap[0] != NULL){
      gbitmap_destroy(s_temp_digits_bitmap[0]);
      s_temp_digits_bitmap[0] = NULL;
    }
    set_container_image(&s_temp_digits_bitmap[1], s_temp_digits_layer[1], DIGIT_IMAGE_RESOURCE_IDS[t2], x, 0);  
    x += 4*RECTWIDTH;  	
    set_container_image(&s_temp_digits_bitmap[2], s_temp_digits_layer[2], DIGIT_IMAGE_RESOURCE_IDS[t3], x, 0);  
    x += 4*RECTWIDTH;  	
    set_container_image(&s_temp_digits_bitmap[3], s_temp_digits_layer[3], RESOURCE_ID_DEGREE, x, 0);          
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {  
  // Read first item
  Tuple *weather_tuple = dict_find(iterator, WEATHER_MESSAGE);
  //Tuple *config_tuple = dict_find(iterator, CONFIG_MESSAGE);
  
  if((int)weather_tuple->value->int32 == 1){
    APP_LOG(APP_LOG_LEVEL_ERROR, "Get weather");
    parse_weather_message(iterator,  context);
  }
  else{
    APP_LOG(APP_LOG_LEVEL_ERROR, "Get config");    
    parse_config_message(iterator,  context);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GRect dummy_frame = { {0, 0}, {0, 0} };
  
  //create bg layers
  s_bg_bitmap[0] = gbitmap_create_with_resource(RESOURCE_ID_BG1);  
  s_bg_layer[0] = create_bitmap_layer(s_bg_bitmap[0], window_layer, 0,0,144,127);
  s_bg_bitmap[1] = gbitmap_create_with_resource(RESOURCE_ID_BG2);   
  s_bg_layer[1] = create_bitmap_layer(s_bg_bitmap[1], window_layer, 0,127,144,41);

  
  //create time layer
  s_time_layer = layer_create(bounds);
  layer_set_update_proc(s_time_layer, time_update_proc);
  layer_add_child(window_layer, s_time_layer);
  
  memset(&s_time_digits_layer, 0, sizeof(s_time_digits_layer));
  
  for (int i = 0; i < 7; ++i) {
    s_time_digits_layer[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(s_time_layer, bitmap_layer_get_layer(s_time_digits_layer[i]));
  }    
  
  //create date layer
  s_date_layer = layer_create(GRect((WIDTH/2-1)*RECTWIDTH, (WIDTH+2)*RECTWIDTH,20*RECTWIDTH,4*RECTWIDTH));
  layer_add_child(window_layer, s_date_layer);
  
  memset(&s_date_digits_layer, 0, sizeof(s_date_digits_layer));
  
  for (int i = 0; i < 5; ++i) {
    s_date_digits_layer[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(s_date_layer, bitmap_layer_get_layer(s_date_digits_layer[i]));
  }
    
  //create day of week layer
  s_day_layer = bitmap_layer_create(GRect((WIDTH/2-1)*RECTWIDTH, (WIDTH+2)*RECTWIDTH,20*RECTWIDTH,4*RECTWIDTH));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_day_layer));
  layer_set_hidden(bitmap_layer_get_layer(s_day_layer), true);
  
  //create temperature layer
  s_temp_layer = layer_create(GRect(2*RECTWIDTH, (WIDTH+2)*RECTWIDTH,16*RECTWIDTH,4*RECTWIDTH));
  layer_add_child(window_layer, s_temp_layer);
  
  memset(&s_temp_digits_layer, 0, sizeof(s_temp_digits_layer));
  
  for (int i = 0; i < 4; ++i) {
    s_temp_digits_layer[i] = bitmap_layer_create(dummy_frame);
    layer_add_child(s_temp_layer, bitmap_layer_get_layer(s_temp_digits_layer[i]));
  }  
  
  layer_set_hidden(s_temp_layer, true);
  
  //create battery layer
  s_battery_layer = layer_create(GRect(RECTWIDTH, (WIDTH+3)*RECTWIDTH,13*RECTWIDTH,3*RECTWIDTH));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_layer, s_battery_layer);
  
  //create bluetooth layer
  s_bt_layer = layer_create(GRect(RECTWIDTH, (WIDTH-4)*RECTWIDTH,7*RECTWIDTH,7*RECTWIDTH));
  layer_add_child(window_layer, s_bt_layer);  

  //Bluetooth img
  s_bt_img_layer = bitmap_layer_create(dummy_frame);
  layer_add_child(s_bt_layer, bitmap_layer_get_layer(s_bt_img_layer)); 
    
  
  //Initial draw of details
  update_date();  
  update_bt_img(bluetooth_connection_service_peek());  

  layer_mark_dirty(window_get_root_layer(s_main_window));
}

static void main_window_unload(Window *window) {
    // Destroy Layers
  
  for(int i = 0; i < 2; i++){
    destroy_bitmap_layer(s_bg_layer[i], s_bg_bitmap[i] );
  } 
  
  for(int i = 0; i < 5; i++){
    destroy_bitmap_layer(s_date_digits_layer[i], s_date_digits_bitmap[i] );    
  }   
  
  for(int i = 0; i < 4; i++){
    destroy_bitmap_layer(s_temp_digits_layer[i], s_temp_digits_bitmap[i] );    
  }  
  
  for(int i = 0; i < 7; i++){
    destroy_bitmap_layer(s_time_digits_layer[i], s_time_digits_bitmap[i] );    
  } 
  
  destroy_bitmap_layer(s_bt_img_layer, s_bt_img_bitmap);   
  destroy_bitmap_layer(s_day_layer, s_day_bitmap);        
  
  layer_destroy(s_time_layer);    
  layer_destroy(s_battery_layer);  
  layer_destroy(s_date_layer);    
  layer_destroy(s_bt_layer);  
  layer_destroy(s_temp_layer); 
  
}


static void init() {
  
  srand(time(NULL));
  
  bt_image_type = BT_IMAGE_SMALL;
  temp_scale = CELSIUS_SCALE; 
  date_format = DDMM_DATE_FORMAT;
  hide_second_hand = false;
  color = WHITE;
  use_large_digits = true;  
  
  if(persist_exists(KEY_SECOND_COLOR)){
    color = persist_read_int(KEY_SECOND_COLOR);
  }  
  if(persist_exists(KEY_TEMP_SCALE)){
    temp_scale = persist_read_int(KEY_TEMP_SCALE);
  }
  if(persist_exists(KEY_BT_LOGO_TYPE)){
      bt_image_type = persist_read_int(KEY_BT_LOGO_TYPE);
  }
  if(persist_exists(KEY_LARGE_DIGITS)){
    use_large_digits = persist_read_bool(KEY_LARGE_DIGITS);
  }  
  if(persist_exists(KEY_HIDE_SECONDS)){
    hide_second_hand = persist_read_bool(KEY_HIDE_SECONDS);
  }  
  /*if(persist_exists(KEY_DATE_FORMAT)){
    date_format = persist_read_int(KEY_DATE_FORMAT);
  }  */
  
  
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Register with Services
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
  accel_tap_service_subscribe(tap_handler);  
  battery_state_service_subscribe(battery_handler);
  bluetooth_connection_service_subscribe(bt_handler);  
  
  

  clock_ready = true;
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}


static void deinit() {
    tick_timer_service_unsubscribe(); 
    accel_tap_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

