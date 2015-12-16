Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready!');
});

Pebble.addEventListener("appmessage",
  function(e) {
    var selectedIndex = parseInt(e.payload["2"]);
    console.log("Got a message: ", e.payload["2"]);

    if (!(localStorage.getItem("array")===null)) {
      console.log("Found existing list. Loading localStorage:");
      console.log(localStorage['array']);
      var currentList = JSON.parse(localStorage['array']);
      sendHttpRequest(
        currentList[selectedIndex]["endpoint"],
        currentList[selectedIndex]["json"]
      );
    }
  }
);

function sendHttpRequest(ToUrl,withJson) {

  var xhr = new XMLHttpRequest();

  if (withJson != "") {
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          console.log("Received response from POST:")
          console.log(JSON.stringify(xhr.responseText));
        }
    }
    xhr.open('POST', ToUrl, true);
    xhr.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
    xhr.send(JSON.stringify(withJson));
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
  var url = 'http://127.0.0.1:8080';

  console.log('Showing configuration page: ' + url);

  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  var configData = JSON.parse(decodeURIComponent(e.response));
  console.log("Storing localStorage stringified: " + JSON.stringify(configData['array']));
  localStorage.setItem("array", JSON.stringify(configData['array']));

  console.log('Configuration page returned: ' + JSON.stringify(configData));

  var listToString = "";
  var i;

  for (i=0; i < configData['array'].length; i++) {
    var currentName = configData['array'][i]["name"].trim().replace(",","");
    listToString = listToString + currentName + ","
  }
  listToString = listToString.substring(0,listToString.length-1);

  console.log("List has been stringified to " + listToString);

  var dict = {};
  if(configData['array']) {
    dict['KEY_LIST'] = listToString;
    dict['KEY_SIZE'] = i;
  } else {
    dict['KEY_LIST'] = "";
    dict['KEY_SIZE'] = 0;
  }


  console.log('Sending dict: ' + JSON.stringify(dict));

  Pebble.sendAppMessage(dict, function() {
      console.log('Send successful!');
    }, function() {
      console.log('Send failed!');
    });

});


