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
const int LED_PIN = 2; // Built-in LED for ESP-01S (GPIO2)
const int CP_pin = 2;
const int PE_pin = 1;

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

void setup() {
  Serial.begin(115200); delay(600); Serial.printf("\n\n\n");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED Off (Active Low)

  drd = new DoubleResetDetector(10, 0x00);
  LittleFS.begin();
  loadConfig();

  // Handle Double Reset (Factory Reset)
  if (drd->detectDoubleReset()) {
    Serial.println("Double Reset Detected! Clearing Config...");
    throbLED(3);
    LittleFS.format();
    ESP.deepSleep(0); 
  }

  // WiFi Captive Portal
  WiFiManager wm;
  if (!wm.autoConnect("ESP-EVSE-Config")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }

  MDNS.begin("ESP-EVSE");
  ArduinoOTA.begin();

  // --- WEB SERVER ROUTES ---

  server.on("/", []() {
    String html = "<h1>ESP-EVSE Home</h1><p>FW Date: " + String(__DATE__) + " " + String(__TIME__) + "</p>";
    html += "<p><a href='/app'>Configure Variables</a></p>";
    html += "<script>fetch('/settime?t='+Math.floor(Date.now()/1000));</script>";
    server.send(200, "text/html", html);
  });

  server.on("/app", []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;padding:20px;max-width:400px;margin:auto;} .row{margin:25px 0;} button{padding:10px 15px;} ";
    html += ".warn{background:#ff4444; color:white; border:none; width:100%; margin-top:40px; padding:15px;}</style></head><body>";
    html += "<h2>Settings</h2>";
    html += "<div class='row'><b>Int:</b> <button onclick='chg(-1)'>-</button> <span id='val_i'>" + String(cfg_int) + "</span> <button onclick='chg(1)'>+</button></div>";
    html += "<div class='row'><b>Float:</b> <br><input type='range' min='0' max='100' step='0.1' style='width:100%' value='" + String(cfg_float) + "' oninput='document.getElementById(\"dis_f\").innerText=this.value' onchange='upd(\"f\",this.value)'><br>Value: <span id='dis_f'>" + String(cfg_float) + "</span></div>";
    html += "<div class='row'><b>String:</b> <br><input type='text' style='width:100%;padding:8px;' value='" + cfg_str + "' onchange='upd(\"s\",this.value)'></div>";
    html += "<button onclick='location.href=\"/save_flash\"' style='width:100%'>Commit to Flash</button>";
    html += "<button class='warn' onclick='if(confirm(\"Clear all settings and Sleep?\"))location.href=\"/factory_reset\"'>Factory Reset & Sleep</button>";
    html += "<script>function upd(p,v){fetch('/update?'+p+'='+encodeURIComponent(v));} function chg(d){let e=document.getElementById(\"val_i\"); let nv=parseInt(e.innerText)+d; e.innerText=nv; upd(\"i\",nv);}</script></body></html>";
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
    ESP.deepSleep(0);
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
  Serial.print("ver ");
  Serial.println("2026-03-15 = ISRs");
  Serial.print("duty ");
  Serial.println(duty);
  Serial.print("correction ");
  Serial.println(correction);
  Serial.print("mark ");
  Serial.println(mark);
  Serial.print("space ");
  Serial.println(space); 
  delay(1000) ; Serial.end();

  pinMode(CP_pin, OUTPUT);
  pinMode(PE_pin, OUTPUT);
  digitalWrite(CP_pin, HIGH);
  digitalWrite(PE_pin, LOW);
  delay(3000);

  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE); // 5 ticks per uS
  timer1_write(31250); timer1_attachInterrupt(ISR_CP_LO);
}

void loop() {
  drd->loop();
  server.handleClient();
  ArduinoOTA.handle();
  MDNS.update();

  duty = cfg_float;
  mark = floor((duty * k) - correction); space = period - mark - correction;
  delay(1000);
}
