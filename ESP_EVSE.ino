#define TRUE (0==0)
#define FALSE (0!=0)

#include <LittleFS.h>
#include <WiFiManager.h> // tzapu
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP_DoubleResetDetector.h>
#include <ESP8266TimerInterrupt.h>

// Configuration variables
int cfg_int = 0;
float cfg_float = 0.0;
String cfg_str = "";
float prv_float = 0.0;
const int LED_PIN = 2; // Built-in LED for ESP-01S (GPIO2)
const int CP_pin = 3;
const int PE_pin = 1;
const String ver = String(__DATE__) + " " + String(__TIME__);
const int debug = FALSE;

// Pulse timing in microseconds
float duty = 10.0;
uint32_t deadband = 3 * 5;
uint32_t correction = 18; // taken from both sides
uint32_t period = (1000 - correction) * 5; // uS 1000000 * (1/freq)ISR_PE_HI
float k = period / 100.0;
uint32_t mark = floor((duty * k) - correction);
uint32_t space = period - mark - correction;

ESP8266WebServer server(80);
DoubleResetDetector* drd;

void ICACHE_RAM_ATTR ISR_CP_LO();
void ICACHE_RAM_ATTR ISR_PE_HI();
void ICACHE_RAM_ATTR ISR_CP_HI();
void ICACHE_RAM_ATTR ISR_PE_LO();

void ICACHE_RAM_ATTR ISR_CP_LO()
{
  timer1_detachInterrupt();
  digitalWrite(CP_pin, LOW);
  timer1_write(deadband); timer1_attachInterrupt(ISR_PE_HI);
}

void ICACHE_RAM_ATTR ISR_PE_HI()
{
  timer1_detachInterrupt();
  digitalWrite(PE_pin, HIGH);
  timer1_write(space - deadband); timer1_attachInterrupt(ISR_CP_HI);
}

void ICACHE_RAM_ATTR ISR_CP_HI()
{
  timer1_detachInterrupt();
  digitalWrite(CP_pin, HIGH);
  timer1_write(deadband); timer1_attachInterrupt(ISR_PE_LO);
}

void ICACHE_RAM_ATTR ISR_PE_LO()
{
  timer1_detachInterrupt();
  digitalWrite(PE_pin, LOW);
  timer1_write(mark - deadband); timer1_attachInterrupt(ISR_CP_LO);
}

// Save variables to LittleFS
void saveConfig() {
  StaticJsonDocument<256> doc;
  doc["cfg_int"] = cfg_int;
  doc["cfg_float"] = cfg_float;
  doc["cfg_str"] = cfg_str;

  File file = LittleFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Config saved to flash.");
  }
}

// Load variables from LittleFS
void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (!error) {
      cfg_int = doc["cfg_int"] | 0;
      cfg_float = doc["cfg_float"] | 0.0;
      cfg_str = doc["cfg_str"].as<String>();
    }
    file.close();
  }
}

// Throb LED logic for visual feedback
void throbLED(int count) {
  for (int i = 0; i < count; i++) {
    for (int v = 255; v >= 0; v--) { analogWrite(LED_PIN, v); delay(2); }
    for (int v = 0; v <= 255; v++) { analogWrite(LED_PIN, v); delay(2); }
  }
}

void blinkNumber(int num) {
  if (num == 0) {
    // Blink 10 times for '0'
    for(int i=0; i<10; i++){
      digitalWrite(LED_PIN, LOW); delay(100);
      digitalWrite(LED_PIN, HIGH); delay(100);
    }
  }
  for (int i = 0; i < num; i++) {
    digitalWrite(LED_PIN, LOW); delay(200);
    digitalWrite(LED_PIN, HIGH); delay(200);
  }
  delay(1000); // Pause between digits
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY); delay(600); Serial.printf("\n\n\n");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED Off (Active Low)

  drd = new DoubleResetDetector(20, 0x00);
  LittleFS.begin();
  loadConfig();
  duty = cfg_float /0.6 ;

  WiFiManager wm; // WiFi Captive Portal
  if (!wm.autoConnect("ESP-EVSE-Config")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }

  if (!MDNS.begin("ESP-EVSE")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    MDNS.addService("http", "tcp", 80);
  }

  // Handle Double Reset (Factory Reset)
  if (drd->detectDoubleReset()) {
    Serial.println("Double Reset Detected! blinking IP...");
    IPAddress ip = WiFi.localIP();
    blinkNumber(ip[3] / 100);
    blinkNumber((ip[3] / 10) % 10);
    blinkNumber(ip[3] % 10);
  }

  ArduinoOTA.begin();

  // --- WEB SERVER ROUTES ---MDNS.addService("http", "tcp", 80)

  server.on("/", []() {
    String html = "<h1>ESP-EVSE Home</h1><p>FW Date: " + ver + "</p>";
    html += "<p><a href='/app'>Configure Variables</a></p>";
    html += "<script>fetch('/settime?t='+Math.floor(Date.now()/1000));</script>";
    server.send(200, "text/html", html);
  });

  server.on("/app", []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;padding:20px;max-width:400px;margin:auto;} .row{margin:25px 0;} button{padding:10px 15px;} ";
    html += ".warn{background:#ff4444; color:white; border:none; width:100%; margin-top:40px; padding:15px;}</style></head><body>";
    html += "<h2>Settings</h2>";
    html += "<div class='row'><b>Int:</b> <button onclick='chg_i(-1)'>-</button> <span id='val_i'>" + String(cfg_int) + "</span> <button onclick='chg_i(1)'>+</button></div>";
    html += "<div class='row'><b>Float:</b> <button onclick='chg_f(-1.0)'>-</button> <span id='val_f'>" + String(cfg_float) + "</span> <button onclick='chg_f(1.0)'>+</button></div>";
