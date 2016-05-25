

function sendHttpRequest(ToUrl,withJson,folderIndex,rowIndex,method) {

  var xhr = new XMLHttpRequest();
  xhr.timeout = 10000;

  if (method == "PUT"){
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from PUT:");
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    };

    xhr.open(method, ToUrl);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(withJson);

  } else if (withJson !== "") {

    // METHOD JSON POST
    xhr.onreadystatechange = function() {
        if (xhr.readyState === 4) {
          console.log("Received response from POST:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    };
    var strToJson = JSON.parse(withJson);
    xhr.open(method, ToUrl, true);

    // Have to to XMLHttpRequest because we dont have jquery :(
    // Testing was done via jquery ajax so results MAY be different
    // Particularly the request content-type (json vs form)
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    var parameterizedUrl = Object.keys(strToJson).map(function(k) {
      return encodeURIComponent(k) + '=' + encodeURIComponent(strToJson[k])
    }).join('&');
    console.log("XMLHttpRequest sending : " + parameterizedUrl);
    xhr.send(parameterizedUrl);
    /*
    $.ajax({
      method: "POST",
      url: ToUrl,
      data: JSON.parse(withJson),
      dataType: "json",
      success: function(data){
        console.log("Successfully sent POST"); 
        console.log("Results: " + JSON.stringify(data));
      },
      failure: function(errMsg) {
        console.log("Failed to send POST");
        console.log("Results: " + JSON.stringify(errMsg));
      },
      error: function(jqXHR, textStatus, errorThrown) {
        console.log("HTTP POST sent: ")
        console.log(jqXHR);
      }
    });
*/
  } else {
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from GET:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    }
    xhr.open(method, ToUrl, true);
    console.log("Using method: " + method);
    console.log("Using ToUrl: " + ToUrl);
    try {
      xhr.send(null);  
    } catch (err) {
      console.log("Error sending XMLHttpRequest: " + JSON.stringify(err));
      sendHttpResponseToPebble("0",folderIndex,rowIndex);
    }
    
    /*
    $.ajax({
      method: "GET",
      url: ToUrl,
      success: function(data){
        console.log("Successfully sent GET");
        console.log("Results: " + JSON.stringify(data));
      },
      failure: function(errMsg) {
          alert(errMsg);
      },
      error: function(jqXHR, textStatus, errorThrown) {
        console.log("HTTP GET sent: ")
        console.log(jqXHR);
      }
    });
*/
  }
}

Pebble.addEventListener('showConfiguration', function() {
  var url = 'http://skonagaya.github.io';
  //var url = 'http://127.0.0.1:8080';
  //var url = 'http://5fa77084.ngrok.io';

  if (getLocalVersion() != '') url = url + "/upgrade/";

  console.log('Showing configuration page: ' + url);

  Pebble.openURL(url);
});

function traverseList (nextList) {
  var currentLevelList = [];
  for (var i=0; i < nextList.length; i++) {
    if (nextList[i]["type"] == "request") {
      currentLevelList.push(nextList[i]["name"]);
    } else if (nextList[i]["type"] == "folder"){
      var folderList = traverseList(nextList[i]["list"]);
      var entry = {}
      entry[nextList[i]["name"]] = folderList
      currentLevelList.push(entry);
    }
  }
  return currentLevelList;
}


function getLocalVersion() {
  var localList = localStorage.getItem("version");
  var version = '';
  if (localList !== null) {
    version = localList;
  }
  console.log("getLocalVersion returned: \""+version+"\"");
  return version;
}

function traverseListString (nextList,folderIndex) {
  
  var currentLevelList = "";
  var parentIndex = folderIndex;

  for (var i=0; i < nextList.length; i++) {
    if (nextList[i]["type"] == "request" || nextList[i]["type"] === undefined) {
      console.log("Found request type");

      console.log("parentIndex.toString()+i.toString(): " + parentIndex.toString()+i.toString());
      console.log("JSON.stringify(nextList[i]): " + (JSON.stringify(nextList[i])));

      currentLevelList =  currentLevelList + "_E"
                          + "_" + parentIndex 
                          + "_" + i.toString() 
                          + "_" + nextList[i]["name"].replace("_","");
                          pebbleTables[parentIndex.toString()+i.toString()] = JSON.parse(JSON.stringify(nextList[i]));

    } else if (nextList[i]["type"] == "folder"){
      folderIndex = folderIndex + 1;
      var nextLevelData = traverseListString(nextList[i]["list"],folderIndex);
      var nextLevelString = nextLevelData[0];
      var nextIndexAfterFolderTraversal = nextLevelData[1];

      currentLevelList =  currentLevelList 
                          + "_F" 
                          + "_" + nextList[i]["list"].length.toString() 
                          + "_" + folderIndex 
                          + "_" + parentIndex 
                          + "_" + i.toString() 
                          + "_" + nextList[i]["name"].replace("_","")
                          + nextLevelString;

      folderIndex = nextIndexAfterFolderTraversal;

    }
  }
  return [currentLevelList,folderIndex];
}

function traverseCount (nextList) {
  var currentLevelCount = 0;
  for (var i=0; i < nextList.length; i++) {
    if (nextList[i]["type"] == "folder"){
      currentLevelCount = currentLevelCount + 1 + traverseCount(nextList[i]["list"]);
    }
  }
  return currentLevelCount;

}

function chunkString(str, length) {
  return str.match(new RegExp('.{1,' + length + '}', 'g'));
}


var pebbleTables = {};

function sendListToPebble(listArray,action) {

  console.log("Preparing to send list to initialize Pebble data");
  console.log("Creating flat datastructure for pebble mapping");

  //TODO: replace _ with space and replace . with empty character
  console.log("listArray: " + JSON.stringify(listArray));
  var listToString = "";
  var listToArray = null;
  var listCount = traverseCount(listArray) + 1;

  console.log("listArray.size = " + listCount.toString());

  pebbleTables = {};

  var trimmedList = "_F_" + listArray.length.toString() + "_0_-1_-1_Root" +traverseListString(listArray,0,0)[0]+"_";
  if (getLocalVersion() != "" && localStorage.getItem("settings") !== null) {
    console.log("Aint here");
    if (localStorage.getItem("settings")["vibration"] !== null){

    var vibrationStr = JSON.parse(localStorage.getItem("settings"))["vibration"].toString();
    console.log("Adding vibration String: "+vibrationStr);
    trimmedList = trimmedList + "V_"+vibrationStr+"_";
    }
  }
  listToString = JSON.stringify(trimmedList).slice(1, -1);
  listToArray = chunkString(listToString, 50);

  console.log("pebbleTables: " + JSON.stringify(pebbleTables));
  localStorage.setItem("pebble_tables", JSON.stringify(pebbleTables));

  console.log("List has been stringified to " + listToString);
  console.log("List has been chunkified to " + JSON.stringify(listToArray));
  
  var dict = {};

  // If the list is empty, send it to the watch to propagate "empty list error msg"

  if(listArray.length == 0) 
  {

    dict.KEY_LIST = "";
    dict.KEY_SIZE = 0;
    dict.KEY_ACTION = action;

    Pebble.sendAppMessage(dict, function() {
        console.log('Successfully sent empty list to pebble');
      }, function(e) {
        console.log('Failed to send empty list to pebble');
        console.log(JSON.stringify(e));
      }
    );
  } else {

    // default case: no need to chunk, just update pebble now..
    if (listToArray.length == 1) {
      console.log('No need to chunk.');
      sendChunkToPebble (listToString, listCount, unescape(encodeURIComponent(listToString)).length , "update");
    } else {
      listChunks = listToArray;
      finalListSize = listCount;
      sendChunkToPebble (listToArray[0], listCount, unescape(encodeURIComponent(listToString)).length, "chunk");
    }
    
  }
}

var chunkIndex = 0;
var listChunks = null;
var finalListSize = 0;
var chunkTotalByteSize = 0;

function sendChunkToPebble(listString, listSize, listStringLength, action) {
  var dict = {};

  dict.KEY_ACTION = action;
  dict.KEY_LIST = listString;
  dict.KEY_SIZE = listSize;
  dict.KEY_CHUNK_SIZE = listStringLength;

  console.log('Sending dict: ' + JSON.stringify(dict));

  Pebble.sendAppMessage(dict, function() {
      console.log('Successfully sent empty list to pebble');
    }, function(e) {
      console.log('Failed to send empty list to pebble');
      console.log(JSON.stringify(e));
    }
  );
}

function sendHttpResponseToPebble(responseStr,folderIndex,rowIndex) {
  var dict = {};
  dict['KEY_LIST'] = "";
  dict['KEY_SIZE'] = 0;
  dict['KEY_RESPONSE'] = responseStr;
  dict['KEY_ACTION'] = "response";
  dict['KEY_INDEX'] = rowIndex;
  dict['KEY_FOLDER_INDEX'] = folderIndex;
  console.log('Sending dict: ' + JSON.stringify(dict));

  Pebble.sendAppMessage(dict, function() {
      console.log('Successfully sent http response to pebble');
    }, function() {
      console.log('Failed to send http response to pebble');
    });
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (e.response === "") { 
    console.log("Configuration page returned nothing....");
  } else {
    var configData;
    try {
      configData = JSON.parse(decodeURIComponent(e.response));
    } catch (err){
      var stringToParse = JSON.stringify(e.response).replace(/%/g,'%25');
      console.log('URL Decode failed');
      if (stringToParse.indexOf('%') > -1) {
        configData = JSON.parse(decodeURIComponent(JSON.parse(stringToParse)));
      } else {
        configData = JSON.parse('{"array":[]}');
      }
    }
    console.log('Configuration page returned: ' + JSON.stringify(configData));
    console.log("Storing localStorage array stringified: " + JSON.stringify(configData['array']));
    localStorage.setItem("array", JSON.stringify(configData['array']));
    if (getLocalVersion() != '') {
      console.log("Storing localStorage settings stringified: " + JSON.stringify(configData['settings']));
      localStorage.setItem("settings", JSON.stringify(configData['settings']));
    }

    sendListToPebble(configData['array'],"update");
  }
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
  console.log('Version: 3.1!');
  var startFresh = false;

  var localList = localStorage.getItem("array");
  if (localList === null) {startFresh = true;}
  else {
    try { // CHECK FOR LOCAL STORAGE CORRUPTION. HAPPENS IN EMU NOT ON WATCH
      localList = JSON.parse(localList);
    } catch(e) {
      console.log('Local storage is corrupted!')
      startFresh = true;
    }
  }
  if (startFresh) {
      console.log('localStorage not found. This must be a fresh install!')
      console.log('Letting the pebble know we\'re shooting blanks.');
      sendListToPebble("","update");
  } else {
    var localList = JSON.parse(localStorage.getItem('array'));
    console.log(JSON.stringify(localList));

    for (i = 0; i < localList.length; i++) {
      if (localList[i]['method'] === undefined) {
        if (localList[i]['json'] == "") {
          localList[i]['method'] = "GET";
        } else {
          localList[i]['method'] = "POST";
        }
      }
    }

    localStorage.setItem("array", JSON.stringify(localList));

    if (!(localList === null)) {
      console.log('Sending data to Pebble');
      console.log(localList['array']);
      //sendListToPebble(localList,"update"); // LOAD INITIAL DATA (causes pull to pebble every time started)
    } else {
      console.log('localStorage not found. This must be a fresh install!');
      console.log('Letting the pebble know we\'re shooting blanks.');
      //sendListToPebble("","update");
    }
  }
  getPebbleAppVersion();

});

