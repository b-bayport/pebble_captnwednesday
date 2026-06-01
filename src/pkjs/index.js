var keys = require('message_keys');

function fetchWeather() {
  var req = new XMLHttpRequest();
  req.onload = function() {
    try {
      var data = JSON.parse(this.responseText);
      var tempF = parseInt(data.current_condition[0].temp_F, 10);
      var tempC = parseInt(data.current_condition[0].temp_C, 10);
      var msg = {};
      msg[keys.TEMPERATURE_F] = tempF;
      msg[keys.TEMPERATURE_C] = tempC;
      Pebble.sendAppMessage(msg,
        function() { console.log('Temp sent: ' + tempF + 'F / ' + tempC + 'C'); },
        function(e) { console.log('Send error: ' + JSON.stringify(e)); }
      );
    } catch(e) {
      console.log('Weather parse error: ' + e);
    }
  };
  req.open('GET', 'https://wttr.in/?format=j1');
  req.send();
}

Pebble.addEventListener('ready', function() {
  fetchWeather();
  setInterval(fetchWeather, 1800000);
});
