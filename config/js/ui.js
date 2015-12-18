
var $createButton = $('#createButton');
var $createFormContainer = $('#createNewFields');
var $saveButton = $('#saveButton');
var $addButton = $('#addButton');
var $checkJsonButton = $('#jsonPostJsonInput');

var currentList;
var newEntry = false;

(function() {
  document.getElementById('createNewFields').style.display = "none";
  document.getElementById('removeFields').style.display = "none";
  document.getElementById('JsonPostFields').style.display = "none";
  //$('#validationFeedbackLabel').hide();

  console.log(localStorage);

  initData();
  generateLists();

  $('#testResultsContainer').hide();
  $('#getFrame').hide();


})();

function generateLists(){
  $('.item-draggable-list').empty();
  $('.item-dynamic-list').empty();
  for (var i=0; i < currentList.length; i++) {

    // Create a row for the draggable list
    var newDragLabel = document.createElement('label');
    newDragLabel.className = "item";
    newDragLabel.innerHTML = currentList[i]["name"];
    newDragLabel.name = currentList[i]["name"];
    newDragLabel.id = i;

    // Create a row for the removable list
    var newRemoveLabel = document.createElement('label');
    newRemoveLabel.className = "item";
    newRemoveLabel.innerHTML = currentList[i]["name"];
    newRemoveLabel.name = currentList[i]["name"];
    newRemoveLabel.id = i;

    $('.item-draggable-list').append(newDragLabel);
    $('.item-dynamic-list').append(newRemoveLabel);
    
  }

  var addItemDraggable = document.createElement('div');
  var addItemDynamic = document.createElement('div');
  addItemDraggable.className = "item addNewButton";
  addItemDynamic.className = "item addNewButton";
  addItemDraggable.innerHTML = '<a href="#" onclick="showCreateDisplay();">Create a New Request</a>';
  addItemDynamic.innerHTML = '<a href="#" onclick="showCreateDisplay();">Create a New Request</a>';


  $('.item-draggable-list').append(addItemDraggable);
  $('.item-dynamic-list').append(addItemDynamic);

  // Reload slate to enable dynamic content 
  $.getScript("/bower_components/Slate/dist/js/slate.min.js", function(){
    $( ".item.add-item" ).css("display","none")
  });
}

function reconcileList() {
  var updatedList = [];
  if (newEntry) {
    updatedList = currentList;
    console.log(updatedList);
    newEntry = false;

  } else if (removeCompleted()) {
    $('.item-dynamic-list').children('label.item').each(function() {
      //alert('pushing ' + this.id);
      updatedList.push(currentList[this.id]);
    });
  } else if (reorderCompleted()) {
    console.log(updatedList);
    $('.item-draggable-list').children('label.item').each(function() {
      updatedList.push(currentList[this.id]);
    });
  } else {
    updatedList = currentList;
  }
  currentList = updatedList;
}


function initData() {
  if (!(localStorage.getItem("array")===null)) {
    console.log("Found existing list. Loading localStorage.");
    console.log(localStorage['array']);
    currentList = JSON.parse(localStorage['array']);

  } else {
    // This will be the default infomation with example data
    // Useful in helping newcomers learn what type of input is acceptable
    console.log("localStorage is null. Using default data.");
    currentList = [
      {
        "name" : "Example HTTP GET",
        "endpoint": "https://example.com:8080/endpoint", 
        "json": ""
      },
      {
        "name" : "Example JSON POST",
        "endpoint": "https://example.com:8080/jsonendpoint",
        "json": '{"key":"value","key":"value"}'
      },
      {
        "name" : "Example2 JSON POST",
        "endpoint": "https://example2.com:8080/jsonendpoint",
        "json": '{"key":"value","key":"value"}'
      }
    ];
  }
}
function removeCompleted() {
  return $( "a[name=tab-2].tab-button.active" ).html() == "Remove";
}
function reorderCompleted() {
  return $( "a[name=tab-2].tab-button.active" ).html() == "Reorder";
}
function createCompleted() {
  return $( "a[name=tab-2].tab-button.active" ).html() == "Create";
}

function jsonSelected() {
  return $( "a[name=tab-1].tab-button.active" ).html() == "POST JSON";
}

