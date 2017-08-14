var pebbleTables = {}; // table that is indexed according to pebble folder structure
var pebbleJsCodeVersion = "4.0.0";
var STATUSBAR_COLOR_NOTIFY_RESPONSE_VERSION = "4.0.0";
var pebblePlatform = null;

// v4.0.0 include JSON only in the following methods
// POST, PUT, DELETE, OPTIONS
function includesJson(method) {
    if (method == "POST" ||
        method == "PUT" ||
        method == "DELETE" ||
        method == "OPTIONS") {
        return true;
    } else {
        return false;
    }
}

// contains all the http request calls
function sendHttpRequest(requestname, ToUrl, withJson, folderIndex, rowIndex, method, contenttype, headers, notify, response) {

    var xhr = new XMLHttpRequest();
    xhr.timeout = 10000;

    console.log("contenttype: " + contenttype);
    console.log("headers: " + JSON.stringify(headers));

    // if content-type is specified in the headers, 
    // then overwrite contenttype variable
    var overrideContentType = false;

    // append all the headers in the request
    for (var i = 0; i < headers.length; i++) {
        for (var key in headers[i]) {
            key = key.trim();
            val = headers[i][key].trim();
            if (key && val) {
                console.log("Setting header: " + key + ": " + val);
                xhr.setRequestHeader(key, val);
            }
            if (key.toLowerCase() == "content-type") {
                overrideContentType = true;
            }
        }
    }


    if (includesJson(method)) {

        xhr.onreadystatechange = function() {
            if (xhr.readyState === 4) {
                console.log("Received response from " + method + ":")
                console.log(xhr.responseText);

                // per v4.0.0
                var responseText = response == "Status Code" ? "Status Code: " + xhr.status.toString() : JSON.stringify(xhr.responseText);
                sendHttpResponseToPebble(requestname, responseText, folderIndex, rowIndex, notify);

            }
        };

        xhr.open(method, ToUrl, true);

        if (contenttype == "application/x-www-form-urlencoded") {
            // converts json body to a parameterized string
            // example: {'a':'b','c':'d'} => "a=b&c=d"
            // this is prone to runtime error since this behavior is particular to this app
            var strToJson = JSON.parse(withJson);
            withJson = Object.keys(strToJson).map(function(k) {
                return encodeURIComponent(k) + '=' + encodeURIComponent(strToJson[k])
            }).join('&');
            if (!overrideContentType) {
                xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            }
            console.log("XMLHttpRequest sending parameterized json: " + withJson);
        } else {
            console.log("XMLHttpRequest sending json: " + withJson);
        }

        try {
            if (!overrideContentType) {
                xhr.setRequestHeader('Content-Type', contenttype);
            }
            xhr.send(withJson);
        } catch (err) {
            console.log("Error sending XMLHttpRequest: " + JSON.stringify(err));
            sendHttpResponseToPebble(requestname, "Server Error", folderIndex, rowIndex, notify);
        }
    } else { // METHOD JSON GET
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                console.log("Received response from " + method + ": ");
                console.log(xhr.responseText);

                // per v4.0.0
                var responseText = response == "Status Code" ? "Status: " + xhr.status.toString() : JSON.stringify(xhr.responseText);
                sendHttpResponseToPebble(requestname, responseText, folderIndex, rowIndex, notify);
            }
        }
        xhr.open(method, ToUrl, true);
        console.log("Using method: " + method);
        console.log("Using ToUrl: " + ToUrl);
        try {
            if (!overrideContentType) {
                xhr.setRequestHeader('Content-Type', contenttype);
            }
            xhr.send(null);
        } catch (err) {
            console.log("Error sending XMLHttpRequest: " + JSON.stringify(err));
            sendHttpResponseToPebble(requestname, "Server Error", folderIndex, rowIndex, notify);
        }
    }
}

// configuration page on mobile device is initiated by the URL specified in this method
Pebble.addEventListener('showConfiguration', function() {

    // make sure we have the pebble version
    if (getLocalVersion() == '') {
        getPebbleAppVersion();
    }

    // https upgrade with v4.0. use iFrames trick to migrate user localStorage
    // this is the url that hosts user's configuration.
    var url = 'http://skonagaya.github.io';
    //var url = 'http://4521b499.ngrok.io';

    console.log("localStorage: " + localStorage.getItem("array"));

    // upgrade mechanism used to avoid losing users data in localStorage
    //if (getLocalVersion() != '') url = url + "/upgrade/";

    console.log('Showing configuration page: ' + url);

    // append the platform and send it to the configuration page
    // used to disable color settings for non-color applications
    var watch = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
    if (watch) {
        // Information is available!
        console.log("Watch information: " + JSON.stringify(watch));
        pebblePlatform = watch.platform.toLowerCase();
        if (pebblePlatform == "aplite" || pebblePlatform == "diorite") {
            url = url + "?color=false";
        }
    }

    Pebble.openURL(url);
});

