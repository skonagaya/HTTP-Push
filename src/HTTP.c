#include <pebble.h>
#include <vector.h>

#define KEY_LIST 0
#define KEY_SIZE 1
#define KEY_INDEX 2
#define KEY_RESPONSE 3
#define KEY_ACTION 4
#define KEY_FOLDER_INDEX 5
#define KEY_CHUNK_INDEX 6
#define KEY_CHUNK_SIZE 7
#define KEY_VERSION 8
#define KEY_ERROR 9
#define KEY_NOTIFICATION 10
#define KEY_REQUEST_NAME 11

#define STACK_MAX 100

static const char VERSION[] = "4.0.0";

static const char DEBUG_ENABLED = false;
static const char RESET_DATA = false;

// Calculate the size of the buffer you require by summing the sizes of all 
// the keys and values in the larges message the app will handle. 
// For example, a message containing three integer keys and values will work with a 32 byte buffer size.

// Messages FROM phone contain multiple string values.
#define MAX_INBOX_BUFFER 1024//54

// Messages TO phone is only an integer (index of call found in phone)
#define MAX_OUTBOX_BUFFER 32

static Window *s_menu_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_error_text_layer, *s_loading_text_layer, *s_upgrade_text_layer;
static StatusBarLayer *status_bar = NULL;
static ScrollLayer *s_scroll_layer = NULL;
static TextLayer *s_text_layer = NULL;
static TextLayer *s_title_text_layer = NULL;
//static char s_scroll_text[] = " eget pharetra a, lacin  ia ac justo. Suspendisse at ante nec felis facilisis eleifend. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quam tellus, fermentu  m quis vulputate quis, vestibulum interdum sapien. Vestibulum lobortis pellentesque pretium. Quisque ultricies purus e  u orci convallis lacinia. Cras a urna mi. Donec convallis ante id dui dapibus nec ullamcorper erat egestas. Aenean a m  auris a sapien commodo lacinia. Sed posuere mi vel risus congue ornare. Curabitur leo nisi, euismod ut pellentesque se  d, suscipit sit amet lorem. Aliquam eget sem vitae sem aliquam ornare. In sem sapien, imperdiet eget pharetra a, lacin  ia ac justo. Suspendisse at ante nec felis facilisis eleifend.";
//static char s_scroll_title_text[] = "Lorem";

static const char *MSG_ERR_EMPTY_FOLDER = "Folder is empty.";
static const char *MSG_ERR_EMPTY_LIST = "Call list is empty!\nConfigure requests on your phone.";
static const char *MSG_ERR_UPGRADE_VERSION = "New version detected\nRe-send configuration from phone.";

static char s_item_text[32];
static char *s_buffer = NULL;
static char *listAction = NULL;
static char ***theList = NULL;
static char ***statusList = NULL;
static int *folderSizeList = NULL;
static char *listString = NULL;
static int listSize = 0;
static bool loaded = false;
static bool versionChecked = false;
static ClickConfigProvider previous_ccp;
static DictionaryIterator *dict;

static vector pendingIndexList;
static vector folderIndexList;

// var for chunking 
static int currentChunkIndex = 0;
static char *chunk_buffer = NULL;
static int listByteSize = 0;
static int persistentListIndex = 0;

// var for settings
static int vibrationLength = 100;
static GColor backgroundColor;
static GColor foregroundColor;
static GColor highlightColor;
static GColor statusBarColor;
static bool backgroundIsDark = false;
static bool showFolderIcon = false;
static bool showStatusBar = false;

// apng
static GBitmap *s_bitmap = NULL;
static BitmapLayer *s_bitmap_layer;
static GBitmapSequence *s_sequence = NULL;
static bool loading = false;
static void load_sequence();
static bool apngEnabled = false;

// icon
static GBitmap *s_menu_icon_black;
static GBitmap *s_menu_icon_white;

#if !defined(PBL_PLATFORM_DIORITE) && !defined(PBL_PLATFORM_APLITE)
static char spinner[4][2] = {
    { "-" },
    { "\\"},
    { "|" },
    { "/" }
  };

static int spinnerCtr = 0;
#endif

static char * indexToString(int,int);
static char * status_content(int,int);
static int findn(int);
static void reverse(char[]);
static void itoa(int,char[]);

enum {
  PERSIST_LIST,
  PERSIST_LIST_SIZE,        // number of folders+requests
  PERSIST_LIST_BYTE_LENGTH, // length in byte of entire list
  PERSIST_VERSION_3_0,
  PERSIST_VERSION_3_1,
  PERSIST_VERSION_4_0_0

};

struct RGB
{
        int r;
        int g;
        int b;
};

static unsigned int strtohex(const char * s) {
  unsigned int result = 0;
  int c ;
  while (*s) {
    result = result << 4;
    if (c=(*s-'0'),(c>=0 && c <=9)) result|=c;
    else if (c=(*s-'A'),(c>=0 && c <=5)) result|=(c+10);
    else if (c=(*s-'a'),(c>=0 && c <=5)) result|=(c+10);
    else break;
    ++s;
  }
  return result;
}

static struct RGB color_converter (int hexValue)
{
 struct RGB rgbColor;
 rgbColor.r = ((hexValue >> 16) & 0xFF) ;
 rgbColor.g = ((hexValue >> 8) & 0xFF) ;
 rgbColor.b = ((hexValue) & 0xFF) ;
 return (rgbColor); 
}


static int luminance_percentage_from_hex_string(char * hexStr) {
    
    struct RGB rgb = color_converter(strtohex(hexStr));
    
    double rd = rgb.r;
    double gd = rgb.g;
    double bd = rgb.b;
    
    // constants from http://www.itu.int/rec/R-REC-BT.601
    return ((int) (0.299 * rd + 0.587 * gd + 0.114 * bd) * 100 / 255); 
}

static bool is_hex_string_dark(char * hexStr) {
  return (luminance_percentage_from_hex_string(hexStr) < 50);
}

struct Stack {
  int     * data;
  int     size;
  int     max;
  bool    initiated;
};
typedef struct Stack Stack;

void Stack_Init(Stack *S,int stackMax)
{
    S->size = 0;
    S->max = stackMax;
    S->data = (int*)malloc(stackMax*sizeof(int));
}

void Stack_Deinit(Stack *S)
{
    if (S->data != NULL){
      free(S->data);
    }
}

int Stack_Top(Stack *S)
{
    if (S->size == 0) {
        if (DEBUG_ENABLED) 
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Error: stack empty\n");
        return -1;
    } 

    return S->data[S->size-1];
}

void Stack_Push(Stack *S, int d)
{
    if (S->size < S->max)
        S->data[S->size++] = d;
    else

        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Error: stack full\n");
}

void Stack_Pop(Stack *S)
{
    if (S->size == 0)
    {
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Error: stack empty\n");
    }
    else
        S->size--;
}
static Stack menuLayerStack;
static Stack menuRowStack;
static int previousRowIndex;

static bool at_root() {
  return(Stack_Top(&menuLayerStack) == 0);
}

static bool at_empty_folder() {
  return(folderSizeList[Stack_Top(&menuLayerStack)] == 0);
}