function testHttp() {
  var displayedName = $('#displayedName').val();
  var endpointURL = $('#httpGetUrlInput').val();
  var jsonString = $('#jsonPostJsonInput').val();

  if (displayedName == null || displayedName == "")
  {
      animateRed($('#displayedName').parent());

  } else if (endpointURL == null || endpointURL == "") {
      animateRed($('#httpGetUrlInput').parent());
  } else if ((jsonString == null || jsonString == "") && jsonSelected()) {
      animateRed($('#jsonPostJsonInput'));
  } else {
    $('#testButton').addClass('pendingResponse');
    $('#testButton').val('');
    //console.log("JSON String: " + jsonString);
    //console.log(JSON.parse(jsonString));
    
    if (jsonSelected()) {

  /*
        $.ajax({
        dataType: "jsonp",
        url: "http://api.openweathermap.org/data/2.5/forecast/city",
        jsonCallback: 'jsonp',
    data: { id: "524901", APPID: "da0bd1a46046c9f4d18a3fca969929b1" },
        cache: false,
        success: function (data) {
          alert(JSON.stringify(data));
        }
      });
  */
      $.ajax({
        method: "POST",
        url: endpointURL,
        data: JSON.parse(jsonString),
        dataType: "json",
        success: function(data){
          $('#testResults').html(JSON.stringify(data));
          $('#testResultsContainer').show();
          $('#testButton').removeClass('pendingResponse');
          $('#testButton').val('Test');
          $('html, body').animate({
              scrollTop: $("#testResultsContainer").offset().top
          }, 1000);
        },
        failure: function(errMsg) {
          $('#testResults').html(JSON.stringify(errMsg));
          $('#testResultsContainer').show();
          $('#testButton').removeClass('pendingResponse');
          $('#testButton').val('Test');
          $('html, body').animate({
              scrollTop: $("#testResultsContainer").offset().top
          }, 1000);
        },
        error: function(jqXHR, textStatus, errorThrown) {
          console.log(jqXHR);
          var respStatus = jqXHR.status;
          var respText = jqXHR.responseText;
          var respStatusText = jqXHR.statusText;
          var results = respStatus + " " + respStatus + ": " + respText;

          if (respStatus == 0) {
            results = results + " Encountered an error. Make sure Access-Control-Allow-Origin is configured.";
          }

          $('#testResults').html(results);
          $('#testResultsContainer').show();
          $('#testButton').removeClass('pendingResponse');
          $('#testButton').val('Test');
          $('html, body').animate({
              scrollTop: $("#testResultsContainer").offset().top
          }, 1000);
        }
      });
    }
    else {



      $.ajax({
        method: "GET",
        url: endpointURL,
        success: function(data){
          $('#testResults').html(JSON.stringify(data));
          $('#testResultsContainer').show();
          $('#testButton').removeClass('pendingResponse');
          $('#testButton').val('Test');
          $('html, body').animate({
              scrollTop: $("#testResultsContainer").offset().top
          }, 1000);
          //alert(JSON.stringify(data));
        },
        failure: function(errMsg) {
          $('#testResults').html(JSON.stringify(errMsg));
          $('#testResultsContainer').show();
          $('#testButton').removeClass('pendingResponse');
          $('#testButton').val('Test');
          $('html, body').animate({
              scrollTop: $("#testResultsContainer").offset().top
          }, 1000);
        },
        error: function(jqXHR, textStatus, errorThrown) {
          if (jqXHR.status == 0) {
              document.getElementById('getFrame').src = endpointURL;

              $('#testResults').html("Encountered an error. The Access-Control-Allow-Origin header may not be configured.");
              $('#testResultsContainer').show();
              $('#testButton').removeClass('pendingResponse');
              $('#testButton').val('Test');
              $('html, body').animate({
                  scrollTop: $("#testResultsContainer").offset().top
              }, 1000);
          }
        }
      });
    }
  }
}

function verifyJson() {
    var validationLabel = document.getElementById('validationFeedbackLabel');
    console.log("Input : " + $('#jsonPostJsonInput').val());
    if (IsJsonString($('#jsonPostJsonInput').val())) {
      console.log("valid");
      animateGreen($('#jsonPostJsonInput'));
    } else {
      console.log("invalid");
      animateRed($('#jsonPostJsonInput'));
    }
  }

