/*
  BRIDGE HEALTH MONITOR — WI-FI + HTTP DIAGNOSTIC
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Purpose: Tests Wi-Fi association, IP assignment, mDNS, and an HTTP endpoint.
  IMPORTANT: Replace WIFI_SSID and WIFI_PASSWORD below with your own credentials before upload.
  This file intentionally does not contain the credentials from the main project.
  Operation:
    1. Enter credentials below, upload, open Serial Monitor at 115200 baud.
    2. Visit the printed IP address in a browser, or try http://esp32-diagnostic.local
    3. Type HELP. Commands: HELP, STATUS, RECONNECT.
  This is a standalone diagnostic server, not the main dashboard. It uses port 80.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

static const char* WIFI_SSID = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
static const char* HOSTNAME = "esp32-diagnostic";

WebServer server(80);
bool mdnsOK=false;

void handleRoot() {
  String page="<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  page+="<title>ESP32 diagnostic</title></head><body style='font:18px sans-serif'>";
  page+="<h1>ESP32 Wi-Fi diagnostic PASS</h1><p>HTTP server is responding.</p><p>IP: ";
  page+=WiFi.localIP().toString();
  page+="</p><p>RSSI: "+String(WiFi.RSSI())+" dBm</p></body></html>";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"text/html",page);
}

void handleStatus() {
  String json="{\"connected\":";
  json+=(WiFi.status()==WL_CONNECTED)?"true":"false";
  json+=",\"ip\":\""+WiFi.localIP().toString()+"\",\"rssi\":"+String(WiFi.RSSI());
  json+=",\"mdns\":"; json+=mdnsOK?"true":"false"; json+="}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",json);
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  HELP      - show help"));
  Serial.println(F("  STATUS    - print Wi-Fi/IP/RSSI/server status"));
  Serial.println(F("  RECONNECT - disconnect and reconnect to configured Wi-Fi"));
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.disconnect();
  delay(300);
  Serial.printf("Connecting to SSID: %s\n",WIFI_SSID);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  unsigned long started=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-started<20000UL) {
    delay(300); Serial.print('.');
  }
  Serial.println();
  if(WiFi.status()!=WL_CONNECTED) {
    Serial.println(F("FAIL: Wi-Fi did not connect. Check SSID/password and 2.4 GHz compatibility."));
    return false;
  }
  Serial.println(F("PASS: Wi-Fi connected."));
  Serial.print(F("IP address: ")); Serial.println(WiFi.localIP());
  Serial.print(F("RSSI: ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));

  if(!mdnsOK) {
    mdnsOK=MDNS.begin(HOSTNAME);
    if(mdnsOK) {
      MDNS.addService("http","tcp",80);
      Serial.printf("mDNS: http://%s.local\n",HOSTNAME);
    } else Serial.println(F("mDNS failed; use the IP address."));
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(300);
  Serial.println(F("\n=== Wi-Fi + HTTP Diagnostic ==="));
  printHelp();
  server.on("/",HTTP_GET,handleRoot);
  server.on("/status",HTTP_GET,handleStatus);
  server.begin();
  Serial.println(F("HTTP server started on port 80."));
  connectWiFi();
}

void loop() {
  if(WiFi.status()==WL_CONNECTED) server.handleClient();
  if(Serial.available()) {
    String command=Serial.readStringUntil('\n');
    command.trim(); command.toUpperCase();
    if(command=="HELP") printHelp();
    else if(command=="STATUS") {
      Serial.printf("Wi-Fi=%s, IP=%s, RSSI=%d dBm, mDNS=%s\n",
        WiFi.status()==WL_CONNECTED?"CONNECTED":"DISCONNECTED",
        WiFi.localIP().toString().c_str(),WiFi.RSSI(),mdnsOK?"OK":"FAILED");
    } else if(command=="RECONNECT") {
      WiFi.disconnect(); delay(300); connectWiFi();
    } else if(command.length()) Serial.println(F("Unknown command. Type HELP."));
  }
}
