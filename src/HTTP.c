#include <pebble.h>

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

#define STACK_MAX 100

static const char VERSION[] = "3.1.0";

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

static const char *MSG_ERR_EMPTY_FOLDER = "Folder is empty.";
static const char *MSG_ERR_EMPTY_LIST = "Call list is empty!\nConfigure requests on your phone.";
static const char *MSG_ERR_UPGRADE_VERSION = "NEW VERSION DETECTED\nResend requests from your phone.";

static char s_item_text[32];
static char *s_buffer = NULL;
static char *listAction = NULL;
static char ***theList = NULL;
static char ***statusList = NULL;
static int *folderSizeList = NULL;
static char *listString = NULL;
static int listSize = 0;
static bool loaded = false;
static ClickConfigProvider previous_ccp;
static DictionaryIterator *dict;

// var for chunking 
static int currentChunkIndex = 0;
static char *chunk_buffer = NULL;
static int listByteSize = 0;
static int persistentListIndex = 0;

// var for settings
static int vibrationLength = 100;

enum {
  PERSIST_LIST,
  PERSIST_LIST_SIZE,        // number of folders+requests
  PERSIST_LIST_BYTE_LENGTH, // length in byte of entire list
  PERSIST_VERSION_3_0,
  PERSIST_VERSION_3_1

};

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
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Error: stack full\n");
}

void Stack_Pop(Stack *S)
{
    if (S->size == 0)
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Error: stack empty\n");
    else
        S->size--;
}
static Stack menuLayerStack;
static Stack menuRowStack;

static void reset_menu_index(MenuLayer * layer, int toRow) {

    MenuIndex idx = menu_layer_get_selected_index(layer);
    idx.row = toRow;
    menu_layer_set_selected_index(layer,idx,MenuRowAlignCenter,false);
}

static void previous_button_hander(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "POPPY ALLLL");

}

static void return_cpp(void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "calling the new ccp");
  previous_ccp(context);
  window_single_click_subscribe(BUTTON_ID_BACK, previous_button_hander);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "done in the new ccp");

}
static void release_back_button(Window *window, MenuLayer *menu_layer) {
  //menu_window_unload(window);
  window_set_click_config_provider_with_context(window, return_cpp, menu_layer);
}


static void back_button_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Back button handler invoked");
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  reset_menu_index(s_menu_layer,Stack_Top(&menuRowStack));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Error layer set to hidden");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stack_Top(&menuLayerStack): %d", Stack_Top(&menuLayerStack));
  Stack_Pop(&menuLayerStack);
  Stack_Pop(&menuRowStack);
  menu_layer_reload_data(s_menu_layer);

  if (Stack_Top(&menuLayerStack) == 0){

    APP_LOG(APP_LOG_LEVEL_DEBUG, "HERE");
    release_back_button(s_menu_window, s_menu_layer);

  }
}


static void new_ccp(void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "calling the new ccp");
  previous_ccp(context);
  window_single_click_subscribe(BUTTON_ID_BACK, back_button_handler);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "done in the new ccp");
}

static void force_back_button(Window *window, MenuLayer *menu_layer) {
  window_set_click_config_provider_with_context(window, new_ccp, menu_layer);
}


static void send_to_phone() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing data to send to Phone");

  if (listSize == 0) return;
  app_message_outbox_begin(&dict);

  int folderIndex = (int) Stack_Top(&menuLayerStack);
  int rowIndex = (int) menu_layer_get_selected_index(s_menu_layer).row;
  
  if (strcmp(statusList[folderIndex][rowIndex],"Ready") != 0 &&
     strcmp(statusList[folderIndex][rowIndex],"Pending...") != 0) {
    free (statusList[folderIndex][rowIndex]);
    statusList[folderIndex][rowIndex] = NULL;
   APP_LOG(APP_LOG_LEVEL_DEBUG, "?Preparing data to send to Phone");
  }
  menu_layer_reload_data(s_menu_layer);
  statusList[folderIndex][rowIndex] = "Pending...";
  APP_LOG(APP_LOG_LEVEL_DEBUG, "statusList[%d][%d] set to Waiting...",folderIndex, rowIndex);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Menu index to send: %d", rowIndex);
  dict_write_uint8(dict,KEY_INDEX,rowIndex);
  dict_write_uint8(dict,KEY_FOLDER_INDEX,folderIndex);
  dict_write_cstring(dict,KEY_ACTION,"response");
  const uint32_t final_size = dict_write_end(dict);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

