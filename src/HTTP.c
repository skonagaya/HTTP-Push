#include <pebble.h>

#define KEY_LIST 0
#define KEY_SIZE 1
#define KEY_INDEX 2
#define KEY_RESPONSE 3
#define KEY_ACTION 4

static Window *s_menu_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_error_text_layer, *s_loading_text_layer;

static char s_item_text[32];
static char *s_buffer = NULL;
static char *listAction = NULL;
static char **theList = NULL;
static char **statusList = NULL;
static char **listBuffer = NULL;
static char *listString = NULL;
static int responseIndex = -1;
static int listSize = 0;
static bool loaded = false;
static char *errorText = NULL;

enum {
  PERSIST_LIST_SIZE, // Persistent storage key for wakeup_id
  PERSIST_LIST
};

static void send_to_phone() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing data to send to Phone");

  if (listSize == 0) return;
  DictionaryIterator *dict;
  app_message_outbox_begin(&dict);

  int indexToSend = (int) menu_layer_get_selected_index(s_menu_layer).row;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "statusList[indexToSend] set to: %s", statusList[indexToSend]);
  
  if (strcmp(statusList[indexToSend],"Ready") != 0 &&
     strcmp(statusList[indexToSend],"Pending...") != 0) {
    free (statusList[indexToSend]);
    statusList[indexToSend] = NULL;
   APP_LOG(APP_LOG_LEVEL_DEBUG, "?Preparing data to send to Phone");
  }
  menu_layer_reload_data(s_menu_layer);
  statusList[indexToSend] = "Pending...";
  APP_LOG(APP_LOG_LEVEL_DEBUG, "statusList[%d] set to Waiting...", indexToSend);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Menu index to send: %d", indexToSend);
  dict_write_uint8(dict,KEY_INDEX,indexToSend);
  const uint32_t final_size = dict_write_end(dict);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void free_all_data() {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Starting free all procedure");

  APP_LOG(APP_LOG_LEVEL_DEBUG, "The listSize is %d", listSize);
  
  if (theList != NULL){
    if (listSize != 0) {
      size_t i = 0;
      for (size_t i=0; i < sizeof(theList) / sizeof(theList[0]); ++i){
        if (theList[i] != NULL){
          free(theList[i]);
          theList[i] = NULL;
        }
        if (statusList[i] != NULL) {
          free(statusList[i]);
          statusList[i] = NULL;
        }
      }
    }
    free(theList);
    theList = NULL;
  }

  if (listString != NULL) {
    free(listString);
    listString = NULL;
  }
  //if (s_buffer != NULL)
    //free(s_buffer);
}