static void set_menu_index(MenuLayer * layer, int toRow) {

    MenuIndex idx = menu_layer_get_selected_index(layer);
    idx.row = toRow;
    menu_layer_set_selected_index(layer,idx,MenuRowAlignCenter,false);
}

// handler that exits app at base
static void pop_all_handler(ClickRecognizerRef recognizer, void *context) {

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "POPPY ALLLL");
  window_stack_pop_all(true);
}


// set to root configuration
static void pop_all_config(void *context) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "calling the new ccp");
  previous_ccp(context);
  window_single_click_subscribe(BUTTON_ID_BACK, pop_all_handler);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "done in the new ccp");

}

static void back_button_handler(ClickRecognizerRef recognizer, void *context) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Back button handler invoked");
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  //set_menu_index(s_menu_layer,Stack_Top(&menuRowStack));
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error layer set to hidden");
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Stack_Top(&menuLayerStack): %d", Stack_Top(&menuLayerStack));
  Stack_Pop(&menuLayerStack); // pop the view
  Stack_Pop(&menuRowStack); // pop the datastructure
  set_menu_index(s_menu_layer,previousRowIndex); // highlight the index of previous view
  menu_layer_reload_data(s_menu_layer);

  if (at_root()){
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "HERE");
    window_set_click_config_provider_with_context(s_menu_window, pop_all_config, s_menu_layer);

  }
}


static void folder_changed_config(void *context) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "calling the new ccp");
  previous_ccp(context);
  window_single_click_subscribe(BUTTON_ID_BACK, back_button_handler);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "done in the new ccp");
}

static void exit_notification_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_button_handler);
  previous_ccp(context);
}


static void notification_back_button_handler(ClickRecognizerRef recognizer, void *context) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "removing notification back handler");
  layer_set_hidden(scroll_layer_get_layer(s_scroll_layer), true);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), false);

  window_set_background_color(s_menu_window, backgroundColor);
  if (at_root()){
    window_set_click_config_provider_with_context(s_menu_window, pop_all_config, s_menu_layer);
  } else {
    window_set_click_config_provider_with_context(s_menu_window, exit_notification_config, s_menu_layer);  
  }
  scroll_layer_destroy(s_scroll_layer);
  text_layer_destroy(s_text_layer);
  text_layer_destroy(s_title_text_layer);
  s_scroll_layer = NULL;
  s_text_layer = NULL;
  s_title_text_layer = NULL;
}

static void notification_exit_callback(void  *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, notification_back_button_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, notification_back_button_handler);
}


static void send_to_phone() {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing data to send to Phone");

  if (listSize == 0) return;
  app_message_outbox_begin(&dict);

  int folderIndex = (int) Stack_Top(&menuLayerStack);
  int rowIndex = (int) menu_layer_get_selected_index(s_menu_layer).row;
  
  if (strcmp(statusList[folderIndex][rowIndex],"Ready") != 0 &&
     strcmp(status_content(folderIndex,rowIndex),"pending")!=0) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "If its not ready or pending...");

    free (statusList[folderIndex][rowIndex]);
    statusList[folderIndex][rowIndex] = NULL;
  }
  menu_layer_reload_data(s_menu_layer);

  #if !defined(PBL_PLATFORM_DIORITE) && !defined(PBL_PLATFORM_APLITE)
  statusList[folderIndex][rowIndex] = spinner[spinnerCtr++];
  if (spinnerCtr >= 5) {
    spinnerCtr = 0;
  }
  #else
  statusList[folderIndex][rowIndex] = "Pending...";
  #endif


  // Add to pending List 


  if (DEBUG_ENABLED) {
      printf("BEFORE ADD");
    for (int i = 0; i < vector_total(&pendingIndexList); i++)
        printf("%s ", (char *) vector_get(&pendingIndexList, i));
    printf("\n");
  }
  
  char * indexString = indexToString(folderIndex,rowIndex);

  vector_add(&pendingIndexList, (indexString));
if (DEBUG_ENABLED) {
      printf("AFTER ADD");
    for (int i = 0; i < vector_total(&pendingIndexList); i++)
        printf("%s ", (char *) vector_get(&pendingIndexList, i));
    printf("\n");
  }

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "statusList[%d][%d] set to Waiting...",folderIndex, rowIndex);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Menu index to send: %d", rowIndex);
  dict_write_uint8(dict,KEY_INDEX,rowIndex);
  dict_write_uint8(dict,KEY_FOLDER_INDEX,folderIndex);
  dict_write_cstring(dict,KEY_ACTION,"response");
  const uint32_t final_size = dict_write_end(dict);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

void chunk_timer_callback(void *data) {
  AppMessageResult amr; 
  DictionaryResult dr;
  amr = app_message_outbox_begin(&dict);

  if(amr == APP_MSG_OK) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_OK");

    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunk index to send: %d", currentChunkIndex);
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_CHUNK_INDEX);
    dict_write_uint8(dict,KEY_CHUNK_INDEX,currentChunkIndex);

    dr = dict_write_cstring(dict,KEY_ACTION,"chunk");
    const uint32_t final_size = dict_write_end(dict);

    if (DEBUG_ENABLED) {
      if (dr == DICT_OK) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_OK");
      else if (dr == DICT_NOT_ENOUGH_STORAGE) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_NOT_ENOUGH_STORAGE");
      else if (dr == DICT_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_INVALID_ARGS");
    }
    
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
    app_message_outbox_send();

  }
  else if(amr == APP_MSG_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_INVALID_ARGS");
  else if(amr == APP_MSG_BUSY) 
  {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_BUSY"); 
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Sleeping for 500 milliseconds..."); 
    app_timer_register(500, chunk_timer_callback, NULL);
  }

}

static void send_version_to_phone() {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to send version to Phone");

  //if (listSize == 0) return;


  app_message_outbox_begin(&dict);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_VERSION);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message version to send: %s", VERSION);
  dict_write_cstring(dict,KEY_VERSION,VERSION);
  dict_write_cstring(dict,KEY_ACTION,"version");
  const uint32_t final_size = dict_write_end(dict);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void send_command_error_to_phone (char *cmd) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to send error to Phone");

  //if (listSize == 0) return;

  app_message_outbox_begin(&dict);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_ERROR);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message error to send: %s", cmd);
  dict_write_cstring(dict,KEY_ERROR,cmd);
  dict_write_cstring(dict,KEY_ACTION,"error");
  const uint32_t final_size = dict_write_end(dict);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void request_next_chunk_from_phone() {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to request next chunk from Phone");

  //if (listSize == 0) return;

  AppMessageResult amr; 
  DictionaryResult dr;

  amr = app_message_outbox_begin(&dict);

  if(amr == APP_MSG_OK) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_OK");
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunk index to send: %d", currentChunkIndex);
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_CHUNK_INDEX);
    dict_write_uint8(dict,KEY_CHUNK_INDEX,currentChunkIndex);
    dr = dict_write_cstring(dict,KEY_ACTION,"chunk");
    const uint32_t final_size = dict_write_end(dict);


    if (DEBUG_ENABLED) {
      if (dr == DICT_OK) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_OK");
      else if (dr == DICT_NOT_ENOUGH_STORAGE) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_NOT_ENOUGH_STORAGE");
      else if (dr == DICT_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_INVALID_ARGS");
    }
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
    app_message_outbox_send();
  }
  else if(amr == APP_MSG_INVALID_ARGS) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_INVALID_ARGS");
  }
  else if(amr == APP_MSG_BUSY) 
  { 
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_BUSY"); 
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"Sleeping for 500 milliseconds..."); 
    app_timer_register(500, chunk_timer_callback, NULL);
  }

}