//  html += "<div class='row'><b>Float:</b> <br><input type='range' min='0' max='100' step='0.1' style='width:100%' value='" + String(cfg_float) + "' oninput='document.getElementById(\"dis_f\").innerText=this.value' onchange='upd(\"f\",this.value)'><br>Value: <span id='dis_f'>" + String(cfg_float) + "</span></div>";
    html += "<div class='row'><b>String:</b> <br><input type='text' style='width:100%;padding:8px;' value='" + cfg_str + "' onchange='upd(\"s\",this.value)'></div>";
    html += "<button onclick='location.href=\"/save_flash\"' style='width:100%'>Commit to Flash</button>";
    html += "<button class='warn' onclick='if(confirm(\"Clear all settings and Sleep?\"))location.href=\"/factory_reset\"'>Factory Reset & Sleep</button>";
    html += "<script>";
    html += "function upd(p,v){fetch('/update?'+p+'='+encodeURIComponent(v));}";
    html += "function chg_i(d){let e=document.getElementById(\"val_i\"); let nv=parseInt(e.innerText)+d; e.innerText=nv; upd(\"i\",nv);}";
    html += "function chg_f(d){let e=document.getElementById(\"val_f\"); let nv=parseFloat(e.innerText)+d; e.innerText=nv; upd(\"f\",nv);}";
    html += "</script></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/update", []() {
    if (server.hasArg("i")) cfg_int = server.arg("i").toInt();
    if (server.hasArg("f")) cfg_float = server.arg("f").toFloat();
    if (server.hasArg("s")) cfg_str = server.arg("s");
    server.send(200, "text/plain", "OK");
  });

  server.on("/save_flash", []() {
    saveConfig();
    server.sendHeader("Location", "/app");
    server.send(303);
  });

  server.on("/factory_reset", []() {
    server.send(200, "text/plain", "Formatting and Sleeping...");
    delay(500);
    LittleFS.format();
    throbLED(3);
    ESP.deepSleep(0);  if(debug){ Serial.println("body"); }
  });

  server.on("/settime", []() {
    if (server.hasArg("t")) {
      struct timeval tv = { .tv_sec = (time_t)server.arg("t").toInt() };
      settimeofday(&tv, NULL);
      server.send(200, "text/plain", "OK");
    }
  });

  server.begin();

  // Final Startup Log
  Serial.println("\n--- STARTUP LOG ---");
  Serial.printf("Int: %d\nFloat: %f\nStr: %s\nIP: %s\n", cfg_int, cfg_float, cfg_str.c_str(), WiFi.localIP().toString().c_str());
  Serial.println("-------------------");
  Serial.print("ver ");         Serial.println(ver);
  Serial.print("duty ");        Serial.println(duty);
  Serial.print("correction ");  Serial.println(correction);
  Serial.print("mark ");        Serial.println(mark);
  Serial.print("space ");       Serial.println(space); 
  Serial.print("debug ");       Serial.println(debug); 
  delay(1000);

  if(! debug){
    Serial.end();

    pinMode(CP_pin, OUTPUT);
    pinMode(PE_pin, OUTPUT);
    digitalWrite(CP_pin, HIGH);
    digitalWrite(PE_pin, LOW);
    delay(3000);

    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE); // 5 ticks per uS
    timer1_write(31250); timer1_attachInterrupt(ISR_CP_LO);
  }
}

void loop() {
  drd->loop();
  server.handleClient();
  ArduinoOTA.handle();
  MDNS.update();

  if(cfg_float != prv_float){
    blinkNumber(int(cfg_float));
    prv_float = cfg_float;
    duty = cfg_float /0.6 ;
    if(debug){ Serial.println(duty); }
    mark = floor((duty * k) - correction); space = period - mark - correction;
  }
  delay(1000);
}