// recusively traverse folders 
function traverseList(nextList) {
    var currentLevelList = [];
    for (var i = 0; i < nextList.length; i++) {
        if (nextList[i]["type"] == "request") {
            currentLevelList.push(nextList[i]["name"]);
        } else if (nextList[i]["type"] == "folder") {
            var folderList = traverseList(nextList[i]["list"]);
            var entry = {}
            entry[nextList[i]["name"]] = folderList
            currentLevelList.push(entry);
        }
    }
    return currentLevelList;
}

// local version getter. used to observe state of pebble C code version
// change pebblejs version to display message on watch such as "Go Update!"
function getLocalVersion() {
    var localList = localStorage.getItem("version");
    var version = '';
    //if (localList !== null && localStorage.getItem("settings") !== null) {
    if (localList !== null) {
        version = localList;
    }
    console.log("getLocalVersion returned: \"" + version + "\"");
    return version;
}

// recursively traverse configuration table to build out data stream which the C code can consume
// refer to github readme for details on the data stream format.
function traverseListString(nextList, folderIndex) {

    var currentLevelList = "";
    var parentIndex = folderIndex;

    for (var i = 0; i < nextList.length; i++) {
        if (nextList[i]["type"] == "request" || nextList[i]["type"] === undefined) {

            console.log("Found request type");
            console.log("parentIndex.toString()+i.toString(): " + parentIndex.toString() + i.toString());
            console.log("JSON.stringify(nextList[i]): " + (JSON.stringify(nextList[i])));
            currentLevelList = currentLevelList + "_E" + "_" + parentIndex + "_" + i.toString() + "_" + nextList[i]["name"].replace("_", " ");

            pebbleTables[parentIndex.toString() + i.toString()] = JSON.parse(JSON.stringify(nextList[i]));

        } else if (nextList[i]["type"] == "folder") {

            folderIndex = folderIndex + 1;

            var nextLevelData = traverseListString(nextList[i]["list"], folderIndex);
            var nextLevelString = nextLevelData[0];
            var nextIndexAfterFolderTraversal = nextLevelData[1];

            currentLevelList = currentLevelList + "_F" + "_" + nextList[i]["list"].length.toString() + "_" + folderIndex + "_" + parentIndex + "_" + i.toString() + "_" + nextList[i]["name"].replace("_", " ") + nextLevelString;
            folderIndex = nextIndexAfterFolderTraversal;
        }
    }
    return [currentLevelList, folderIndex];
}

// traverse for the folder item count
// used to allocate enough memory on watch
function traverseCount(nextList) {
    var currentLevelCount = 0;
    for (var i = 0; i < nextList.length; i++) {
        if (nextList[i]["type"] == "folder") {
            currentLevelCount = currentLevelCount + 1 + traverseCount(nextList[i]["list"]);
        }
    }
    return currentLevelCount;

}

function chunkString(str, length) {
    return str.match(new RegExp('.{1,' + length + '}', 'g'));
}