/*
static void free_all_data() {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Starting free all procedure");

  APP_LOG(APP_LOG_LEVEL_DEBUG, "The listSize is %d", listSize);

  if(listAction != NULL) {
    free(listAction);
    listAction = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed listAction memory");
  }

  if (theList != NULL){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Attempting to free list matrix?");
    if (listSize != 0) {
      for (int o=0; o < listSize; o++){
        for (int i=0; i < folderSizeList[o]; i++){
          if (theList[o][i] != NULL){
            free(theList[o][i]);
            theList[o][i] = NULL;
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed theList[%d] memory", i);
          }
          if (strcmp(statusList[o][i],"Ready") != 0 &&
              strcmp(statusList[o][i],"Pending...") != 0) {
            free(statusList[o][i]);
            statusList[o][i] = NULL;
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed statusList[%d][%d] memory",o, i);

          }
        }
      }
    }
    free(theList);
    theList = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed theList memory");
  }

  if (listString != NULL) {
    free(listString);
    listString = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed listString memory");
  }
  //if (s_buffer != NULL)
    //free(s_buffer);
}*/

static bool startsWith(const char *a, const char *b)
{
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

static void chopStringBy(char *list,int amount) {
  memmove(list, list+amount, strlen(list)); 
}



static char * extract_between(const char *str, const char *p1, const char *p2)
{
  const char *i1 = strstr(str, p1);
  if(i1 != NULL)
  {
    const size_t pl1 = strlen(p1);
    const char *i2 = strstr(i1 + pl1, p2);
    if(p2 != NULL)
    {
     /* Found both markers, extract text. */
     const size_t mlen = i2 - (i1 + pl1);
     char *ret = malloc(mlen + 1);
     if(ret != NULL)
     {
       memcpy(ret, i1 + pl1, mlen);
       ret[mlen] = '\0';
       return ret;
     }
    }
  }
  return "";
}


#if !defined(PBL_PLATFORM_DIORITE) && !defined(PBL_PLATFORM_APLITE)
static void update_colors() {
  menu_layer_set_highlight_colors(s_menu_layer,highlightColor,foregroundColor);
  menu_layer_set_normal_colors(s_menu_layer,backgroundColor,highlightColor);
  text_layer_set_text_color(s_error_text_layer, foregroundColor);
  text_layer_set_background_color(s_error_text_layer, highlightColor);
  text_layer_set_text_color(s_upgrade_text_layer, foregroundColor);
  text_layer_set_background_color(s_upgrade_text_layer, highlightColor);
  if (showStatusBar) {

    status_bar_layer_set_colors(status_bar, statusBarColor, foregroundColor);
  }
  window_set_background_color(s_menu_window, backgroundColor);
}
#endif

static void update_menu_data(int stringSize) {

  //free_all_data();
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  if (apngEnabled)
    layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), false);
  loading = true;
  loaded = false;

  menu_layer_reload_data(s_menu_layer);

  if (stringSize == 0) {

    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    if (apngEnabled)
    layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), true);
    loading = false;
    loaded = true;
    menu_layer_reload_data(s_menu_layer);
    return;
  }


  if (stringSize > 0) {

    listSize = persist_read_int(PERSIST_LIST_SIZE);
    listByteSize = persist_read_int(PERSIST_LIST_BYTE_LENGTH);

    int bytesRemaining = listByteSize;
    int currentPersistenceIndex = 0;

    if (listString != NULL) {
      free(listString);
      listString = NULL;
    }
    listString = malloc(listByteSize+1);

    vector_clear(&folderIndexList);

    while (bytesRemaining > 0) {
      char *buf= malloc(PERSIST_DATA_MAX_LENGTH);

      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "PERSIST_DATA_MAX_LENGTH: %d", PERSIST_DATA_MAX_LENGTH);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading chunk %d from persistent store", currentPersistenceIndex);
      //int readByteSize = persist_read_string(PERSIST_LIST+currentPersistenceIndex,buf,PERSIST_DATA_MAX_LENGTH);
      persist_read_string(PERSIST_LIST+currentPersistenceIndex,buf,PERSIST_DATA_MAX_LENGTH);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Read: %s", buf);
      if (currentPersistenceIndex == 0) {
        strcpy(listString, buf);
      }
      else {
        strcat(listString, buf);
      }
      currentPersistenceIndex = currentPersistenceIndex - 1;
      //memcpy(listString,buf,readByteSize);
      bytesRemaining = bytesRemaining - PERSIST_DATA_MAX_LENGTH;
      free(buf);
      buf = NULL;
    }
    
    listString[listByteSize] = '\0';

    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Read list from persistent storage: %s", listString);
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Read size (value) from persistent storage: %d", listSize);

  } else {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Processing stringSize: %i",stringSize);
    listSize = stringSize;
    listString = (char*)malloc((strlen(s_buffer)+1)*sizeof(char));
    memcpy(listString, s_buffer, (strlen(s_buffer)+1)*sizeof(char));
    //strcpy(listString,s_buffer);
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "strlen(listString): %i",strlen(listString));
  }

  theList = (char ***) malloc(listSize * sizeof(char**));
  statusList = (char ***) malloc(listSize * sizeof(char**));
  folderSizeList = (int *) malloc(listSize * sizeof(int));

  Stack_Deinit(&menuLayerStack);
  Stack_Deinit(&menuRowStack);
  Stack_Init(&menuLayerStack,listSize);
  Stack_Init(&menuRowStack,listSize);
  Stack_Push(&menuLayerStack,0);

  bool parseSuccessful = true; //

  //Assumes the following format
  //_F_<Folder Size>_<Folder Index>_<Parent Folder Index>_<Row Index>_<Folder Name>
  //_E_<Parent Folder Index>_<Entry Row Index>_<Entry Name>
  //_V_<Vibration_Length>
  while (startsWith(listString, "_") && parseSuccessful) {
    parseSuccessful = false;
    if (startsWith(listString, "_F_")) {
      chopStringBy(listString,2);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n\n",listString);

      char * folderSizeStr = extract_between(listString,"_","_");
      int folderSize    = atoi(folderSizeStr);
      chopStringBy(listString,1+ strlen(folderSizeStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) folderSize %d",folderSize);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(folderSizeStr) %d",strlen(folderSizeStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }

      char * folderIndexStr = extract_between(listString,"_","_");
      int folderIndex    = atoi(folderIndexStr);
      chopStringBy(listString,1+ strlen(folderIndexStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) folderIndex %d",folderIndex);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) strlen(folderIndexStr) %d",strlen(folderIndexStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) ListString %s\n",listString);
      }
      char * folderParentIndexStr = extract_between(listString,"_","_");
      int folderParentIndex    = atoi(folderParentIndexStr);
      chopStringBy(listString,1+ strlen(folderParentIndexStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) folderParentIndex %d",folderParentIndex);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) strlen(folderParentIndexStr) %d",strlen(folderParentIndexStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) ListString %s\n",listString);
      }

      char * folderRowStr = extract_between(listString,"_","_");
      int folderRow    = atoi(folderRowStr);
      chopStringBy(listString,1+ strlen(folderRowStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "4) folderRowStr %s",folderRowStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "4) folderRow %d",folderRow);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "4) strlen(folderRowStr) %d",strlen(folderRowStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "4) ListString %s\n",listString);
      }

      char * folderName = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(folderName));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "5) folderName %s",folderName);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "5) strlen(folderName) %d",strlen(folderName));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "5) ListString %s\n",listString);
      }

      if (folderSize != 0) {
        theList[folderIndex] = (char **) malloc(folderSize * sizeof(char*));
        statusList[folderIndex] = (char **) malloc(folderSize * sizeof(char*));
      } 

      
      folderSizeList[folderIndex] = folderSize;

      if (folderParentIndex != -1 && folderRow != -1) {
        if (DEBUG_ENABLED) 
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing folder name to theList[%d][%d]: %s",folderParentIndex,folderRow,folderName);
        theList[folderParentIndex][folderRow] = (char*)malloc((strlen(folderName)+1) * sizeof(char));
        memcpy(theList[folderParentIndex][folderRow],folderName+'\0',(strlen(folderName)+1) * sizeof(char));
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing status name to theList[%d][%d]: %s",folderParentIndex,folderRow,"F");
        statusList[folderParentIndex][folderRow] = strcat(folderIndexStr,"_");

        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking theList[%d][%d]: %s",folderParentIndex,folderRow,theList[folderParentIndex][folderRow]);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking statusList[%d][%d]: %s",folderParentIndex,folderRow,statusList[folderParentIndex][folderRow]);
      }
      parseSuccessful = true;

      // Don't add root
      if (folderParentIndex >= 0 && folderRow >= 0) {
        char * indexString = indexToString(folderParentIndex,folderRow);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "6) Added %s to vector folderIndexList",indexString);
        vector_add(&folderIndexList,(indexString));
      }

    } else if (startsWith(listString, "_E_")){
      chopStringBy(listString,2);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * parentIndexStr = extract_between(listString,"_","_");
      int parentIndex    = atoi(parentIndexStr);
      chopStringBy(listString,1+ strlen(parentIndexStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) parentIndex %d",parentIndex);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(parentIndexStr) %d",strlen(parentIndexStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      char * entryRowStr = extract_between(listString,"_","_");
      int entryRow    = atoi(entryRowStr);
      chopStringBy(listString,1+ strlen(entryRowStr));
      if (DEBUG_ENABLED){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) entryRow %d",entryRow);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) strlen(entryRowStr) %d",strlen(entryRowStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "2) ListString %s\n",listString);
      }

      char * entryName = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(entryName));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) entryName %s",entryName);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) strlen(entryName) %d",strlen(entryName));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "3) ListString %s\n",listString);
      }
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing entry name to theList[%d][%d]: %s",parentIndex,entryRow,entryName);
      theList[parentIndex][entryRow] = (char*)malloc((strlen(entryName)+1) * sizeof(char));
      memcpy(theList[parentIndex][entryRow],entryName+'\0',(strlen(entryName)+1) * sizeof(char));
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing status name to theList[%d][%d]: %s",parentIndex,entryRow,"Ready");
      statusList[parentIndex][entryRow] = "Ready";
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking theList[%d][%d]: %s",parentIndex,entryRow,theList[parentIndex][entryRow]);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking statusList[%d][%d]: %s",parentIndex,entryRow,statusList[parentIndex][entryRow]);

      parseSuccessful = true;
    } else if (startsWith(listString, "_V_")) {
      chopStringBy(listString,2);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * vibrationLengthStr = extract_between(listString,"_","_");
      vibrationLength    = atoi(vibrationLengthStr);
      chopStringBy(listString,1+ strlen(vibrationLengthStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) vibtrationLengthInt %d",vibrationLength);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(vibrationLengthStr) %d",strlen(vibrationLengthStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      parseSuccessful = true;

    } else if (startsWith(listString, "_BC_")) { // Set Background color
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * backgroundColorStr = extract_between(listString,"_","_");

      backgroundIsDark = is_hex_string_dark(backgroundColorStr);
      chopStringBy(listString,1+ strlen(backgroundColorStr));
      if (DEBUG_ENABLED){ 
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) backgroundColorStr %s",backgroundColorStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(backgroundColorStr) %d",strlen(backgroundColorStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      #if defined(PBL_COLOR)
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting for Pebble Color");
        backgroundColor = GColorFromHEX(strtohex(backgroundColorStr));
      #endif
      parseSuccessful = true;
    } else if (startsWith(listString, "_FC_")) { // Set foreground color
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * foregroundColorStr = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(foregroundColorStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) foregroundColorStr %s",foregroundColorStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(foregroundColorStr) %d",strlen(foregroundColorStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      #if defined(PBL_COLOR)
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting for Pebble Color");
        foregroundColor = GColorFromHEX(strtohex(foregroundColorStr));
      #endif
      parseSuccessful = true;
    } else if (startsWith(listString, "_SC_")) { // Set highlight color
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * highlightColorStr = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(highlightColorStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) highlightColorStr %s",highlightColorStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(highlightColorStr) %d",strlen(highlightColorStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      #if defined(PBL_COLOR)
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting for Pebble Color");
        highlightColor = GColorFromHEX(strtohex(highlightColorStr));
      #endif
      parseSuccessful = true;
    } else if (startsWith(listString, "_TC_")) { // Set statusbar color
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * statusBarColorStr = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(statusBarColorStr));
      if (DEBUG_ENABLED) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) statusBarColorStr %s",statusBarColorStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(statusBarColorStr) %d",strlen(statusBarColorStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }
      #if defined(PBL_COLOR)
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting for Pebble Color");
        statusBarColor = GColorFromHEX(strtohex(statusBarColorStr));
      #endif
      parseSuccessful = true;
    } else if (startsWith(listString, "_FI_")) { // Set folder icon
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * showFolderIconStr = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(showFolderIconStr));
      if (DEBUG_ENABLED) { 
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) highlightColorStr %s",showFolderIconStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(highlightColorStr) %d",strlen(showFolderIconStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }

      showFolderIcon = strcmp(showFolderIconStr, "0")==0 ? 0 : 1;

      parseSuccessful = true;
    } else if (startsWith(listString, "_SB_")) { // Set status bar
      chopStringBy(listString,3);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * showStatusBarStr = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(showStatusBarStr));
      if (DEBUG_ENABLED) { 
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) highlightColorStr %s",showStatusBarStr);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(highlightColorStr) %d",strlen(showStatusBarStr));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
      }

      showStatusBar = strcmp(showStatusBarStr, "0")==0 ? 0 : 1;

      parseSuccessful = true;
    } /*else if (startsWith(listString, "_FL_")) { // Set folder Index List
      chopStringBy(listString,3);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      folderIndexList = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(folderIndexList));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) folderIndexList %s",folderIndexList);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(folderIndexList) %d",strlen(folderIndexList));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);
    }*/

    if (!parseSuccessful) {
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "...nothing more to do...");
      break;
    }
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "...continuing...");
  }
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Completed data stream parsing.");

  if (DEBUG_ENABLED) {
    if (listSize != 0) {
      for (int o=0; o < listSize; o++){
        for (int i=0; i < folderSizeList[o]; i++){
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d][%d]: %s",o,i, theList[o][i]);
        }
      }
    }
  }

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "updating color");

  #if defined(PBL_COLOR)
  update_colors();
  #endif

  if (showStatusBar) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "show status bar");
    layer_set_hidden(status_bar_layer_get_layer(status_bar), false);
  } else {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "hide status bar");
    layer_set_hidden(status_bar_layer_get_layer(status_bar), true);
  }

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "updating color completed");
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_menu_data completed");

  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
  if (apngEnabled)
  layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), true);
  loading = false;
  loaded = true;

  menu_layer_reload_data(s_menu_layer);
}