void chunk_timer_callback(void *data) {
  AppMessageResult amr; 
  DictionaryResult dr;
  amr = app_message_outbox_begin(&dict);

  if(amr == APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_OK");

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunk index to send: %d", currentChunkIndex);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_CHUNK_INDEX);
    dict_write_uint8(dict,KEY_CHUNK_INDEX,currentChunkIndex);

    dr = dict_write_cstring(dict,KEY_ACTION,"chunk");
    const uint32_t final_size = dict_write_end(dict);

    if (dr == DICT_OK) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_OK");
    else if (dr == DICT_NOT_ENOUGH_STORAGE) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_NOT_ENOUGH_STORAGE");
    else if (dr == DICT_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_INVALID_ARGS");
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
    app_message_outbox_send();

  }
  else if(amr == APP_MSG_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_INVALID_ARGS");
  else if(amr == APP_MSG_BUSY) 
  {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_BUSY"); 
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Sleeping for 500 milliseconds..."); 
    app_timer_register(500, chunk_timer_callback, NULL);
  }

}

static void send_version_to_phone() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to send version to Phone");

  //if (listSize == 0) return;


  app_message_outbox_begin(&dict);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_VERSION);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message version to send: %s", VERSION);
  dict_write_cstring(dict,KEY_VERSION,VERSION);
  dict_write_cstring(dict,KEY_ACTION,"version");
  const uint32_t final_size = dict_write_end(dict);

  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void send_command_error_to_phone (char *cmd) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to send error to Phone");

  //if (listSize == 0) return;

  app_message_outbox_begin(&dict);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_ERROR);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message error to send: %s", cmd);
  dict_write_cstring(dict,KEY_ERROR,cmd);
  dict_write_cstring(dict,KEY_ACTION,"error");
  const uint32_t final_size = dict_write_end(dict);

  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
  app_message_outbox_send();
}

static void request_next_chunk_from_phone() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Preparing to request next chunk from Phone");

  //if (listSize == 0) return;

  AppMessageResult amr; 
  DictionaryResult dr;

  amr = app_message_outbox_begin(&dict);

  if(amr == APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_OK");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunk index to send: %d", currentChunkIndex);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message index to send: %d", KEY_CHUNK_INDEX);
    dict_write_uint8(dict,KEY_CHUNK_INDEX,currentChunkIndex);
    dr = dict_write_cstring(dict,KEY_ACTION,"chunk");
    const uint32_t final_size = dict_write_end(dict);

    if (dr == DICT_OK) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_OK");
    else if (dr == DICT_NOT_ENOUGH_STORAGE) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_NOT_ENOUGH_STORAGE");
    else if (dr == DICT_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"DICT_INVALID_ARGS");
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to phone! (%d bytes)", (int) final_size);
    app_message_outbox_send();
  }
  else if(amr == APP_MSG_INVALID_ARGS) APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_INVALID_ARGS");
  else if(amr == APP_MSG_BUSY) 
  {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"APP_MSG_BUSY"); 
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Sleeping for 500 milliseconds..."); 
    app_timer_register(500, chunk_timer_callback, NULL);
  }

}

static void free_all_data() {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Starting free all procedure");

  APP_LOG(APP_LOG_LEVEL_DEBUG, "The listSize is %d", listSize);

/*
  if(s_buffer != NULL) {
    free(s_buffer);
    s_buffer = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed s_buffer memory");
  }
*/
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
}

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