function getPebbleAppVersion() {
  console.log("Retreiving app version from Pebble");
  var dict = {};
  dict.KEY_LIST = "";
  dict.KEY_SIZE = 0;
  dict.KEY_ACTION = "version";
  Pebble.sendAppMessage(dict, function() {
      console.log('Successfully sent empty list to pebble');
    }, function(e) {
      console.log('Failed to send empty list to pebble');
      console.log(JSON.stringify(e));
    }
  );
}

Pebble.addEventListener("appmessage",
  function(e) {
    console.log("Got KEY_FOLDER_INDEX: "+ e.payload["KEY_FOLDER_INDEX"]);
    console.log("Got KEY_INDEX: "+ e.payload["KEY_INDEX"]);
    console.log("Got KEY_CHUNK_INDEX: "+ e.payload["KEY_CHUNK_INDEX"]);
    console.log("Got KEY_ACTION: "+ e.payload["KEY_ACTION"]);
    console.log("Got KEY_ERROR: "+ e.payload["KEY_ERROR"]);
    console.log("Got KEY_VERSION: "+ e.payload["KEY_VERSION"]);

    var action = e.payload["KEY_ACTION"];
    var folderIndex = parseInt(e.payload["KEY_FOLDER_INDEX"]);
    var rowIndex = parseInt(e.payload["KEY_INDEX"]);
    var selectedIndex = folderIndex.toString() + rowIndex.toString();
    var pebbleChunkIndex = (parseInt(e.payload["KEY_CHUNK_INDEX"]));
    var version = e.payload["KEY_VERSION"];

    console.log(JSON.stringify(e));

    if (pebbleChunkIndex == undefined) { console.log("==");}
    if (pebbleChunkIndex != undefined) { console.log("!=");}
    if (pebbleChunkIndex === undefined) { console.log("===");}
    if (pebbleChunkIndex !== undefined) { console.log("!==");}

    if (action == "chunk") {
      console.log("Received chunking message from Pebble");
      console.log("Using listChunks: " + JSON.stringify(listChunks));
      if (listChunks.length-1 == pebbleChunkIndex) { // if last chunk
        sendChunkToPebble(listChunks[pebbleChunkIndex], finalListSize, 0, "update");
      } else {
        sendChunkToPebble(listChunks[pebbleChunkIndex], finalListSize, 0, "chunk");
      }
    } else if (action == "version") {
      console.log("Received version message from Pebble");
      console.log("Setting localStorage version to " + version);
      localStorage.setItem("version", version);
    } else if (!(localStorage.getItem("pebble_tables")===null)) {
      console.log("Found existing list. Loading localStorage:");
      console.log(localStorage['pebble_tables']);
      var currentList = JSON.parse(localStorage['pebble_tables']);
      sendHttpRequest(
        currentList[selectedIndex]["endpoint"],
        currentList[selectedIndex]["json"],
        folderIndex,
        rowIndex,
        currentList[selectedIndex]["method"]

      );
    }
  }
);

