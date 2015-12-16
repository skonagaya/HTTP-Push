#include <pebble.h>
#include <stdio.h>
#include <string.h>

#define KEY_LIST 0
#define KEY_SIZE 1
#define KEY_INDEX 2

static Window *s_menu_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_error_text_layer;

static char s_item_text[32];
static char *s_buffer;
static char **theList;
static char *listString;
static int listSize;

enum {
  PERSIST_LIST_SIZE, // Persistent storage key for wakeup_id
  PERSIST_LIST
};

static void send_to_phone() {
  DictionaryIterator *dict;
  app_message_outbox_begin(&dict);
  vibes_short_pulse();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Menu index to send: %d", menu_layer_get_selected_index(s_menu_layer).row);
  dict_write_uint8(dict,KEY_INDEX,(int)menu_layer_get_selected_index(s_menu_layer).row);
  const uint32_t final_size = dict_write_end(dict);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble received message from Phone!");
  Tuple *array_size = dict_find(iter,KEY_SIZE);
  Tuple *array_string = dict_find(iter, KEY_LIST);
  int size_of_array = 0;

  if (array_size){
    size_of_array = array_size->value->int32;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%d", size_of_array);

  }


    // Check it was found. If not, dict_find() returns NULL
  if(array_string) {
    // Get the length of the string
    int length = strlen(array_string->value->cstring);
    char *array[size_of_array];

    // Free any previous data
    if(s_buffer != NULL) {
      free(s_buffer);
    }

    // Allocate exactly the right amount of memory.
    // This is usually the number of elements multiplied by 
    // the size of each element, returned by sizeof()
    s_buffer = (char*)malloc(length * sizeof(char));

    // Copy in the string to the newly allocated buffer
    strcpy(s_buffer, array_string->value->cstring);

    int bytesWritten = persist_write_string(PERSIST_LIST,s_buffer);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (list): %d", bytesWritten);

    int bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,size_of_array);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (size): %d", bytesSizeWritten);

    char * pch;
    int i;
    i = 0;
    printf ("Splitting string \"%s\" into tokens:\n",s_buffer);
    pch = strtok (s_buffer,",");
    while (pch != NULL)
    {
      array[i++] = pch;
      pch = strtok (NULL, ",");
    }

    for (i=0;i<3; ++i) 
      printf("%s\n", array[i]);
    free(s_buffer);

  }
}
static void select_callback(struct MenuLayer *s_menu_layer, MenuIndex *cell_index, 
                            void *callback_context) {
  // If we were displaying s_error_text_layer, remove it and return
  if (!layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    return;
  }
  send_to_phone();

}

static uint16_t get_sections_count_callback(struct MenuLayer *menulayer, uint16_t section_index, 
                                            void *callback_context) {
  return listSize;
}

#ifdef PBL_ROUND
static int16_t get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, 
                                        void *callback_context) {
  return 60;
}
#endif

static void draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, 
                             void *callback_context) {
  char* name = theList[cell_index->row];

  // Using simple space padding between name and s_item_text for appearance of edge-alignment
  snprintf(s_item_text, sizeof(s_item_text), "%s", PBL_IF_ROUND_ELSE("", name));
  menu_cell_basic_draw(ctx, cell_layer, PBL_IF_ROUND_ELSE(name, s_item_text), 
                       PBL_IF_ROUND_ELSE(s_item_text, NULL), NULL);
}

static void menu_window_load(Window *window) {


  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loading Window");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "listSize: %d", listSize);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "listString: %s", listString);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);


  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_sections_count_callback,
    .get_cell_height = PBL_IF_ROUND_ELSE(get_cell_height_callback, NULL),
    .draw_row = draw_row_handler,
    .select_click = select_callback
  }); 
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_error_text_layer = text_layer_create((GRect) { .origin = {0, 44}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_error_text_layer, "Cannot\nschedule");
  text_layer_set_text_alignment(s_error_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_color(s_error_text_layer, GColorWhite);
  text_layer_set_background_color(s_error_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_error_text_layer);
}


static void init(void) {
  if (persist_exists(PERSIST_LIST)){
    char *buf= malloc(255);
    int readByteSize = persist_read_string(PERSIST_LIST,buf,255);
    if (listString != NULL) {
      free(listString);
    }
    listString = malloc(readByteSize);
    memcpy(listString,buf,readByteSize);
    free(buf);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read list from persistent storage: %s", listString);
    listSize = persist_read_int(PERSIST_LIST_SIZE);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read size from persistent storage: %d", listSize);

    if (theList != NULL){
      free(theList);
    }
    theList = malloc(listSize * sizeof(char*));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Size of theList: %d", sizeof(theList));

    char * pch;
    int i;
    i = 0;
    printf ("Splitting string \"%s\" into tokens:\n",listString);
    pch = strtok (listString,",");
    while (pch != NULL)
    {
      theList[i] = malloc((strlen(pch)+1) * sizeof(char));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Size of strlen(pch): %d", strlen(pch));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "[1] Finished allocating");
      strcpy(theList[i++],pch);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "[2] Finished strcpy: %s", theList[i-1]);
      pch = strtok (NULL, ",");
      APP_LOG(APP_LOG_LEVEL_DEBUG, "[3] Finished strtok");
    }

    for (i=0;i<3; ++i) 
      printf("%s\n", theList[i]);
  }
  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  window_stack_push(s_menu_window, false);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
  window_destroy(s_menu_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