static void showNotification(char * title,char * text){

  int statusBarOffset = showStatusBar ? STATUS_BAR_LAYER_HEIGHT : 0;

  Layer *window_layer = window_get_root_layer(s_menu_window);
  GRect window_bounds = layer_get_bounds(window_layer);
  GRect bounds = GRect(0, statusBarOffset, window_bounds.size.w, window_bounds.size.h - statusBarOffset);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "Loading Notification layer");

  s_scroll_layer = scroll_layer_create(bounds);

  s_title_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, 60 - statusBarOffset));
  s_text_layer = text_layer_create(GRect(0, 60 - statusBarOffset, bounds.size.w, 32767));
  text_layer_set_text(s_text_layer, text);
  text_layer_set_text(s_title_text_layer, title);

  scroll_layer_set_callbacks(s_scroll_layer, (ScrollLayerCallbacks){.click_config_provider=notification_exit_callback});

  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  // Add the layers for display
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_title_text_layer));
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_text_layer));
  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));

  // Set the ScrollLayer's content size to the total size of the text
  scroll_layer_set_content_size(s_scroll_layer,
                                text_layer_get_content_size(s_text_layer));

  // Trim text layer and scroll content to fit text box
  //text_layer_set_size(s_text_layer, text_layer_get_content_size(s_text_layer));
  //text_layer_set_size(s_title_text_layer, text_layer_get_content_size(s_title_text_layer));
  text_layer_set_text_alignment(s_title_text_layer, GTextAlignmentCenter);