static void update_menu_data(int stringSize) {

  //free_all_data();
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  loaded = false;

  menu_layer_reload_data(s_menu_layer);

  if (stringSize == 0) {

    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
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

    

    while (bytesRemaining > 0) {
      char *buf= malloc(PERSIST_DATA_MAX_LENGTH);


      APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading chunk %d from persistent store", currentPersistenceIndex);
      int readByteSize = persist_read_string(PERSIST_LIST+currentPersistenceIndex,buf,PERSIST_DATA_MAX_LENGTH);
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


    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read list from persistent storage: %s", listString);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read size (value) from persistent storage: %d", listSize);

  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Processing stringSize: %i",stringSize);
    listSize = stringSize;
    listString = (char*)malloc((strlen(s_buffer)+1)*sizeof(char));
    memcpy(listString, s_buffer, (strlen(s_buffer)+1)*sizeof(char));
    //strcpy(listString,s_buffer);
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
    if (startsWith(listString, "_F")) {
      chopStringBy(listString,2);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n\n",listString);

      char * folderSizeStr = extract_between(listString,"_","_");
      int folderSize    = atoi(folderSizeStr);
      chopStringBy(listString,1+ strlen(folderSizeStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) folderSize %d",folderSize);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(folderSizeStr) %d",strlen(folderSizeStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);

      char * folderIndexStr = extract_between(listString,"_","_");
      int folderIndex    = atoi(folderIndexStr);
      chopStringBy(listString,1+ strlen(folderIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) folderIndex %d",folderIndex);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) strlen(folderIndexStr) %d",strlen(folderIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) ListString %s\n",listString);

      char * folderParentIndexStr = extract_between(listString,"_","_");
      int folderParentIndex    = atoi(folderParentIndexStr);
      chopStringBy(listString,1+ strlen(folderParentIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) folderParentIndex %d",folderParentIndex);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) strlen(folderParentIndexStr) %d",strlen(folderParentIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) ListString %s\n",listString);

      char * folderRowStr = extract_between(listString,"_","_");
      int folderRow    = atoi(folderRowStr);
      chopStringBy(listString,1+ strlen(folderRowStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "4) folderRow %d",folderRow);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "4) strlen(folderRowStr) %d",strlen(folderRowStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "4) ListString %s\n",listString);

      char * folderName = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(folderName));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "5) folderRow %d",folderRow);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "5) strlen(folderRowStr) %d",strlen(folderName));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "5) ListString %s\n",listString);

      if (folderSize != 0) {
        theList[folderIndex] = (char **) malloc(folderSize * sizeof(char*));
        statusList[folderIndex] = (char **) malloc(folderSize * sizeof(char*));
      } 

      
      folderSizeList[folderIndex] = folderSize;

      if (folderParentIndex != -1 && folderRow != -1) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing folder name to theList[%d][%d]: %s",folderParentIndex,folderRow,folderName);
        theList[folderParentIndex][folderRow] = (char*)malloc((strlen(folderName)+1) * sizeof(char));
        memcpy(theList[folderParentIndex][folderRow],folderName+'\0',(strlen(folderName)+1) * sizeof(char));
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing status name to theList[%d][%d]: %s",folderParentIndex,folderRow,"F");
        statusList[folderParentIndex][folderRow] = strcat(folderIndexStr,"_");

        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking theList[%d][%d]: %s",folderParentIndex,folderRow,theList[folderParentIndex][folderRow]);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking statusList[%d][%d]: %s",folderParentIndex,folderRow,statusList[folderParentIndex][folderRow]);
      }
      parseSuccessful = true;
    } else if (startsWith(listString, "_E")){
      chopStringBy(listString,2);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * parentIndexStr = extract_between(listString,"_","_");
      int parentIndex    = atoi(parentIndexStr);
      chopStringBy(listString,1+ strlen(parentIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) parentIndex %d",parentIndex);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(parentIndexStr) %d",strlen(parentIndexStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);

      char * entryRowStr = extract_between(listString,"_","_");
      int entryRow    = atoi(entryRowStr);
      chopStringBy(listString,1+ strlen(entryRowStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) entryRow %d",entryRow);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) strlen(entryRowStr) %d",strlen(entryRowStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "2) ListString %s\n",listString);

      char * entryName = extract_between(listString,"_","_");
      chopStringBy(listString,1+ strlen(entryName));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) entryName %s",entryName);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) strlen(entryName) %d",strlen(entryName));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "3) ListString %s\n",listString);

      APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing entry name to theList[%d][%d]: %s",parentIndex,entryRow,entryName);
      theList[parentIndex][entryRow] = (char*)malloc((strlen(entryName)+1) * sizeof(char));
      memcpy(theList[parentIndex][entryRow],entryName+'\0',(strlen(entryName)+1) * sizeof(char));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing status name to theList[%d][%d]: %s",parentIndex,entryRow,"Ready");
      statusList[parentIndex][entryRow] = "Ready";
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking theList[%d][%d]: %s",parentIndex,entryRow,theList[parentIndex][entryRow]);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Doublechecking statusList[%d][%d]: %s",parentIndex,entryRow,statusList[parentIndex][entryRow]);

      parseSuccessful = true;
    } else if (startsWith(listString, "_V")) {
      chopStringBy(listString,2);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "0) ListString %s\n",listString);

      char * vibrationLengthStr = extract_between(listString,"_","_");
      vibrationLength    = atoi(vibrationLengthStr);
      chopStringBy(listString,1+ strlen(vibrationLengthStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) vibtrationLengthInt %d",vibrationLength);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) strlen(vibrationLengthStr) %d",strlen(vibrationLengthStr));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "1) ListString %s\n",listString);

    }
    if (!parseSuccessful) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "...nothing more to do...");
      break;
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "...continuing...");
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Completed data stream parsing.");
  //free(theList[0][1]);
  //theList[0][1] = NULL;
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d][%d]: %s",0,4, theList[0][3]);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d][%d]: %s",0,4, theList[0][4]);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d][%d]: %s",0,4, theList[0][5]);


  if (listSize != 0) {
    for (int o=0; o < listSize; o++){
      for (int i=0; i < folderSizeList[o]; i++){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Data[%d][%d]: %s",o,i, theList[o][i]);
      }
    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "update_menu_data completed");

  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
  loaded = true;

  menu_layer_reload_data(s_menu_layer);
  /*

  char * pch = NULL;
  int i;
  i = 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Splitting string \"%s\" into tokens",listString);
  pch = strtok (listString,",");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "strtok'd");

  while (pch != NULL)
  {
    theList[i] = (char*)malloc((strlen(pch)+1) * sizeof(char)); // Add extra for end char
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
*/
  
}

