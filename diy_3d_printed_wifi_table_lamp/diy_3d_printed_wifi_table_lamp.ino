#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <StreamString.h>
#include <math.h>
#define TRIGGER_PIN 0
#ifndef APSSID
  #define APSSID "STS RGB Table Lamp"
  #define APPSK "letitbelight"
#endif

const char* ssid = APSSID;
const char* password = APPSK;
int brightness = 0;
float intensity = 1;
int red = 0;
int green = 0;
int blue = 0;
int new_red = 0;
int new_green = 0;
int new_blue = 0;
bool cycle = true;
bool up = true;
bool config_enabled = false;
bool control_enabled = false;
unsigned long previous_time = 0;
unsigned long led_previous_time = 0;
unsigned long pwm_delay = 2;
unsigned long led_pwm_delay = 50;
unsigned long wifi_connection_start = 0;

#define redLedPin 14
#define greenLedPin 12
#define blueLedPin 13

ESP8266WebServer server(80);

const char homepage[]PROGMEM=R"=====(
<!DOCTYPE html><html><head> <title>STS RGB Table Lamp</title> <meta name="viewport" content="width=device-width, initial-scale=1"> <style type="text/css"> body{margin: 0; padding: 0; width: 100%%; height: 100%%; background:linear-gradient(to bottom,#7f8c8d,#95a5a6); background-repeat: no-repeat; background-attachment: fixed;}.main{display: block; position:absolute; height:auto; bottom:0; top:0; left:0; right:0; margin: 20px; background-color: green; padding: 20px; background:#2c3e50; border-radius:10px; box-shadow:5px 20px 50px #000; text-align: center;}.title{font-size:5vw; color: #fff;}.form_wrap{padding: 0; margin: 0; width: 100%%; display: block;}small{font-size:3vw; color: #fff;}.input_wrap{padding: 10px; position: relative;}input{font-size: 5vw; text-align: center; width: 100%%; border-radius: 10px; background-color: #34495e; border: none; box-shadow:2px 5px 5px #233140; color: #95a5a6; padding: 5px;}button{background: #27ae60; font-size: 1em; border: none; border-radius: 5px; transition: 0.2s ease-in; width: 100%%; height: 100%%; min-height: 50px; font-size: 3vw; box-shadow:2px 5px 5px #233140;}button:hover{background:#2ecc71;}.input_wrap i{position: absolute; right: 10px; font-size: 5vw; cursor: pointer; color: #95a5a6; z-index: 9999; display: flex; align-items: center; height: 100%%; top: 0;}</style> <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.13.0/css/all.min.css"></head><body> <div class="main"> <div class="form_wrap"> <label class="title" aria-hidden="true">STS RGB Table Lamp</label> <hr> <form id="form" action="/config" method="GET"> <p><small>Enter your router SSID and Password.</small></p><div class="input_wrap"> <input type="text" name="ssid" placeholder="SSID" required=""> </div><div class="input_wrap"> <input id="id_password" type="password" name="password" placeholder="Password" required=""> <i class="far fa-eye" id="togglePassword"></i> </div><div class="input_wrap"> <button type="submit" form="form" value="Submit">Connect</button> </div></form> </div></div></body><script type="text/javascript">(function(){const togglePassword=document.querySelector('#togglePassword'); const password=document.querySelector('#id_password'); togglePassword.addEventListener('click', function (e){const type=password.getAttribute('type')==='password' ? 'text' : 'password'; password.setAttribute('type', type); this.classList.toggle('fa-eye-slash');});})();</script></html>
)=====";

const char connection_page[]PROGMEM=R"=====(
<!DOCTYPE html><html><head> <title>STS RGB Table Lamp</title> <meta name="viewport" content="width=device-width, initial-scale=1"> <style type="text/css"> body{margin: 0; padding: 0; width: 100%%; height: 100%%; background:linear-gradient(to bottom,#7f8c8d,#95a5a6); background-repeat: no-repeat; background-attachment: fixed;}.main{display: block; position:absolute; height:auto; bottom:0; top:0; left:0; right:0; margin: 20px; background-color: green; padding: 20px; background:#2c3e50; border-radius:10px; box-shadow:5px 20px 50px #000; text-align: center;}.title{font-size:5vw; color: #fff;}.form_wrap{padding: 0; margin: 0; width: 100%%; display: block;}small{font-size:3vw; color: #fff;}</style></head><body> <div class="main"> <label class="title" aria-hidden="true">STS RGB Table Lamp</label> <hr> <div class="form_wrap"> <small> <p>Device is trying to connect...</p><p>Wait for 10 seconds, then check the blue status LED.</p><p>If the LED is constantly on, open your router and find the ip of the device named STS_RGB_Table_Lamp, then add `/control` after the ip and open that URL in your browser</p><p>If the LED is blinking, connection failed, try setting the SSID and password of your WiFi again.</p></small> </div></div></body></html>
)=====";

void setup() {    
  #ifdef WIFI_IS_OFF_AT_BOOT
    enableWiFiAtBootTime();
  #endif  
  Serial.begin(115200);
  while(!Serial);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
}

void loop() {  
  check_connection();
  check_restart();  
  change_led_status();  
}

void check_connection(){
  if(WiFi.status() != WL_CONNECTED) {    
    fade_led_not_connected();
    handle_wifi_config();
  } else {
    handle_control();
    digitalWrite(LED_BUILTIN, LOW);
  }  
}

void handle_control(){
  if(control_enabled){
    server.handleClient();
  } else {
    server.on("/control", provideControl);
    control_enabled = true;
  }
}

void provideControl(){
  if(server.arg("auto_cycle").toInt() == 1){
    red = 0;
    green = 0;
    blue = 0;
    cycle = true;
  } else {
    if(server.arg("redrange") == "" && server.arg("greenrange") == "" && server.arg("bluerange") == "" && server.arg("whiterange") == "" && server.arg("intensity") == ""){
      cycle = true;
    } else {
      cycle = false;
    }
    if(server.arg("intensity") != ""){
      intensity = server.arg("intensity").toFloat(); 
    }
    red = round(server.arg("redrange").toInt() * intensity);
    green = round(server.arg("greenrange").toInt() * intensity);
    blue = round(server.arg("bluerange").toInt() * intensity);
  }  
  StreamString controlpage;
  controlpage.reserve(10000);
  controlpage.printf("\
<!DOCTYPE html>\
<html>\
<head>\
  <title>STS RGB Table Lamp</title>\
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
  <style type=\"text/css\">\
    body {\
      margin: 0;\
      padding: 0;\
      width: 100%%;\
      height: 100%%;\
      background:linear-gradient(to bottom,#7f8c8d,#95a5a6);\
      background-repeat: no-repeat;\
      background-attachment: fixed;\
    }\
    .main {\
      display: block;\
      position:absolute;\
      height:auto;\
      bottom:0;\
      top:0;\
      left:0;\
      right:0;\
      margin: 20px;\
      background-color: green;\
      padding: 20px;\
      background:#2c3e50;\
      border-radius:10px;\
      box-shadow:5px 20px 50px #000;\
      text-align: center;\
    }\
    .title {\
      font-size:5vw;\
      color: #fff;\
    }\
    .form_wrap {\
      padding: 0;\
      margin: 0;\
      width: 100%%;\
      display: block;\
    }\
    .colorcyclewrapper {\
      min-width: 100%%;\
      min-height: 50px;\
      margin-bottom: 10px;\
    }\
    .colorcyclewrapper button {\
      background: #27ae60;\
      font-size: 1em;\
      border: none;\
      border-radius: 5px;\
      transition: 0.2s ease-in;\
      width: 100%%;\
      height: 100%%;\
      min-height: 50px;\
    }\
    .colorcyclewrapper button:hover {\
      background:#2ecc71;\
    }\
    .slidecontainer {\
      width: 100%%;\
    }\
    .slider {\
      -webkit-appearance: none;\
      appearance: none;\
      width: 80%%;\
      height: 25px;\
      background: #4e6d8c;\
      outline: none;\
      -webkit-transition: .2s;\
      transition: opacity .2s;\
      border-radius: 10px;\
    }\
    .slider:hover {\
      opacity: 1;\
    }\
    .slider::-webkit-slider-thumb {\
      -webkit-appearance: none;\
      appearance: none;\
      width: 25px;\
      height: 25px;\
      background: #04AA6D;\
      cursor: pointer;\
    }\
    .slider::-moz-range-thumb {\
      width: 25px;\
      height: 25px;\
      background: #04AA6D;\
      cursor: pointer;\
    }\
    .night{\
      width: 25px;\
      display: inline-block;\
      height: 25px;\
      background-color: #000;\
      margin: 0;\
      padding: 0;\
      border-radius: 25px;\
    }\
    .day{\
      width: 25px;\
      display: inline-block;\
      height: 25px;\
      background-color: #fff;\
      margin: 0;\
      padding: 0;\
      border-radius: 25px;\
    }\
    .sel_col {\
      -webkit-box-shadow:inset 0px 0px 0px 10px #2c3e50;\
      -moz-box-shadow:inset 0px 0px 0px 10px #2c3e50;\
      box-shadow:inset 0px 0px 0px 10px #2c3e50;\
    }\
  </style>\
  <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.13.0/css/all.min.css\">\
</head>\
<body>\
  <div class=\"main\">\
    <label class=\"title\" aria-hidden=\"true\">STS RGB Table Lamp</label>\
    <hr>\
    <div class=\"colorcyclewrapper\">\
      <a href=\"/control?auto_cycle=1\"><button>Auto Cycle</button></a>\
    </div>\
    <div class=\"slidecontainer\">\
      <span class=\"night\"></span>\
      <input type=\"range\" min=\"0\" max=\"1\" value=\"%f\" step=\"0.1\" class=\"slider\" id=\"myRange\" onchange=\"setBrightness()\">\
      <span class=\"day\"></span>\
    </div>\
    <div class=\"form_wrap\">\
    </div>\
  </div>\
</body>\
<script type=\"text/javascript\">\
  (function() {\
    let main = document.querySelector('.main');\
    let form_wrap = document.querySelector('.form_wrap');\
    let wrap_width = form_wrap.getBoundingClientRect().width;\
    let main_height = main.getBoundingClientRect().height;\
    let box_width = Math.round(wrap_width/9);\
    let title = document.querySelector('.title');\
    let title_height = title.offsetHeight;\
    let hr = document.querySelector('hr');\
    let hr_height = hr.getBoundingClientRect().height;\
    let cycle = document.querySelector('.colorcyclewrapper');\
    let cycle_height = cycle.getBoundingClientRect().height;\
    let wrapper_height = Math.floor(main_height - title_height - hr_height - cycle_height - 10);\
    let box_height = Math.floor(wrapper_height/11);\
    var i = 0;\
    var r = 255;\
    var g = 0;\
    var b = 0;\
    var colors = [];\
    for (g = 0; g < 255; g += 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (r = 255; r > 0; r -= 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (b = 0; b < 255; b += 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (g = 255; g > 0; g -= 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (r = 0; r < 255; r += 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (b = 255; b > 0; b -= 17) {\
      colors.push([r, g, b]);\
      i++;\
    }\
    for (var shade = 0; shade < 255; shade+= 31) {\
      if(255 - shade < 31){\
        shade = 255;\
      }\
      colors.push([shade, shade, shade]);\
      i++;\
    }\
    colors.forEach(make_color_box);\
    function make_color_box(item, index, arr) {\
      var url_red = new URLSearchParams(window.location.search).get(\"redrange\");\
      var url_green = new URLSearchParams(window.location.search).get(\"greenrange\");\
      var url_blue = new URLSearchParams(window.location.search).get(\"bluerange\");\
      if(url_red == item[0] && url_green == item[1] && url_blue == item[2]){\
        form_wrap.innerHTML += '<a class=\"sel_col\" href=\"/control?redrange=' + item[0] + '&greenrange=' + item[1] + '&bluerange=' + item[2] + '&intensity=%f\" style=\"display:inline-block;height:' + (box_height - 10) + 'px;width:11%%;background-color: rgb(' + item[0] + ',' + item[1] + ',' + item[2] + ');box-sizing: border-box;-moz-box-sizing: border-box;-webkit-box-sizing: border-box;border:1px solid #2c3e50\"><div class=\"colors\" style=\"display:inline-block;width:11%%;\"></div></a>';\
      } else {\
        form_wrap.innerHTML += '<a href=\"/control?redrange=' + item[0] + '&greenrange=' + item[1] + '&bluerange=' + item[2] + '&intensity=%f\" style=\"display:inline-block;height:' + (box_height - 10) + 'px;width:11%%;background-color: rgb(' + item[0] + ',' + item[1] + ',' + item[2] + ');box-sizing: border-box;-moz-box-sizing: border-box;-webkit-box-sizing: border-box;border:1px solid #2c3e50\"><div class=\"colors\" style=\"display:inline-block;width:11%%;\"></div></a>';\
      }\
    }\
  })();\
  function setBrightness(){\
    let slider = document.querySelector('.slider');\
    window.open(\"/control?intensity=\" + slider.value + \"&redrange=%d&greenrange=%d&bluerange=%d\",\"_self\");\
  }\
</script>\
</html>", intensity, intensity, intensity, server.arg("redrange").toInt(), server.arg("greenrange").toInt(), server.arg("bluerange").toInt());
  server.send(200, "text/html", controlpage.c_str());
  delay(300);    
//  \
//<!DOCTYPE html>\
//<html>\
//<head>\
//  <title>STS RGB Table Lamp</title>\
//  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
//  <style type=\"text/css\">\
//body{margin: 0; padding: 0; width: 100%%; height: 100%%; background:linear-gradient(to bottom,#7f8c8d,#95a5a6); background-repeat: no-repeat; background-attachment: fixed;}.main{display: block; position:absolute; height:auto; bottom:0; top:0; left:0; right:0; margin: 20px; background-color: green; padding: 20px; background:#2c3e50; border-radius:10px; box-shadow:5px 20px 50px #000; text-align: center;}.title{font-size:5vw; color: #fff;}.form_wrap{padding: 0; margin: 0; width: 100%%; display: block;}.colorcyclewrapper{min-width: 100%%; min-height: 50px; margin-bottom: 10px;}.colorcyclewrapper button{background: #27ae60; font-size: 1em; border: none; border-radius: 5px; transition: 0.2s ease-in; width: 100%%; height: 100%%; min-height: 50px;}.colorcyclewrapper button:hover{background:#2ecc71;}\
//  </style>\
//</head>\
//<body>\
//  <div class=\"main\">\
//    <label class=\"title\" aria-hidden=\"true\">STS RGB Table Lamp</label>\
//    <hr>\
//    <div class=\"colorcyclewrapper\">\
//      <a href=\"/control?auto_cycle=1\"><button>Auto Cycle</button></a>\
//    </div>\
//    <div class=\"form_wrap\">\
//    </div>\
//  </div>\
//</body>\
//<script type=\"text/javascript\">\
//  (function() {\
//    let main = document.querySelector('.main');\
//    let form_wrap = document.querySelector('.form_wrap');\
//    let wrap_width = form_wrap.getBoundingClientRect().width;\
//    let main_height = main.getBoundingClientRect().height;\
//    let box_width = Math.round(wrap_width/9);\
//    let title = document.querySelector('.title');\
//    let title_height = title.offsetHeight;\
//    let hr = document.querySelector('hr');\
//    let hr_height = hr.getBoundingClientRect().height;\
//    let cycle = document.querySelector('.colorcyclewrapper');\
//    let cycle_height = cycle.getBoundingClientRect().height;\
//    let wrapper_height = Math.floor(main_height - title_height - hr_height - cycle_height - 10);\
//    let box_height = Math.floor(wrapper_height/10);\
//    var i = 0;\
//    var r = 255;\
//    var g = 0;\
//    var b = 0;\
//    var colors = [];\
//    for (g = 0; g < 255; g += 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    for (r = 255; r > 0; r -= 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    for (b = 0; b < 255; b += 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    for (g = 255; g > 0; g -= 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    for (r = 0; r < 255; r += 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    for (b = 255; b > 0; b -= 17) {\
//      colors.push([r, g, b]);\
//      i++;\
//    }\
//    colors.forEach(make_color_box);\
//    function make_color_box(item, index, arr) {\
//      form_wrap.innerHTML += '<a href=\"/control?redrange=' + item[0] + '&greenrange=' + item[1] + '&bluerange=' + item[2] + '\" style=\"display:inline-block;height:' + (box_height - 10) + 'px;width:11%%;background-color: rgb(' + item[0] + ',' + item[1] + ',' + item[2] + ');box-sizing: border-box;-moz-box-sizing: border-box;-webkit-box-sizing: border-box;border:1px solid #2c3e50\"><div class=\"colors\" style=\"display:inline-block;width:11%%;\"></div></a>';\
//    }\
//  })();\
//</script>\
//</html>
}

void handle_wifi_config(){
  if(config_enabled){
    server.handleClient();
  } else {
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/", displayConfig);
    server.on("/config", saveConfig);
    server.begin();    
    config_enabled = true;
  }    
}

void displayConfig() {
  server.send(200, "text/html", homepage);
  delay(300);
}

void saveConfig(){
  String message = "";
  if (server.arg("ssid") == "" || server.arg("password") == ""){
    message = "ssid or password not found";
  }else{
    message = "ssid argument = ";
    message += server.arg("ssid");  
    message += " | password argument = ";
    message += server.arg("password");
  }
  Serial.println(message);
  server.send(200, "text/html", connection_page);
  delay(500);
  make_connection(server.arg("ssid"), server.arg("password"));
}

void make_connection(String ssid, String password){
  WiFi.disconnect();  
  #ifdef WIFI_IS_OFF_AT_BOOT
    enableWiFiAtBootTime();
  #endif      
  WiFi.mode(WIFI_STA);
  WiFi.hostname("STS_RGB_Table_Lamp");  
  WiFi.begin(ssid, password);
  wifi_connection_start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(millis() - wifi_connection_start > 10000){
      flush_config();
      break;
    }
  }  
  Serial.println("connection successful");
}

void check_restart(){
  if( digitalRead(TRIGGER_PIN) == LOW ){
    delay(100);        
    flush_config();
  }
}

void flush_config(){
    WiFi.disconnect();
    WiFi.persistent(false);
    Serial.println("wifi config flushed");
    ESP.restart();  
}

void fade_led_not_connected(){
  if(millis() >= previous_time + pwm_delay){
    previous_time = millis();
    if(!up){
      if(brightness == 0){
        brightness ++;      
        up = true;
      } else {
        brightness --;
      }
    } else {
      if(brightness == 255){
        brightness --;
        up = false;
      } else {
        brightness ++;
      }
    }
  }  
  analogWrite(LED_BUILTIN, brightness);  
}

void change_led_status(){
  if(cycle){
    cycle_colors();
  } else {
    analogWrite(redLedPin, red);
    analogWrite(greenLedPin, green);
    analogWrite(blueLedPin, blue);
  }
}

void cycle_colors(){
  if(millis() >= led_previous_time + led_pwm_delay){
    led_previous_time = millis();
    if(new_red == red && new_green == green && new_blue == blue){
     new_red = random(0, 255);
     new_green = random(0, 255);
     new_blue = random(0, 255);
    }
    if(new_red != red){
      if(new_red > red){
        red++;
      } else {
        red--;
      }
    }
    if(new_green != green){
      if(new_green > green){
        green++;
      } else {
        green--;
      }
    }
    if(new_blue != blue){
      if(new_blue > blue){
        blue++;
      } else {
        blue--;
      }
    }
    analogWrite(redLedPin, red);
    analogWrite(greenLedPin, green);
    analogWrite(blueLedPin, blue);   
  }    
}