function resetAfterCreation() {
    document.getElementById('createNewFields').style.display = "none";
}

function showReorderDisplay() {
    reconcileList();
    generateLists();
    clearFields();
    document.getElementById('createNewFields').style.display = "none";
    document.getElementById('reorderFields').style.display = "block";
    document.getElementById('removeFields').style.display = "none";
    $('#maintab').show();
    $('#removeTab').removeClass("active");
    $('#reorderTab').addClass("active");
    $('#pebbleSaveButton').show();
}

function showCreateDisplay() {
    // Show the div that contains user entry fields
    reconcileList();
    generateLists();
    clearFields();
    document.getElementById('createNewFields').style.display = "block";
    document.getElementById('reorderFields').style.display = "none";
    document.getElementById('removeFields').style.display = "none";

    $('#maintab').hide();
    $('#testResultsContainer').hide();
    $('#pebbleSaveButton').hide();
}

function showRemoveDisplay() {
  reconcileList();
  generateLists();
  // Remove Slate function which appends to the list
  // We use the create tab for this config page
  $( ".item.add-item" ).css("display","none")
  document.getElementById('removeFields').style.display = "block";
  document.getElementById('reorderFields').style.display = "none";
  document.getElementById('createNewFields').style.display = "none";
    $('#maintab').show();

  $('#testResultsContainer').hide()
    $('#reorderTab').removeClass("active");
    $('#removeTab').addClass("active");
    $('#pebbleSaveButton').show();
}

function clearFields() {
  $('#displayedName').val('');
  $('#httpGetUrlInput').val('');
  $('#jsonPostJsonInput').val('');
}

function createNewEntry() {

  var displayedName = $('#displayedName').val();
  var endpointURL = $('#httpGetUrlInput').val();
  var jsonString = $('#jsonPostJsonInput').val();

  if (displayedName == null || displayedName == "")
  {
      animateRed($('#displayedName').parent());

  } else if (endpointURL == null || endpointURL == "") {
      animateRed($('#httpGetUrlInput').parent());
  } else if ((jsonString == null || jsonString == "") && jsonSelected()) {
      animateRed($('#jsonPostJsonInput'));
  } else {
    if (jsonSelected()) {
      currentList.push({
        "name": displayedName,
        "endpoint": endpointURL,
        "json": jsonString
      });
    } else {
        currentList.push({
        "name": displayedName,
        "endpoint": endpointURL,
        "json": ""
      });
    }
    newEntry = true;
    showReorderDisplay();
    $('#removeTab').removeClass("active");
    $('#reorderTab').addClass("active");

  }
}

function sendToPebble() {

}

function sendClose(){

}

  function getConfigData() {
 
    var options = {
      'array': currentList
    };

    // Save for next launch
    localStorage['array'] = JSON.stringify(options['array']);

    console.log('Got options: ' + JSON.stringify(options));
    return options;
  }

  function getQueryParam(variable, defaultValue) {
    var query = location.search.substring(1);
    var vars = query.split('&');
    for (var i = 0; i < vars.length; i++) {
      var pair = vars[i].split('=');
      if (pair[0] === variable) {
        return decodeURIComponent(pair[1]);
      }
    }
    return defaultValue || false;
  }

function sendClose(saveChanges) {
  console.log("Sending close");

  if (saveChanges) {
    reconcileList();

    // Set the return URL depending on the runtime environment
    var return_to = getQueryParam('return_to', 'pebblejs://close#');
    document.location = return_to + encodeURIComponent(JSON.stringify(getConfigData()));
  } else {
    var return_to = getQueryParam('return_to', 'pebblejs://close#');
    document.location = return_to;
  }
}



function showHttpGetForm() {
    document.getElementById('JsonPostFields').style.display = "none";

}

function showJsonPostForm() {
    document.getElementById('JsonPostFields').style.display = "block";
}

function hasClass(element, cls) {
    return (' ' + element.className + ' ').indexOf(' ' + cls + ' ') > -1;
}