void chopN(char *str, size_t n)
{
    size_t len = strlen(str);
    if (n > len)
        return;  // Or: n = len;
    memmove(str, str+n, len - n + 1);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  if (!loaded) {
    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    loaded = true;
  }

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received dict from phone of size: \"%d\"", sizeof(iter));

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

    uint32_t segments[] = { vibrationLength };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);

    Tuple *response_string = dict_find(iter, KEY_RESPONSE);
    Tuple *array_row_index = dict_find(iter,KEY_INDEX);
    Tuple *array_folder_index = dict_find(iter,KEY_FOLDER_INDEX);

    int folder_index_of_array = 0;
    int row_index_of_array = 0;

    if (array_folder_index){ folder_index_of_array = array_folder_index->value->int32; }
    if (array_row_index){ row_index_of_array = array_row_index->value->int32; }

    length = strlen(response_string->value->cstring);

    if (statusList[folder_index_of_array][row_index_of_array] != NULL) {
      //free(statusList[index_of_array] );
      statusList[folder_index_of_array][row_index_of_array]  = NULL;
    }
    statusList[folder_index_of_array][row_index_of_array] = (char*)malloc((length+1) * sizeof(char));
    memcpy(statusList[folder_index_of_array][row_index_of_array],response_string->value->cstring,(length +1) * sizeof(char));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "HTTP Response: %s", statusList[folder_index_of_array][row_index_of_array]);
    menu_layer_reload_data(s_menu_layer);


  } else if (strcmp(listAction, "chunk")==0) {


    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
    loaded = false;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Found chunking call");

    char *string_chunk = dict_find(iter, KEY_LIST)->value->cstring; 

    // make sure that buffer is clear before starting new chunking activity
    if (currentChunkIndex == 0) { 
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing chunk_buffer");


      int list_string_length = dict_find(iter, KEY_CHUNK_SIZE)->value->int32;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "list_string_length is : %d", list_string_length);

      if (chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
      }

      chunk_buffer = (char*)malloc((list_string_length+1) * sizeof(char));

      // Copy the first chunk to chunk buffer
      strcpy(chunk_buffer, string_chunk);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer strcpy result: %s", chunk_buffer);

    } else {
      strcat(chunk_buffer, string_chunk);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer strcat result: %s", chunk_buffer);
    }

    currentChunkIndex = currentChunkIndex + 1;
    
    request_next_chunk_from_phone();

  } else if (strcmp(listAction, "version")==0) {
    send_version_to_phone();
  } else if (strcmp(listAction, "update")==0) {

    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
    loaded = false;

    int array_size = dict_find(iter,KEY_SIZE)->value->int32;;
    char *array_string = dict_find(iter, KEY_LIST)->value->cstring;

    listSize = array_size;

    free_all_data();

    if (array_size > 0 && !layer_get_hidden(text_layer_get_layer(s_error_text_layer))) {

      text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
    }

    if (currentChunkIndex > 0) {
      currentChunkIndex = 0;

      strcat(chunk_buffer, array_string);
      array_string = chunk_buffer;

      APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk_buffer concat result: %s", chunk_buffer);
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

      APP_LOG(APP_LOG_LEVEL_DEBUG, "List length: %d", length);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Size retrieved from phone: %d", array_size);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "List retrieved from phone: %s", s_buffer);

      int bytesWritten = 0;
      int bytesSizeWritten = 0;
      int bytesLengthWritten = 0;

      if (array_size == 0) {
        bytesWritten = persist_write_string(PERSIST_LIST,"");
        bytesWritten = persist_write_string(PERSIST_VERSION_3_1,VERSION);
        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,0);
        bytesLengthWritten = persist_write_int(PERSIST_LIST_BYTE_LENGTH,0);

        APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing Empty values to persistent store.");

      } else {

        // write into persistent store PERSIST_DATA_MAX_LENGTH - 1 bytes at a time
        int bytesRemaining = length;
        persistentListIndex = 0; 
        
        while (bytesRemaining > 0) {

          //char indexToInt[15];
          //sprintf(indexToInt, "%d", persistentListIndex);

          char *buf= malloc(PERSIST_DATA_MAX_LENGTH - 1);
          strncpy(buf, s_buffer, PERSIST_DATA_MAX_LENGTH - 1);
          //buf[PERSIST_DATA_MAX_LENGTH - 1] = '\0';

          bytesWritten = persist_write_string(PERSIST_LIST + persistentListIndex,buf);
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Chunked into persistent store: %s", s_buffer);
          chopN(s_buffer, PERSIST_DATA_MAX_LENGTH - 1);
          bytesRemaining = bytesRemaining - PERSIST_DATA_MAX_LENGTH - 1;
          persistentListIndex = persistentListIndex - 1;
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (list): %d", bytesWritten);
          free(buf);
          buf = NULL;
          bytesWritten = persist_write_string(PERSIST_VERSION_3_1,VERSION);
        }


        bytesSizeWritten = persist_write_int(PERSIST_LIST_SIZE,array_size);
        bytesLengthWritten = persist_write_int(PERSIST_LIST_BYTE_LENGTH,length);

        APP_LOG(APP_LOG_LEVEL_DEBUG, "Written to persistent storage (size): %d", bytesSizeWritten);
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
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed s_buffer memory");
      }
      if(chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed chunk_buffer memory");
      }


      layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
      loaded = true;
      menu_layer_reload_data(s_menu_layer);
    }
  } else {
    send_command_error_to_phone(listAction);
  }
  if (listAction != NULL){
    free(listAction);
    listAction = NULL;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed listAction memory");

  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Inbox handler completed");
}

