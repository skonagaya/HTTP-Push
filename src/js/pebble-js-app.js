

function sendHttpRequest(ToUrl,withJson,index) {

  var xhr = new XMLHttpRequest();

  console.log("I'M here now");

  if (withJson != "") {
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from POST:")
          console.log(JSON.stringify(xhr.responseText));
          sendHttpResponseToPebble(xhr.status.toString(),index);
        }
    }
    var strToJson = JSON.parse(withJson);
    xhr.open('POST', ToUrl, true);

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
          sendHttpResponseToPebble(xhr.status.toString(),index);
        }
    }
    xhr.open('GET', ToUrl, true);
    xhr.send(null);
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
  //var url = 'http://127.0.0.1:8080';
  var url = 'http://skonagaya.github.io/';

  console.log('Showing configuration page: ' + url);

  Pebble.openURL(url);
});

function sendListToPebble(listArray,action) {
  console.log("Preparing to send list to initialize Pebble data");
  var listToString = "";
  var i;

  for (i=0; i < listArray.length; i++) {
    var currentName = listArray[i]["name"].trim().replace(",","");
    listToString = listToString + currentName + ","
  }
  listToString = listToString.substring(0,listToString.length-1);

  console.log("List has been stringified to " + listToString);
  var dict = {};
  if(listArray.length > 0) {
    dict['KEY_LIST'] = listToString;
    dict['KEY_SIZE'] = i;
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

function sendHttpResponseToPebble(responseStr,index) {
  var dict = {};
  dict['KEY_LIST'] = "";
  dict['KEY_SIZE'] = 0;
  dict['KEY_RESPONSE'] = responseStr
  dict['KEY_ACTION'] = "response";
  dict['KEY_INDEX'] = index;
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
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration page returned: ' + JSON.stringify(configData));
    console.log("Storing localStorage stringified: " + JSON.stringify(configData['array']));
    localStorage.setItem("array", JSON.stringify(configData['array']));

    sendListToPebble(configData['array'],"update");
  }
});

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
  var localList = JSON.parse(localStorage.getItem('array'));
  console.log(JSON.stringify(localList));
  if (!(localList === null)) {
    console.log('Sending data to Pebble');
    console.log(localList['array']);
    sendListToPebble(localList,"update");
  } else {
    console.log('localStorage not found. This must be a fresh install!')
    console.log('Letting the pebble know we\'re shooting blanks.');
    sendListToPebble("","update");
  }
  
});

Pebble.addEventListener("appmessage",
  function(e) {
    var selectedIndex = parseInt(e.payload["KEY_INDEX"]);
    console.log("Got a message: ", e.payload["KEY_INDEX"]);
    console.log(JSON.stringify(e));

    if (!(localStorage.getItem("array")===null)) {
      console.log("Found existing list. Loading localStorage:");
      console.log(localStorage['array']);
      var currentList = JSON.parse(localStorage['array']);
      sendHttpRequest(
        currentList[selectedIndex]["endpoint"],
        currentList[selectedIndex]["json"],
        selectedIndex
      );
    }
  }
);

