/*
  BRIDGE HEALTH MONITOR — HX711 RAW DIAGNOSTIC
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: HX711 DOUT=GPIO33, SCK=GPIO32 (as reserved in the final project wiring).
  Library: "HX711" by Bogdan Necula (bogde) or a compatible library exposing the same API.
  Purpose: Checks whether HX711 becomes ready and reports raw ADC counts. It does NOT
           assume a calibration factor and does NOT report calibrated kilograms.
  Operation:
    1. Install the HX711 library in Arduino IDE Library Manager.
    2. Upload this sketch and open Serial Monitor at 115200 baud, Newline.
    3. Type HELP. Commands: HELP, RAW, AVG 10, AVG 25, TARE, INFO.
  Troubleshooting:
    - "NOT READY": check HX711 power/GND, DOUT/SCK pin order and common ground.
    - Raw values should respond when the load changes, but may drift/noise.
    - Do not use this diagnostic as a calibrated weighing instrument.
*/

#include <HX711.h>

static const int HX_DOUT=33, HX_SCK=32;
HX711 scale;
bool hxReady=false;

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  HELP   - show help"));
  Serial.println(F("  RAW    - read one raw ADC value if ready"));
  Serial.println(F("  AVG n  - average n raw readings (1..100)"));
  Serial.println(F("  TARE   - record a zero reference and show subsequent relative counts"));
  Serial.println(F("  INFO   - show pins and current readiness"));
  Serial.println(F("This sketch reports counts only; no calibration factor is applied."));
}

void readRawOnce() {
  if(!scale.is_ready()) { Serial.println(F("NOT READY: HX711 DOUT is not indicating data-ready.")); return; }
  long raw=scale.read();
  Serial.printf("RAW=%ld\n",raw);
}

void readAverage(int n) {
  n=constrain(n,1,100);
  long sum=0;
  int got=0;
  for(int i=0;i<n;i++) {
    if(scale.is_ready()) { sum+=scale.read(); ++got; }
    else delay(20);
  }
  if(!got) { Serial.println(F("No readings acquired; HX711 not ready.")); return; }
  Serial.printf("AVG_RAW=%ld (readings=%d requested=%d)\n",sum/got,got,n);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(300);
  Serial.println(F("\n=== HX711 Raw Diagnostic ==="));
  scale.begin(HX_DOUT,HX_SCK);
  hxReady=scale.is_ready();
  Serial.printf("Initial HX711 readiness: %s\n",hxReady?"READY":"NOT READY");
  printHelp();
}

void loop() {
  if(!Serial.available()) return;
  String command=Serial.readStringUntil('\n');
  command.trim();
  String upper=command; upper.toUpperCase();

  if(upper=="HELP") printHelp();
  else if(upper=="RAW") readRawOnce();
  else if(upper=="TARE") {
    if(!scale.is_ready()) Serial.println(F("NOT READY: cannot take reference."));
    else {
      long reference=scale.read_average(10);
      Serial.printf("Reference raw count (10-read average)=%ld\n",reference);
      Serial.println(F("This is informational only; no persistent tare/calibration is applied."));
    }
  } else if(upper=="INFO") {
    Serial.printf("DOUT=GPIO%d, SCK=GPIO%d, readiness=%s\n",HX_DOUT,HX_SCK,
                  scale.is_ready()?"READY":"NOT READY");
  } else if(upper.startsWith("AVG ")) {
    int n=command.substring(4).toInt();
    if(n<1 || n>100) Serial.println(F("Use AVG 1 through AVG 100."));
    else readAverage(n);
  } else if(upper.length()) Serial.println(F("Unknown command. Type HELP."));
}