static void select_callback(struct MenuLayer *s_menu_layer, MenuIndex *cell_index, 
                            void *callback_context) {

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
  } else if (folderSizeList[Stack_Top(&menuLayerStack)] == 0) {
    text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_FOLDER);
    layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
  } else if (strstr(statusList[Stack_Top(&menuLayerStack)][cell_index->row], "_") != NULL) { // If the status has a '_' char, we know it's a folder
    char * currentStatus = statusList[Stack_Top(&menuLayerStack)][cell_index->row];
    if(Stack_Top(&menuLayerStack) == 0) {
      force_back_button(s_menu_window, s_menu_layer);
    }
    char *stringcopy = malloc ((strlen(currentStatus)+1)*sizeof(char));
    if (stringcopy)
    {
      strcpy(stringcopy,currentStatus);
      stringcopy[strlen(stringcopy)-1] = 0;

    }
    Stack_Push(&menuLayerStack,atoi(stringcopy));
    Stack_Push(&menuRowStack,cell_index->row);
    reset_menu_index(s_menu_layer,0);
    menu_layer_reload_data(s_menu_layer);
    free(stringcopy);
    stringcopy = NULL;

    // Check if we're now in a folder that's empty. If we are, then show error
    if (folderSizeList[Stack_Top(&menuLayerStack)] == 0) {
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

  if (listSize == 0) return;
  
  bool isEmptyFolder = folderSizeList[Stack_Top(&menuLayerStack)]==0;

  // if the folder's empty, draw nutt'n
  if (!isEmptyFolder){
    char* name = theList[Stack_Top(&menuLayerStack)][cell_index->row]; //theList TODO
    char * currentStatus = statusList[Stack_Top(&menuLayerStack)][cell_index->row];
    if (currentStatus != NULL) {
      if (strstr(currentStatus, "_") != NULL) { // If the status has a '_' char, we know it's a folder

        // Set the status field as "Open Folder"
        snprintf(s_item_text, sizeof(s_item_text), "%s", "Open Folder");
      } else { // otherwise it's a request entry

        // Pad the status field with "Status: " + <status of request>
        snprintf(s_item_text, sizeof(s_item_text), "Status: %s", statusList[Stack_Top(&menuLayerStack)][cell_index->row]);
      }
    }

    // Using simple space padding between name and s_item_text for appearance of edge-alignment
    menu_cell_basic_draw(ctx, cell_layer, name, s_item_text, NULL);
  }
  
}



static void menu_window_load(Window *window) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Loading Window");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "listSize: %d", listSize);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "listString: %s", listString); 

  Stack_Init(&menuLayerStack,listSize); 

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  APP_LOG(APP_LOG_LEVEL_INFO, "1Loading Window");

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_sections_count_callback,
    .get_cell_height = PBL_IF_ROUND_ELSE(get_cell_height_callback, NULL),
    .draw_row = draw_row_handler,
    .select_click = select_callback
  });
  APP_LOG(APP_LOG_LEVEL_INFO, "2Loading Window");

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
  previous_ccp = window_get_click_config_provider(window);

  APP_LOG(APP_LOG_LEVEL_INFO, "3Loading Window");
  s_error_text_layer = text_layer_create((GRect) { .origin = {0, 44}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
  text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_error_text_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_error_text_layer, GColorWhite);
  text_layer_set_background_color(s_error_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_error_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));

  s_upgrade_text_layer = text_layer_create((GRect) { .origin = {0, 44}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_upgrade_text_layer, MSG_ERR_UPGRADE_VERSION);
  text_layer_set_font(s_upgrade_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_upgrade_text_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_upgrade_text_layer, GColorWhite);
  text_layer_set_background_color(s_upgrade_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), true);
  layer_add_child(window_layer, text_layer_get_layer(s_upgrade_text_layer));

  APP_LOG(APP_LOG_LEVEL_INFO, "4Loading Window");
  s_loading_text_layer = text_layer_create((GRect) { .origin = {0, 60}, .size = {bounds.size.w, 60}});
  text_layer_set_text(s_loading_text_layer, "LOADING");
  text_layer_set_font(s_loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_loading_text_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_loading_text_layer, GColorWhite);
  text_layer_set_background_color(s_loading_text_layer, GColorBlack);
  layer_set_hidden(text_layer_get_layer(s_loading_text_layer), false);
  layer_add_child(window_layer, text_layer_get_layer(s_loading_text_layer));
  APP_LOG(APP_LOG_LEVEL_INFO, "Window load completed");
}

