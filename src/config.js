function updateMenu(conf){
  var configData = JSON.parse(conf);
  console.log('Configuration page returned: ' + JSON.stringify(configData));

  var dict = {};
  dict['isWeather'] = 0;
  dict['KEY_TIME_COLOR'] = parseInt(configData['color']);
  dict['KEY_TEMP_SCALE'] = parseInt(configData['temp_scale']);  
  dict['KEY_BT_LOGO_TYPE'] = parseInt(configData['bt_logo']);
  dict['KEY_LARGE_DIGITS'] = parseInt(configData['large_digits']);
  dict['KEY_HIDE_SECONDS'] = parseInt(configData['hide_seconds']);
  dict['KEY_WEATHER_MODE'] = parseInt(configData['temp_update']);
  dict['KEY_DATE_FORMAT'] = parseInt(configData['date_format']);

  // Send to watchapp
  Pebble.sendAppMessage(dict, function() {
    console.log('Send successful: ' + JSON.stringify(dict));
  }, function() {
    console.log('Send failed!');
  });
}

Pebble.addEventListener('ready', function(e) {
    console.log('JavaScript ready.');
});

Pebble.addEventListener('showConfiguration', function(e) {
  // Show config page
  var loc = 'https://cdn.rawgit.com/phytomine/PixelGridDigital/f4b2cf6fdcf9b896631085938f1793654f603eb0/config/index.html';
  console.log('Showing configuration page');
  Pebble.openURL(loc);  
});


Pebble.addEventListener("webviewclosed", function(e) {
  if(e.response && e.response.length) {
    var config = decodeURIComponent(e.response);
    updateMenu(config);
  }
});