static void update_menu_data(char *newString, int stringSize) {



  free_all_data();

  if (stringSize == 0) {return;}


  if (newString == NULL) {
    char *buf= malloc(255);
    int readByteSize = persist_read_string(PERSIST_LIST,buf,255);
    listString = malloc(readByteSize);
    memcpy(listString,buf,readByteSize);
    free(buf);
    buf = NULL;

    listSize = persist_read_int(PERSIST_LIST_SIZE);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read list from persistent storage: %s", listString);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read size (value) from persistent storage: %d", listSize);

  } else {
    listSize = stringSize;
    listString = malloc((strlen(newString)+1) * sizeof(char));
    memcpy(listString, newString, (strlen(newString)+1)*sizeof(char));
  }

  theList = malloc(listSize * sizeof(char*));
  statusList = malloc(listSize * sizeof(char*));

  char * pch = NULL;
  int i;
  i = 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Splitting string \"%s\" into tokens:\n",listString);
  pch = strtok (listString,",");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "strtok'd");

  while (pch != NULL)
  {
    theList[i] = malloc((strlen(pch)+1) * sizeof(char)); // Add extra for end char
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Malloc'd");
    statusList[i] = "Ready";
    memcpy(theList[i++],pch,(strlen(pch)+1) * sizeof(char));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Mem copied");
    pch = strtok (NULL, ",");
  }

  for (i=0;i<listSize; ++i) 
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d]: %s",i, theList[i]);
  pch = NULL;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Completed free all procedure");

  
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  if (!loaded) {
    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Maybe its not this?");
    loaded = true;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Maybe its this?");
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pebble received message from Phone!");

  Tuple *action_string = dict_find(iter, KEY_ACTION);
  int length = 0;

  if (action_string) {

    length = strlen(action_string->value->cstring);

    if (listAction != NULL) {
      free(listAction);
      listAction = NULL;
    }

    listAction = (char*)malloc((length+1) * sizeof(char));
    memcpy(listAction,action_string->value->cstring,(length+1) * sizeof(char));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received action of: \"%s\"", listAction);
  }

  if (strcmp(listAction, "response")==0){

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received a http response from Phone!");
    vibes_short_pulse();

    Tuple *response_string = dict_find(iter, KEY_RESPONSE);
    Tuple *array_index = dict_find(iter,KEY_INDEX);

    int index_of_array = 0;

    if (array_index){ index_of_array = array_index->value->int32; }

    length = strlen(response_string->value->cstring);

    if (statusList[index_of_array] != NULL) {
      //free(statusList[index_of_array] );
      statusList[index_of_array]  = NULL;
    }
    statusList[index_of_array] = (char*)malloc((length+1) * sizeof(char));
    memcpy(statusList[index_of_array],response_string->value->cstring,(length +1) * sizeof(char));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "HTTP Response: %s", statusList[index_of_array]);
    menu_layer_reload_data(s_menu_layer);



  } else {

    Tuple *array_size = dict_find(iter,KEY_SIZE);
    Tuple *array_string = dict_find(iter, KEY_LIST);

    int size_of_array = 0;

    if (array_size){ size_of_array = array_size->value->int32; }


    if (size_of_array > 0 && !layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    }

    listSize = size_of_array;
      // Check it was found. If not, dict_find() returns NULL
    if(array_string) {
      // Get the length of the string
      length = strlen(array_string->value->cstring);

      // Free any previous data
      if(s_buffer != NULL) {
        free(s_buffer);
        s_buffer = NULL;
      }

      // Allocate exactly the right amount of memory.
      // This is usually the number of elements multiplied by 
      // the size of each element, returned by sizeof()
      s_buffer = (char*)malloc((length+1) * sizeof(char));

      // Copy in the string to the newly allocated buffer
      //strcpy(s_buffer, array_string->value->cstring);
      memcpy(s_buffer,array_string->value->cstring,(length+1) * sizeof(char));


      APP_LOG(APP_LOG_LEVEL_DEBUG, "Size retrieved from phone: %d", size_of_array);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "List retrieved from phone: %s", s_buffer);

      int bytesWritten = 0;
      int bytesSizeWritten = 0;

      if (size_of_array == 0) {
        bytesWritten = persist_write_string(PERSIST_LIST,"");
        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,0);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing Empty values to persistent store.");

      } else {
        bytesWritten = persist_write_string(PERSIST_LIST,s_buffer);
        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,size_of_array);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (size): %d", bytesSizeWritten);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (list): %d", bytesWritten);
        update_menu_data(s_buffer,(int)size_of_array);
      }
      menu_layer_reload_data(s_menu_layer);
      if (layer_get_hidden(text_layer_get_layer(s_error_text_layer)) && listString == 0) {
        layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
      }
    }
  }
  if (listAction != NULL){
    free(listAction);
    listAction = NULL;
  }
}

static void select_callback(struct MenuLayer *s_menu_layer, MenuIndex *cell_index, 
                            void *callback_context) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "listSize is %d",listSize);
  // If we were displaying s_error_text_layer, remove it and return
  if (!layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    return;
  }
  if (listSize == 0) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
  } else {
    send_to_phone();
  }
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
  char* status = "";

  if (statusList[cell_index->row] != NULL) {
    status = statusList[cell_index->row];
  }

  int text_gap_size = 14 - strlen(name);
  //int mins = tea_array[cell_index->row].mins;

  // Using simple space padding between name and s_item_text for appearance of edge-alignment
  snprintf(s_item_text, sizeof(s_item_text), "%s%*sStatus: %s ", PBL_IF_ROUND_ELSE("", name), 
           PBL_IF_ROUND_ELSE(0, text_gap_size), "", status);
  menu_cell_basic_draw(ctx, cell_layer, PBL_IF_ROUND_ELSE(name, s_item_text), 
                       PBL_IF_ROUND_ELSE(s_item_text, NULL), NULL);
}

static void menu_window_load(Window *window) {

  APP_LOG(APP_LOG_LEVEL_INFO, "Loading Window");
  APP_LOG(APP_LOG_LEVEL_INFO, "listSize: %d", listSize);
  APP_LOG(APP_LOG_LEVEL_INFO, "listString: %s", listString);  
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
  text_layer_set_text(s_error_text_layer, "Call list is empty!\nConfigure requests on your phone.");
  text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_error_text_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_error_text_layer, GColorWhite);
  text_layer_set_background_color(s_error_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));

  s_loading_text_layer = text_layer_create((GRect) { .origin = {0, 60}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_loading_text_layer, "LOADING");
  text_layer_set_font(s_loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_loading_text_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_loading_text_layer, GColorWhite);
  text_layer_set_background_color(s_loading_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  layer_add_child(window_layer, text_layer_get_layer(s_loading_text_layer));
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_error_text_layer);
  text_layer_destroy(s_loading_text_layer);
}



static void init(void) {
  if (persist_exists(PERSIST_LIST_SIZE)){
    update_menu_data(NULL, 0);
    //persist_delete(PERSIST_LIST_SIZE);
  }

  if (listSize > 0) {
    if (statusList != NULL) {
      free (statusList);
      statusList = NULL;
    }
    statusList =  malloc(listSize * sizeof(char*));
    for (int i = 0; i < listSize; ++i) {
      statusList[i] = "Ready";
    }
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
  free_all_data();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