// send the request to pebble watch, this triggers requests and folders to be created on the watch
// used when changes are made to the configuration page
function sendListToPebble(listArray, action) {

    console.log("Preparing to send list to initialize Pebble data");
    console.log("Creating flat datastructure for pebble mapping");

    //TODO: replace _ with space and replace . with empty character
    console.log("listArray: " + JSON.stringify(listArray));
    var listToString = "";
    var listToArray = null;
    var listCount = traverseCount(listArray) + 1;

    console.log("listArray.size = " + listCount.toString());

    // reset table
    pebbleTables = {};

    // traverse the localStorage data to build out folder structure on the watch.
    // format details described in github readme
    var trimmedList = "_F_" + listArray.length.toString() + "_0_-1_-1_Root" + traverseListString(listArray, 0, 0)[0] + "_";

    var pebbleVersion = getLocalVersion();

    // catch case where user has not updated pebble app version
    // older version may not be able to handle new content, and must be ommitted
    if (pebbleVersion) {
        console.log("Local version or localstorage.settings was missing");

        if (localStorage.getItem("settings") !== null) {

            // if vibration is set, then send it to pebble watch
            if (localStorage.getItem("settings")["vibration"] !== null) {

                var vibrationStr = JSON.parse(localStorage.getItem("settings"))["vibration"].toString();
                console.log("Adding vibration String: " + vibrationStr);
                trimmedList = trimmedList + "V_" + vibrationStr + "_";
            }

            // 4.0.0 migration code
            if (pebbleVersion >= STATUSBAR_COLOR_NOTIFY_RESPONSE_VERSION) {
                // if color is set, then send it to pebble watch
                if (localStorage.getItem("settings")["backgroundColor"] !== null) {

                    var backgroundColorStr = JSON.parse(localStorage.getItem("settings"))["backgroundColor"].toString();
                    console.log("Adding backgroundColor String: " + backgroundColorStr);
                    trimmedList = trimmedList + "BC_" + backgroundColorStr + "_";
                }
                // if color is set, then send it to pebble watch
                if (localStorage.getItem("settings")["foregroundColor"] !== null) {

                    var foregroundColorStr = JSON.parse(localStorage.getItem("settings"))["foregroundColor"].toString();
                    console.log("Adding foregroundColor String: " + foregroundColorStr);
                    trimmedList = trimmedList + "FC_" + foregroundColorStr + "_";
                }
                // if color is set, then send it to pebble watch
                if (localStorage.getItem("settings")["selectedColor"] !== null) {

                    var selectedColorStr = JSON.parse(localStorage.getItem("settings"))["selectedColor"].toString();
                    console.log("Adding selectedColor String: " + selectedColorStr);
                    trimmedList = trimmedList + "SC_" + selectedColorStr + "_";
                }
                // if color is set, then send it to pebble watch
                if (localStorage.getItem("settings")["statusBarColor"] !== null) {

                    var statusBarColorStr = JSON.parse(localStorage.getItem("settings"))["statusBarColor"].toString();
                    console.log("Adding statusBarColorStr String: " + statusBarColorStr);
                    trimmedList = trimmedList + "TC_" + statusBarColorStr + "_";
                }
                // if showFolderIcon is set, then send it to pebble watch
                if (localStorage.getItem("settings")["showFolderIcon"] !== null) {

                    var showFolderIconStr = JSON.parse(localStorage.getItem("settings"))["showFolderIcon"] ? "1" : "0";
                    console.log("Adding showFolderIcon String: " + showFolderIconStr);
                    trimmedList = trimmedList + "FI_" + showFolderIconStr + "_";
                }
                // if showStatusBar is set, then send it to pebble watch
                if (localStorage.getItem("settings")["showStatusBar"] !== null) {

                    var showStatusBarStr = JSON.parse(localStorage.getItem("settings"))["showStatusBar"] ? "1" : "0";
                    console.log("Adding showStatusBar String: " + showStatusBarStr);
                    trimmedList = trimmedList + "SB_" + showStatusBarStr + "_";
                }
            }
        }
    }

    // the string has to be chunked due to the length of payload sent over bluetooth is limited
    // i forgot to document what the exact limit was. my bad. anyways we chunk using 50 char intervals
    // i do remember that this limit is different between basalt and chalk. 
    listToString = JSON.stringify(trimmedList).slice(1, -1);
    listToArray = chunkString(listToString, 50);

    // store this "Watch View" as pebble_tables. we now have a local copy of the datastructure of the pebble watch
    // pebble_tables indexes maps to the folder-index and folder-item-index to localStorage configurations
    console.log("pebbleTables: " + JSON.stringify(pebbleTables));
    localStorage.setItem("pebble_tables", JSON.stringify(pebbleTables));

    console.log("List has been stringified to " + listToString);
    console.log("List has been chunkified to " + JSON.stringify(listToArray));

    var dict = {};

    // If the list is empty, send it to the watch to propagate "empty list error msg"
    if (listArray.length == 0) {

        dict.KEY_LIST = "";
        dict.KEY_SIZE = 0;
        dict.KEY_ACTION = action;

        Pebble.sendAppMessage(dict, function() {
            console.log('Successfully sent empty list to pebble');
        }, function(e) {
            console.log('Failed to send empty list to pebble');
            console.log(JSON.stringify(e));
        });
    } else {
        // default case: no need to chunk, just update pebble now..
        if (listToArray.length == 1) {
            console.log('No need to chunk.');
            sendChunkToPebble(listToString, listCount, unescape(encodeURIComponent(listToString)).length, "update");
        } else {
            listChunks = listToArray;
            finalListSize = listCount;
            sendChunkToPebble(listToArray[0], listCount, unescape(encodeURIComponent(listToString)).length, "chunk");
        }

    }
}