#if defined(PBL_ROUND)

  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  uint8_t inset = 3;
  text_layer_enable_screen_text_flow_and_paging(s_title_text_layer, inset);
  text_layer_enable_screen_text_flow_and_paging(s_text_layer, inset);

  // Enable ScrollLayer paging
  scroll_layer_set_paging(s_scroll_layer, true);

#endif


  text_layer_set_text_color(s_title_text_layer, foregroundColor);
  text_layer_set_background_color(s_title_text_layer, highlightColor);
  text_layer_set_text_color(s_text_layer, highlightColor);
  text_layer_set_background_color(s_text_layer, foregroundColor);
  window_set_background_color(s_menu_window, foregroundColor);
  // This binds the scroll layer to the window so that up and down map to scrolling
  // You may use scroll_layer_set_callbacks to add or override interactivity
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_menu_window);
  //scroll_layer_set_callbacks(s_scroll_layer, (ScrollLayerCallbacks){.click_config_provider=notification_exit_callback});
  layer_set_hidden(scroll_layer_get_layer(s_scroll_layer), false);

}



static int findn(int num)
{
  if (num == 0) {
    return 1;
  }
    int n = 0;
    while(num) {
        num /= 10;
        n++;
    }
    return n;
}

 /* reverse:  reverse string s in place */
 static void reverse(char s[])
 {
     int i, j;
     char c;
 
     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }
 
 /* itoa:  convert n to characters in s */
 static void itoa(int n, char s[])
 {
     int i, sign;
 
     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
 }

static char * indexToString(int currentFolderIndex, int currentRowIndex) {
  char * indexString = (char *) malloc(findn(currentFolderIndex)+findn(currentRowIndex)+1);
  char currentFolderIndexStr[findn(currentFolderIndex)];
  char currentRowIndexStr[findn(currentRowIndex)];

  itoa(currentFolderIndex,currentFolderIndexStr);
  itoa(currentRowIndex,currentRowIndexStr);

  strcpy(indexString,currentFolderIndexStr);
  strcat(indexString,currentRowIndexStr);
  return indexString;
}

static char * status_content(int currentFolderIndex, int currentRowIndex) {

  char * indexString = indexToString( currentFolderIndex, currentRowIndex );
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "%d,%d: %s",currentFolderIndex,currentRowIndex,indexString);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "folderIndexList: %s",folderIndexList);


    // for (int i = 0; i < vector_total(&pendingIndexList); i++)
    //     printf("--%s ", (char *) vector_get(&pendingIndexList, i));
    // printf("\n");

  if (vector_contains(&folderIndexList,indexString)) {
    free(indexString);
    indexString = NULL;
    return "folder";
  } else if (vector_contains(&pendingIndexList,indexString)) {
    free(indexString);
    indexString = NULL;
    return "pending";
  }
  free(indexString);
  indexString = NULL;
  return "";
}