static void menu_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_error_text_layer);
  text_layer_destroy(s_upgrade_text_layer);
  text_layer_destroy(s_loading_text_layer);
}



static void init(void) {

  APP_LOG(APP_LOG_LEVEL_DEBUG,"INITIALIZING...");
  APP_LOG(APP_LOG_LEVEL_DEBUG,"APP VERSION: %s", VERSION);

  /*
    persist_delete(PERSIST_LIST_SIZE);
    persist_delete(PERSIST_LIST);
    persist_delete(PERSIST_LIST_BYTE_LENGTH);
    persist_delete(PERSIST_VERSION_3_1);
    persist_delete(PERSIST_VERSION_3_0);
*/

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  window_stack_push(s_menu_window, false);
  app_message_register_inbox_received(inbox_received_handler);
  //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Max size for inbox: %u",(int)app_message_inbox_size_maximum());
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Max size for outbox: %u",(int)app_message_outbox_size_maximum());

  // aplite check
  //app_message_open(6364, 6364);
  app_message_open(MAX_INBOX_BUFFER,MAX_OUTBOX_BUFFER);
  if (persist_exists(PERSIST_VERSION_3_1)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Version persistent store found. Assuming version post-3.1");
    if (persist_exists(PERSIST_LIST_SIZE)){
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Found presistent store. Loading into memory.");
      int tempSize = persist_read_int(PERSIST_LIST_SIZE);
      update_menu_data(tempSize);
      //persist_delete(PERSIST_LIST_SIZE);

      if(s_buffer != NULL) {
        free(s_buffer);
        s_buffer = NULL;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed s_buffer memory");
      }
      if(chunk_buffer != NULL) {
        free(chunk_buffer);
        chunk_buffer = NULL;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed chunk_buffer memory");
      }
    }
    if (layer_get_hidden(text_layer_get_layer(s_error_text_layer)) && listString == 0) {
      text_layer_set_text(s_error_text_layer, MSG_ERR_EMPTY_LIST);
      layer_set_hidden(text_layer_get_layer(s_error_text_layer), false);
    }
    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    loaded = true;
    menu_layer_reload_data(s_menu_layer);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Version persistent store empty. Assuming version pre-3.1");
    APP_LOG(APP_LOG_LEVEL_DEBUG,"DELETING!");
    
    persist_delete(PERSIST_LIST_SIZE);
    persist_delete(PERSIST_LIST);
    persist_delete(PERSIST_LIST_BYTE_LENGTH);
    persist_delete(PERSIST_VERSION_3_1);
    persist_delete(PERSIST_VERSION_3_0);

    text_layer_set_text(s_upgrade_text_layer, MSG_ERR_UPGRADE_VERSION);
    layer_set_hidden(text_layer_get_layer(s_upgrade_text_layer), false);
    layer_set_hidden(text_layer_get_layer(s_loading_text_layer), true);
    loaded = true;
    menu_layer_reload_data(s_menu_layer);
  }
}

static void deinit(void) {
  window_destroy(s_menu_window);
  Stack_Deinit(&menuLayerStack);
  //free_all_data();
}

int main(void) {


  init();
  app_event_loop();
  deinit();
}