// global variables used to chunk the data stream to pebble watch.
var chunkIndex = 0;
var listChunks = null;
var finalListSize = 0;
var chunkTotalByteSize = 0;

// send chunks of data to pebble
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
    });
}

// method includes all messages sent TO pebble watch device
function sendHttpResponseToPebble(requestName, responseStr, folderIndex, rowIndex, notify) {
    var dict = {};
    dict['KEY_LIST'] = ""; // the request content such as request name
    dict['KEY_REQUEST_NAME'] = requestName; // the request content such as request name
    dict['KEY_SIZE'] = 0; // the size used to allocate enough memory
    dict['KEY_NOTIFICATION'] = notify ? 1 : 0; // whether to display as notification
    dict['KEY_RESPONSE'] = responseStr.substring(0, 600); // response as a result of an http request
    dict['KEY_ACTION'] = "response"; // command that tells the watch what to do (chunk / update / display response)
    dict['KEY_INDEX'] = rowIndex; // index of the request which requires action such as display http response
    dict['KEY_FOLDER_INDEX'] = folderIndex; // index of the folder which requires action
    console.log('Sending dict: ' + JSON.stringify(dict));

    // this is pebble code to initiated the bluetooth communication
    Pebble.sendAppMessage(dict, function() {
        console.log('Successfully sent http response to pebble');
    }, function() {
        console.log('Failed to send http response to pebble');
    });
}

// this is the async listener for events coming from the configuration page on the mobile device.
// if the configuration page is closed using the save & close button, we update our localStorage
// and the requests displayed on the watch.
Pebble.addEventListener('webviewclosed', function(e) {
    if (e.response === "") {
        console.log("Configuration page returned nothing....");
    } else {
        var configData;
        try { configData = JSON.parse(decodeURIComponent(e.response)); } catch (err) {
            // ran into an issue where % characters were not encoding prooperly
            // this may need to be revisited
            var stringToParse = JSON.stringify(e.response).replace(/%/g, '%25');
            console.log('URL Decode failed');
            if (stringToParse.indexOf('%') > -1) {
                configData = JSON.parse(decodeURIComponent(JSON.parse(stringToParse)));
            } else {
                configData = JSON.parse('{"array":[]}');
            }
        }
        console.log('Configuration page returned: ' + JSON.stringify(configData));
        console.log("Storing localStorage array stringified: " + JSON.stringify(configData['array']));

        //save it to our localStorage
        localStorage.setItem("array", JSON.stringify(configData['array']));
        if (getLocalVersion() != '') {
            console.log("Storing localStorage settings stringified: " + JSON.stringify(configData['settings']));
            localStorage.setItem("settings", JSON.stringify(configData['settings']));
        }

        sendListToPebble(configData['array'], "update");
    }
});