void chopN(char *str, size_t n)
{
    size_t len = strlen(str);
    if (n > len)
        return;  // Or: n = len;
    memmove(str, str+n, len - n + 1);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Received inbox_received_handler from Phone!");
  if (!loaded) {
    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    if(apngEnabled)
    layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), true);
    loading = false;
    loaded = true;
  }
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received dict from phone of size: \"%d\"", sizeof(iter));
  if (DEBUG_ENABLED)
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
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Received action of: \"%s\"", listAction);
  }

  if (strcmp(listAction, "response")==0){
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Received a http response from Phone!");

    uint32_t segments[] = { vibrationLength };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);

    Tuple *response_string = dict_find(iter, KEY_RESPONSE);
    Tuple *array_row_index = dict_find(iter,KEY_INDEX);
    Tuple *array_folder_index = dict_find(iter,KEY_FOLDER_INDEX);
    Tuple *request_name_string = dict_find(iter, KEY_REQUEST_NAME);
    Tuple *notification_enabled = dict_find(iter,KEY_NOTIFICATION);

    int folder_index_of_array = 0;
    int row_index_of_array = 0;

    if (array_folder_index){ folder_index_of_array = array_folder_index->value->int32; }
    if (array_row_index){ row_index_of_array = array_row_index->value->int32; }

    length = strlen(response_string->value->cstring);

    bool notify = (notification_enabled->value->int32 == 0) ? false:true;

    if (statusList[folder_index_of_array][row_index_of_array] != NULL) {
      statusList[folder_index_of_array][row_index_of_array]  = NULL;

    if (DEBUG_ENABLED) {
      printf("BEFORE REMOVE");
    
      for (int i = 0; i < vector_total(&pendingIndexList); i++)
          printf("%s ", (char *) vector_get(&pendingIndexList, i));
      printf("\n");
    }

      //remove from pending list
      char * indexString = indexToString(folder_index_of_array,row_index_of_array);
      vector_remove(&pendingIndexList, indexString);
if (DEBUG_ENABLED) {
    printf("AFTER REMOVE");
  for (int i = 0; i < vector_total(&pendingIndexList); i++)
      printf("%s ", (char *) vector_get(&pendingIndexList, i));
  printf("\n");
}

      free(indexString);
      indexString = NULL;

    }
    if (notify) {
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting response in notification");
      layer_set_hidden(menu_layer_get_layer(s_menu_layer), true);
      if (s_scroll_layer != NULL) {
        scroll_layer_destroy(s_scroll_layer);
        s_scroll_layer = NULL;

        if (s_text_layer != NULL) {
          text_layer_destroy(s_text_layer);
          s_text_layer = NULL;
        }
        if (s_title_text_layer != NULL) {
          text_layer_destroy(s_title_text_layer);
          s_title_text_layer = NULL;

        }
      }
      showNotification(request_name_string->value->cstring,response_string->value->cstring);
      statusList[folder_index_of_array][row_index_of_array] = (char*)malloc((length+1) * sizeof(char));
      memcpy(statusList[folder_index_of_array][row_index_of_array],response_string->value->cstring,(length +1) * sizeof(char));
    } else {
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting response in status row");
      statusList[folder_index_of_array][row_index_of_array] = (char*)malloc((length+1) * sizeof(char));
      memcpy(statusList[folder_index_of_array][row_index_of_array],response_string->value->cstring,(length +1) * sizeof(char));
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "HTTP Response: %s", statusList[folder_index_of_array][row_index_of_array]);
    }
    menu_layer_reload_data(s_menu_layer);


  } else if (strcmp(listAction, "chunk")==0) {
  if (!versionChecked) {
    layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), true);
    versionChecked = true;
  }


    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
    if (apngEnabled)
      layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), false);
    loading = true;
    loaded = false;
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Found chunking call");

    char *string_chunk = dict_find(iter, KEY_LIST)->value->cstring; 

    // make sure that buffer is clear before starting new chunking activity
    if (currentChunkIndex == 0) { 
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing chunk_buffer");

      //free_all_data();

      int list_string_length = dict_find(iter, KEY_CHUNK_SIZE)->value->int32;
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "list_string_length is : %d", list_string_length);

      if (chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
      }

      chunk_buffer = (char*)malloc((list_string_length+1) * sizeof(char));

      // Copy the first chunk to chunk buffer
      strcpy(chunk_buffer, string_chunk);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer strcpy result: %s", chunk_buffer);

    } else {
      strcat(chunk_buffer, string_chunk);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer strcat result: %s", chunk_buffer);
    }

    currentChunkIndex = currentChunkIndex + 1;
    
    request_next_chunk_from_phone();

  } else if (strcmp(listAction, "version")==0) {
    send_version_to_phone();
  } else if (strcmp(listAction, "update")==0) {
  if (!versionChecked) {
    layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), true);
    versionChecked = true;
  }

    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
    if (apngEnabled)
      layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), false);
    loading = true;
    loaded = false;

    if (currentChunkIndex == 0) {
      //free_all_data();
    }

    int array_size = dict_find(iter,KEY_SIZE)->value->int32;;
    char *array_string = dict_find(iter, KEY_LIST)->value->cstring;

    listSize = array_size;

    

    if (array_size > 0 && !layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {

      text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    }

    if (currentChunkIndex > 0) {
      currentChunkIndex = 0;

      strcat(chunk_buffer, array_string);
      array_string = chunk_buffer;

      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer concat result: %s", chunk_buffer);
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating with array_string: %s", array_string);

      // TODO: append array_string and copy back into array_string then free chunk_buffer
    } 

      // Check it was found. If not, dict_find() returns NULL
    if(array_string) {

      // Get the length of the string
      length = strlen(array_string);

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
      memcpy(s_buffer,array_string,(length+1) * sizeof(char));

      if (DEBUG_ENABLED) { 
        APP_LOG(APP_LOG_LEVEL_DEBUG, "List length: %d", length);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Size retrieved from phone: %d", array_size);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "List retrieved from phone: %s", s_buffer);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "List retrieved from phone: %s", array_string);
      }

      int bytesWritten = 0;
      int bytesSizeWritten = 0;
      int bytesLengthWritten = 0;

      if (array_size == 0) {
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Start writing empty values to  persist store.");
        bytesWritten = persist_write_string(PERSIST_LIST,"");
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrote PERSIST_LIST");
        bytesWritten = persist_write_string(PERSIST_VERSION_4_0_0,VERSION);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrote VERSION");
        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,0);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Wrote PERSIST_LIST_SIZE");
        bytesLengthWritten = persist_write_int(PERSIST_LIST_BYTE_LENGTH,0);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing Empty values to persistent store.");

      } else {
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Start writing values to persist store.");
        // write into persistent store PERSIST_DATA_MAX_LENGTH - 1 bytes at a time
        int bytesRemaining = length;
        persistentListIndex = 0; 
        
        while (bytesRemaining > 0) {

          //char indexToInt[15];
          //sprintf(indexToInt, "%d", persistentListIndex);

          char *buf= malloc(PERSIST_DATA_MAX_LENGTH - 1);
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Allocating PERSIST_DATA_MAX_LENGTH-1 : %d", PERSIST_DATA_MAX_LENGTH);
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "Anything in array_string? : %s", array_string);
          //strncpy(buf, s_buffer, PERSIST_DATA_MAX_LENGTH - 1);
          memcpy(buf, s_buffer, PERSIST_DATA_MAX_LENGTH - 1);
          //strcpy(buf, s_buffer);
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "String");
          //buf[PERSIST_DATA_MAX_LENGTH - 1] = '\0';

          bytesWritten = persist_write_string(PERSIST_LIST + persistentListIndex,buf);
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunked into persistent store: %s", s_buffer);
          chopN(s_buffer, PERSIST_DATA_MAX_LENGTH - 1);
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "bytesRemaining before: %d", bytesRemaining);
          bytesRemaining = bytesRemaining - PERSIST_DATA_MAX_LENGTH - 1;
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "bytesRemaining after: %d", bytesRemaining);
          persistentListIndex = persistentListIndex - 1;
          if (DEBUG_ENABLED)
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (list): %d", bytesWritten);
          free(buf);
          buf = NULL;
        }
        bytesWritten = persist_write_string(PERSIST_VERSION_4_0_0,VERSION);


        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,array_size);
        bytesLengthWritten = persist_write_int(PERSIST_LIST_BYTE_LENGTH,length);

        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (size): %d", bytesSizeWritten);
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (list_bytes): %d", bytesLengthWritten);

        update_menu_data(array_size);
      }
      menu_layer_reload_data(s_menu_layer);
      if (layer_get_hidden(text_layer_get_layer(s_error_text_layer)) && listString == 0) {
        text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
        layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
      }
      if(s_buffer != NULL) {
        free(s_buffer);
        s_buffer = NULL;
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed s_buffer memory");
      }
      if(chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed chunk_buffer memory");
      }


      layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
      if (apngEnabled)
        layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), true);
      loading = false;
      loaded = true;
      menu_layer_reload_data(s_menu_layer);
    }
  } else {
    send_command_error_to_phone(listAction);
  }
  if (listAction != NULL){
    free(listAction);
    listAction = NULL;
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed listAction memory");

  }
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox handler completed");
}

