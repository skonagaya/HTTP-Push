

function sendHttpRequest(ToUrl,withJson,folderIndex,rowIndex,method) {

  var xhr = new XMLHttpRequest();
  xhr.timeout = 10000;

  if (method == "PUT"){
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from PUT:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    }

    xhr.open(method, ToUrl);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(withJson);

  } else if (withJson != "") {
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from POST:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    }
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
          console.log("Received response from POST:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),folderIndex,rowIndex);
        }
    }
    xhr.open(method, ToUrl, true);
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
  //var url = 'http://skonagaya.github.io/';
  var url = 'http://ebc9c45e.ngrok.io';

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

function traverseListString (nextList,parentIndex,folderIndex,pebbleTable) {
  var currentLevelList = "";
  for (var i=0; i < nextList.length; i++) {
    if (nextList[i]["type"] == "request" || nextList[i]["type"] === undefined) {

      currentLevelList =  currentLevelList + "_E"
                          + "_" + parentIndex 
                          + "_" + i.toString() 
                          + "_" + nextList[i]["name"].replace("_","");
                          pebbleTable[parentIndex.toString()+i.toString()] = JSON.parse(JSON.stringify(nextList[i]));

    } else if (nextList[i]["type"] == "folder"){
      folderIndex = folderIndex + 1;

      currentLevelList =  currentLevelList 
                          + "_F" 
                          + "_" + nextList[i]["list"].length.toString() 
                          + "_" + folderIndex 
                          + "_" + parentIndex 
                          + "_" + i.toString() 
                          + "_" + nextList[i]["name"].replace("_","")
                          + traverseListString(nextList[i]["list"],folderIndex,folderIndex,pebbleTable);

    }
  }
  return currentLevelList;
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

function sendListToPebble(listArray,action) {
  console.log("Preparing to send list to initialize Pebble data");

  console.log("Creating flat datastructure for pebble mapping")

  //TODO: replace _ with space and replace . with empty character
  console.log("listArray: " + JSON.stringify(listArray));
  var listToString = "";
  var listCount = traverseCount(listArray) + 1;

  console.log("listArray.size = " + listCount.toString());

  var pebbleTables = {};

  var trimmedList = "_F_" + listArray.length.toString() + "_0_-1_-1_Root" +traverseListString(listArray,0,0,pebbleTables)+"_";
  listToString = JSON.stringify(trimmedList).slice(1, -1);

  console.log("pebbleTables: " + JSON.stringify(pebbleTables));
  localStorage.setItem("pebble_tables", JSON.stringify(pebbleTables));

  console.log("List has been stringified to " + listToString);
  var dict = {};
  if(listArray.length > 0) {
    dict['KEY_LIST'] = listToString;
    dict['KEY_SIZE'] = listCount;
    dict['KEY_RESPONSE'] = "";
    dict['KEY_ACTION'] = action;
  } else {
    dict['KEY_LIST'] = "";
    dict['KEY_SIZE'] = 0;
    dict['KEY_RESPONSE'] = "";
    dict['KEY_ACTION'] = action;
  }
  console.log('Sending dict: ' + JSON.stringify(dict));

  Pebble.sendAppMessage(dict, function() {
      console.log('Successfully sent data to update pebble data');
    }, function() {
      console.log('Failed to send data to update pebble data');
    });
}

function sendHttpResponseToPebble(responseStr,folderIndex,rowIndex) {
  var dict = {};
  dict['KEY_LIST'] = "";
  dict['KEY_SIZE'] = 0;
  dict['KEY_RESPONSE'] = responseStr
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
  if (e.response == "") { 
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
    console.log("Storing localStorage stringified: " + JSON.stringify(configData['array']));
    localStorage.setItem("array", JSON.stringify(configData['array']));

    sendListToPebble(configData['array'],"update");
  }
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
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
      sendListToPebble(localList,"update");
    } else {
      console.log('localStorage not found. This must be a fresh install!')
      console.log('Letting the pebble know we\'re shooting blanks.');
      sendListToPebble("","update");
    }
  }
});

Pebble.addEventListener("appmessage",
  function(e) {
    var folderIndex = parseInt(e.payload["KEY_FOLDER_INDEX"]);
    var rowIndex = parseInt(e.payload["KEY_INDEX"]);
    var selectedIndex = folderIndex.toString() + rowIndex.toString();
    console.log("Got KEY_FOLDER_INDEX: ", e.payload["KEY_FOLDER_INDEX"]);
    console.log("Got KEY_INDEX: ", e.payload["KEY_INDEX"]);
    console.log(JSON.stringify(e));

    if (!(localStorage.getItem("pebble_tables")===null)) {
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