// this is the async listener for events coming from the pebble watch
Pebble.addEventListener('ready', function() {

    console.log('PebbleKit JS ready!');
    console.log('Version: ' + pebbleJsCodeVersion);
    var startFresh = false;

    // check if the user is new (fresh start) and updates with an empty list
    // first check if data is started in localStorage
    var localList = localStorage.getItem("array");
    if (localList === null) { startFresh = true; } else {
        try { // CHECK FOR LOCAL STORAGE CORRUPTION. HAPPENS IN EMU NOT ON WATCH
            localList = JSON.parse(localList);
        } catch (e) {
            console.log('Local storage is corrupted!')
            startFresh = true;
        }
    }
    if (startFresh) {
        console.log('localStorage not found. This must be a fresh install!')
        console.log('Letting the pebble know we\'re shooting blanks.');
        sendListToPebble("", "update");
    } else {
        var localList = JSON.parse(localStorage.getItem('array'));

        console.log("Found existing data: ")
        console.log(JSON.stringify(localList));

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

// get the version of pebble C code
// the version state is used when we have to display messages to users through the watch after an upgrade.
// this assumes that users usually interact with the watch more that config page
function getPebbleAppVersion() {
    console.log("Retreiving app version from Pebble");
    var dict = {};
    dict.KEY_LIST = "";
    dict.KEY_SIZE = 0;
    dict.KEY_ACTION = "version";

    console.log("Sending dict to Pebble: " + JSON.stringify(dict));

    Pebble.sendAppMessage(dict, function() {
        console.log('Successfully sent empty list to pebble');
    }, function(e) {
        console.log('Failed to send empty list to pebble');
        console.log(JSON.stringify(e));
    });
}


// async listener for messages coming FROM the watch
Pebble.addEventListener("appmessage",
    function(e) {
        console.log("Got KEY_FOLDER_INDEX: " + e.payload["KEY_FOLDER_INDEX"]);
        console.log("Got KEY_INDEX: " + e.payload["KEY_INDEX"]);
        console.log("Got KEY_CHUNK_INDEX: " + e.payload["KEY_CHUNK_INDEX"]);
        console.log("Got KEY_ACTION: " + e.payload["KEY_ACTION"]);
        console.log("Got KEY_ERROR: " + e.payload["KEY_ERROR"]);
        console.log("Got KEY_VERSION: " + e.payload["KEY_VERSION"]);

        var action = e.payload["KEY_ACTION"]; // the info requested by the pebble watch
        var folderIndex = parseInt(e.payload["KEY_FOLDER_INDEX"]); // the folder index of the request highlighted on watch
        var rowIndex = parseInt(e.payload["KEY_INDEX"]); // the folder item index of the request highlighted on watch
        var selectedIndex = folderIndex.toString() + rowIndex.toString(); // translate folder+item index to match pebble_tables
        var pebbleChunkIndex = (parseInt(e.payload["KEY_CHUNK_INDEX"])); // the index of the chunk (used if chunking in process)
        var version = e.payload["KEY_VERSION"]; // the version of the pebble C code

        console.log(JSON.stringify(e));

        if (action == "chunk") {
            console.log("Received chunking message from Pebble");
            console.log("Using listChunks: " + JSON.stringify(listChunks));
            if (listChunks.length - 1 == pebbleChunkIndex) { // if last chunk
                sendChunkToPebble(listChunks[pebbleChunkIndex], finalListSize, 0, "update");
            } else {
                sendChunkToPebble(listChunks[pebbleChunkIndex], finalListSize, 0, "chunk");
            }
        } else if (action == "version") {
            console.log("Received version message from Pebble");
            console.log("Setting localStorage version to " + version);
            localStorage.setItem("version", version);
        } else if (!(localStorage.getItem("pebble_tables") === null)) {
            console.log("Found existing list. Loading localStorage:");
            console.log(localStorage['pebble_tables']);

            var currentList = JSON.parse(localStorage['pebble_tables']);

            // place in default values for any missing configurations.
            // missing configurations is caused by version upgrades.
            // for example, version 4.0.0 introduces content-type: if missing, set to default value of application/x-www-form-urlencoded 

            // method schema addition
            if (currentList[selectedIndex]['method'] === undefined) {
                console.log("Found undefined method");
                if (currentList[selectedIndex]['json'] == "") {
                    console.log("Setting method as GET");
                    currentList[selectedIndex]['method'] = "GET";
                } else {
                    currentList[selectedIndex]['method'] = "POST";
                }
            }
            // Content-Type schema addition (added with v4.0.0)
            if (currentList[selectedIndex]['contenttype'] === undefined) {

                console.log("Found undefined contentType. using application/x-www-form-urlencoded");
                currentList[selectedIndex]['contenttype'] = "application/json";
            }
            // headers schema addition (added with v4.0.0)
            if (currentList[selectedIndex]['headers'] === undefined) {
                console.log("Found undefined headers. Using []");
                currentList[selectedIndex]['headers'] = [];
            }
            // notification schema addition (added with v4.0.0)
            if (currentList[selectedIndex]['notify'] === undefined) {
                console.log("Found undefined notify. Using false");
                currentList[selectedIndex]['notify'] = false;
            }
            // response schema addition (added with v4.0.0)
            if (currentList[selectedIndex]['response'] === undefined) {
                console.log("Found undefined response. Using Status Code");
                currentList[selectedIndex]['response'] = "Status Code";
            }

            // send the HTTP request and respond to pebble with results
            sendHttpRequest(
                currentList[selectedIndex]["name"],
                currentList[selectedIndex]["endpoint"],
                currentList[selectedIndex]["json"],
                folderIndex,
                rowIndex,
                currentList[selectedIndex]["method"],
                currentList[selectedIndex]["contenttype"],
                currentList[selectedIndex]["headers"],
                currentList[selectedIndex]["notify"],
                currentList[selectedIndex]["response"]
            );
        }
    }
);