static void select_callback(struct MenuLayer *s_menu_layer, MenuIndex *cell_index, 
                            void *callback_context) {

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "listSize is %d",listSize);
  // If we were displaying s_error_text_layer, remove it and return
  if (!layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    return;
  }

  if (!layer_get_hidden(text_layer_get_layer(s_upgrade_text_layer))) {
    layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), true);
    return;
  }


  if (listSize == 0) {
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
  } else if (at_empty_folder()) {
    text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_FOLDER);
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
  
  // If the status has a '_' char, we know it's a folder 
  // also if folderIndexList is NULL, that means that means 1) no folders exist and 2) list was not generated from pebbleJS yet after 3.3.0
  } else if (strcmp(status_content(Stack_Top(&menuLayerStack),cell_index->row),"folder") == 0) {
    previousRowIndex = cell_index->row;
    char * currentStatus = statusList[Stack_Top(&menuLayerStack)][cell_index->row];
    if(at_root()) {
      window_set_click_config_provider_with_context(s_menu_window, folder_changed_config, s_menu_layer);
    }
    char *stringcopy = malloc ((strlen(currentStatus)+1)*sizeof(char));
    if (stringcopy)
    {
      strcpy(stringcopy,currentStatus);
      stringcopy[strlen(stringcopy)-1] = 0;

    }
    Stack_Push(&menuLayerStack,atoi(stringcopy));
    Stack_Push(&menuRowStack,cell_index->row);
    set_menu_index(s_menu_layer,0);
    menu_layer_reload_data(s_menu_layer);
    free(stringcopy);
    stringcopy = NULL;

    // Check if we're now in a folder that's empty. If we are, then show error
    if (at_empty_folder()) {
      text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_FOLDER);
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    }

  } else {
    send_to_phone();
  }
}

static uint16_t get_sections_count_callback(struct MenuLayer *menulayer, uint16_t section_index, 
                                            void *callback_context) {
  int currentIndex = Stack_Top(&menuLayerStack);
  if (currentIndex == -1) 
    return 0;
  else 
    return folderSizeList[currentIndex];
}

#ifdef PBL_ROUND
static int16_t get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, 
                                        void *callback_context) {
  return 60;
}
#endif


static void draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, 
                             void *callback_context) {
  //if (DEBUG_ENABLED)
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Draw handler");
  if (listSize == 0) return;
  
  // if the folder's empty, draw nutt'n
  if (at_empty_folder()) return;

  int folderIndex = Stack_Top(&menuLayerStack);
  int rowIndex = cell_index->row;

  char* name = theList[folderIndex][rowIndex]; 
  char * currentStatus = statusList[folderIndex][rowIndex];

  GBitmap * isFolder = NULL;

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Found status update: %s", currentStatus);

  // printf("pendingIndexList:");
  // for (int i = 0; i < vector_total(&pendingIndexList); i++)
  //     printf("%s ", (char *) vector_get(&pendingIndexList, i));
  // printf("\n");

  if (currentStatus != NULL) {
    
    if (strcmp(status_content(folderIndex,rowIndex),"folder")==0) { 
      // Set the status field as "Open Folder"
      if (showFolderIcon) {
        if (backgroundIsDark) {
          isFolder = s_menu_icon_white;
        } else {
          isFolder = s_menu_icon_black;
        }
      }
      snprintf(s_item_text, sizeof(s_item_text), "%s", "Open Folder");
    } else if (strcmp(status_content(Stack_Top(&menuLayerStack),cell_index->row),"pending")==0) {


      #if !defined(PBL_PLATFORM_DIORITE) && !defined(PBL_PLATFORM_APLITE)
      snprintf(s_item_text, sizeof(s_item_text), "%s", spinner[spinnerCtr++]);
      if (spinnerCtr >= 4) {
        spinnerCtr = 0;
      }
      #else
      snprintf(s_item_text, sizeof(s_item_text), "Status: %s", statusList[Stack_Top(&menuLayerStack)][cell_index->row]);
      #endif

      // Using simple space padding between name and s_item_text for appearance of edge-alignment
      //menu_cell_basic_draw(ctx, cell_layer, name, s_item_text, s_menu_icon);
      //menu_cell_basic_draw(ctx, cell_layer, name, s_item_text, NULL);
    } else { // otherwise it's a request entry

      // Pad the status field with "Status: " + <status of request>
      snprintf(s_item_text, sizeof(s_item_text), "%s", statusList[Stack_Top(&menuLayerStack)][cell_index->row]);
      // Using simple space padding between name and s_item_text for appearance of edge-alignment
    }
  } else {

  }

    menu_cell_basic_draw(ctx, cell_layer, name, s_item_text, isFolder);

  
}


static void timer_handler(void *context) {
  uint32_t next_delay;

  // Advance to the next APNG frame
  if(gbitmap_sequence_update_bitmap_next_frame(s_sequence, s_bitmap, &next_delay)) {
    bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));

    // Timer for that delay
    app_timer_register(next_delay, timer_handler, NULL);
  } else if (!loading) {
    // Start again
    load_sequence();
  }
}

static void load_sequence() {
  // Free old data
  if(s_sequence) {
    gbitmap_sequence_destroy(s_sequence);
    s_sequence = NULL;
  }
  if(s_bitmap) {
    gbitmap_destroy(s_bitmap);
    s_bitmap = NULL;
  }

  // Create sequence
  s_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_ANIMATION);

  // Create GBitmap
  s_bitmap = gbitmap_create_blank(gbitmap_sequence_get_bitmap_size(s_sequence), GBitmapFormat8Bit);

  // Begin animation
  app_timer_register(1, timer_handler, NULL);
}



