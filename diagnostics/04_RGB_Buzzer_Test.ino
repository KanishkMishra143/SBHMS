/*
  BRIDGE HEALTH MONITOR — RGB LED + BUZZER TEST
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: RGB R=GPIO14, G=GPIO26, B=GPIO27; buzzer control=GPIO25.
  Wiring assumptions:
    - Common-anode RGB LED tied to 3V3; each color has its own 180-ohm resistor.
    - LED channels are active LOW.
    - Buzzer + to 5V, buzzer - to BC547 collector, emitter to GND,
      GPIO25 to base through 1k; a 10k base-emitter pull-down is recommended.
  Purpose: Tests each output separately. No automatic alert logic is running in this sketch.
  Operation: Serial Monitor 115200 baud, Newline. Type HELP.
  Commands: R, G, B, OFF, RGB, BUZZER ON, BUZZER OFF, TEST.
  Note: Never connect a 5V buzzer directly to an ESP32 GPIO.
*/

static const int LED_R=14, LED_G=26, LED_B=27, BUZZER=25;

void setChannel(int pin,bool on) { digitalWrite(pin,on?LOW:HIGH); }
void setRGB(bool r,bool g,bool b) { setChannel(LED_R,r); setChannel(LED_G,g); setChannel(LED_B,b); }

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  HELP        - show help"));
  Serial.println(F("  R / G / B   - turn on that LED color only"));
  Serial.println(F("  RGB         - turn on all three channels (white-ish)"));
  Serial.println(F("  OFF         - turn all LED channels off"));
  Serial.println(F("  BUZZER ON   - activate buzzer"));
  Serial.println(F("  BUZZER OFF  - deactivate buzzer"));
  Serial.println(F("  TEST        - run a short LED/buzzer sequence"));
}

void runTest() {
  Serial.println(F("Running output test..."));
  setRGB(true,false,false); delay(500);
  setRGB(false,true,false); delay(500);
  setRGB(false,false,true); delay(500);
  setRGB(true,true,true); delay(500);
  setRGB(false,false,false);
  digitalWrite(BUZZER,HIGH); delay(150); digitalWrite(BUZZER,LOW);
  Serial.println(F("Output sequence complete."));
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  pinMode(LED_R,OUTPUT); pinMode(LED_G,OUTPUT); pinMode(LED_B,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  setRGB(false,false,false);
  digitalWrite(BUZZER,LOW);
  delay(200);
  Serial.println(F("\n=== RGB + Buzzer Test ==="));
  printHelp();
}

void loop() {
  if(!Serial.available()) return;
  String command=Serial.readStringUntil('\n');
  command.trim(); command.toUpperCase();
  if(command=="HELP") printHelp();
  else if(command=="R") { setRGB(true,false,false); Serial.println(F("Red ON.")); }
  else if(command=="G") { setRGB(false,true,false); Serial.println(F("Green ON.")); }
  else if(command=="B") { setRGB(false,false,true); Serial.println(F("Blue ON.")); }
  else if(command=="RGB") { setRGB(true,true,true); Serial.println(F("All channels ON.")); }
  else if(command=="OFF") { setRGB(false,false,false); digitalWrite(BUZZER,LOW); Serial.println(F("LEDs and buzzer OFF.")); }
  else if(command=="BUZZER ON") { digitalWrite(BUZZER,HIGH); Serial.println(F("Buzzer ON.")); }
  else if(command=="BUZZER OFF") { digitalWrite(BUZZER,LOW); Serial.println(F("Buzzer OFF.")); }
  else if(command=="TEST") runTest();
  else if(command.length()) Serial.println(F("Unknown command. Type HELP."));
}