static void menu_window_load(Window *window) {

  if (DEBUG_ENABLED) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Loading Window");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "listSize: %d", listSize);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "listString: %s", listString); 
  }

  Stack_Init(&menuLayerStack,listSize); 

  int statusBarOffset = showStatusBar ? STATUS_BAR_LAYER_HEIGHT : 0;

  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  GRect bounds = GRect(0, statusBarOffset, window_bounds.size.w, window_bounds.size.h - statusBarOffset);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "6Loading Icon");

  s_menu_icon_white = gbitmap_create_with_resource(RESOURCE_ID_FOLDER_WHITE);
  s_menu_icon_black = gbitmap_create_with_resource(RESOURCE_ID_FOLDER_BLACK);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "1Loading Window");

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_sections_count_callback,
    .get_cell_height = PBL_IF_ROUND_ELSE(get_cell_height_callback, NULL),
    .draw_row = draw_row_handler,
    .select_click = select_callback
  });

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "2Loading Window");

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
  previous_ccp = window_get_click_config_provider(window);

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "3Loading Window");

  s_error_text_layer = text_layer_create((GRect) { .origin = {0, bounds.size.h / 2 - 21 + statusBarOffset}, .size = {bounds.size.w, 42}});
  text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
  text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_error_text_layer, GTextAlignmentCenter);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));

  s_upgrade_text_layer = text_layer_create((GRect) { .origin = {0, 44}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_upgrade_text_layer, MSG_ERR_UPGRADE_VERSION);
  text_layer_set_font(s_upgrade_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_upgrade_text_layer, GTextAlignmentCenter);
  layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_upgrade_text_layer));

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "6Loading Status Bar");

  status_bar = status_bar_layer_create();
  layer_add_child(window_layer, status_bar_layer_get_layer(status_bar));

  if (!showStatusBar) {
    layer_set_hidden(status_bar_layer_get_layer(status_bar), true);
  }

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "4Loading loading background");

  s_loading_text_layer = text_layer_create((GRect) { .origin = {0, 0}, .size = {bounds.size.w, bounds.size.h + statusBarOffset}});
  //text_layer_set_text_color(s_loading_text_layer, GColorWhite);
  text_layer_set_background_color(s_loading_text_layer, GColorFromHEX(0xA12E37));
  //text_layer_set_text(s_loading_text_layer, "LOADING");
  //text_layer_set_font(s_loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  //text_layer_set_text_alignment(s_loading_text_layer, GTextAlignmentCenter);
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  layer_add_child(window_layer, text_layer_get_layer(s_loading_text_layer));

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "5Loading Gif");

  #if !defined(PBL_PLATFORM_DIORITE) && !defined(PBL_PLATFORM_APLITE)
  apngEnabled = true;
  s_bitmap_layer = bitmap_layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
  load_sequence();
  layer_set_hidden(bitmap_layer_get_layer(s_bitmap_layer), true);
  #endif

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_INFO, "Window load completed");

}

static void menu_window_unload(Window *window) {
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Begin destroying windows");
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_menu_layer windows");
  menu_layer_destroy(s_menu_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_error_text_layer windows");
  text_layer_destroy(s_error_text_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_upgrade_text_layer windows");
  text_layer_destroy(s_upgrade_text_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_loading_text_layer windows");
  text_layer_destroy(s_loading_text_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying status_bar windows");
  status_bar_layer_destroy(status_bar);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_bitmap_layer windows");
  if (apngEnabled)
  bitmap_layer_destroy(s_bitmap_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_text_layer windows");
  text_layer_destroy(s_text_layer);
  text_layer_destroy(s_title_text_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_scroll_layer windows");
  scroll_layer_destroy(s_scroll_layer);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_menu_icon_white windows");
  gbitmap_destroy(s_menu_icon_white);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Destroying s_menu_icon_black windows");
  gbitmap_destroy(s_menu_icon_black);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "End destroying windows");


}



static void init(void) {
  app_message_register_inbox_received(inbox_received_handler);
  //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Max size for inbox: %u",(int)app_message_inbox_size_maximum());
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Max size for outbox: %u",(int)app_message_outbox_size_maximum());

  // aplite check
  //app_message_open(6364, 6364);
  #if defined(PBL_PLATFORM_DIORITE) || defined(PBL_PLATFORM_APLITE)
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"MAX_INBOX_BUFFER %d",6364);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"MAX_OUTBOX_BUFFER %d",6364);
  app_message_open(MAX_INBOX_BUFFER,MAX_OUTBOX_BUFFER);
  #else
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"MAX_INBOX_BUFFER %d",MAX_INBOX_BUFFER);
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"MAX_OUTBOX_BUFFER %d",MAX_OUTBOX_BUFFER);
  app_message_open(MAX_INBOX_BUFFER,MAX_OUTBOX_BUFFER);
  #endif

  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"INITIALIZING...");
  if (DEBUG_ENABLED)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"APP VERSION: %s", VERSION);

  vector_init(&pendingIndexList);
  vector_init(&folderIndexList);
  loading = true;

  
    if (RESET_DATA) {
      persist_delete(PERSIST_LIST_SIZE);
      persist_delete(PERSIST_LIST);
      persist_delete(PERSIST_LIST_BYTE_LENGTH);
      persist_delete(PERSIST_VERSION_3_1);
      persist_delete(PERSIST_VERSION_3_0);
      persist_delete(PERSIST_VERSION_4_0_0);
    }


    backgroundColor = GColorWhite;
    foregroundColor = GColorWhite;
    highlightColor = GColorFromHEX(0xA12E37);
    backgroundIsDark = is_hex_string_dark("FFFFFF");
    statusBarColor = GColorFromHEX(0xA12E37);

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  window_stack_push(s_menu_window, false);

  if (persist_exists(PERSIST_VERSION_4_0_0)) {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Version persistent store found. Assuming version post-4.0.0");
    if (persist_exists(PERSIST_LIST_SIZE)){
      if (DEBUG_ENABLED)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Found presistent store. Loading into memory.");
      int tempSize = persist_read_int(PERSIST_LIST_SIZE);
      update_menu_data(tempSize);
      //persist_delete(PERSIST_LIST_SIZE);

      if(s_buffer != NULL) {
        free(s_buffer);
        s_buffer = NULL;
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed s_buffer memory");
      }
      if(chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
        if (DEBUG_ENABLED)
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed chunk_buffer memory");
      }
    }
    if (layer_get_hidden(text_layer_get_layer(s_error_text_layer)) && listString == 0) {
      text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    }
    menu_layer_reload_data(s_menu_layer);
  } else {
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Version persistent store empty. Assuming version pre-3.1");
    if (DEBUG_ENABLED)
      APP_LOG(APP_LOG_LEVEL_DEBUG,"DELETING!");

    if (persist_exists(PERSIST_LIST_SIZE))
      persist_delete(PERSIST_LIST_SIZE);
    if (persist_exists(PERSIST_LIST))
      persist_delete(PERSIST_LIST);
    if (persist_exists(PERSIST_LIST_BYTE_LENGTH))
      persist_delete(PERSIST_LIST_BYTE_LENGTH);
    if (persist_exists(PERSIST_VERSION_3_1))
      persist_delete(PERSIST_VERSION_3_1);
    if (persist_exists(PERSIST_VERSION_3_0))
      persist_delete(PERSIST_VERSION_3_0);
    if (persist_exists(PERSIST_VERSION_4_0_0))
      persist_delete(PERSIST_VERSION_4_0_0);

    text_layer_set_text(s_upgrade_text_layer, MSG_ERR_UPGRADE_VERSION);
    layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), false);
    menu_layer_reload_data(s_menu_layer);
  }
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
  loaded = true;
  loading = false;
}

static void deinit(void) {
  window_destroy(s_menu_window);
  Stack_Deinit(&menuLayerStack);
  vector_free(&pendingIndexList);
  vector_free(&folderIndexList);
  //free_all_data();
}

int main(void) {


  init();
  app_event_loop();
  deinit();
}
