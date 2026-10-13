/*
  BRIDGE HEALTH MONITOR — 16x2 I2C LCD TEST
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: SDA = GPIO21, SCL = GPIO22; default backpack address = 0x27
  Library: LiquidCrystal_I2C (same library used by the final project sketch)
  Purpose: Tests LCD initialization, backlight, text rendering and basic updates.
  Operation:
    1. Upload by itself.
    2. Open Serial Monitor at 115200 baud, Newline.
    3. Type HELP. Commands: HELP, CLEAR, BACKLIGHT ON, BACKLIGHT OFF, DEMO.
  If the screen lights but shows no text, adjust the backpack contrast potentiometer.
  If your I2C scanner reports a different LCD address, change LCD_ADDR below.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static const int SDA_PIN=21, SCL_PIN=22;
static const uint8_t LCD_ADDR=0x27;
LiquidCrystal_I2C lcd(LCD_ADDR,16,2);
unsigned long lastDemo=0;
uint32_t counter=0;

void showHelp() {
  Serial.println(F("\nCommands: HELP, CLEAR, BACKLIGHT ON, BACKLIGHT OFF, DEMO"));
}

void runDemo() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Bridge Monitor");
  lcd.setCursor(0,1); lcd.print("LCD test PASS");
  Serial.printf("LCD demo written at I2C address 0x%02X.\n",LCD_ADDR);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  Wire.begin(SDA_PIN,SCL_PIN);
  Wire.setClock(100000);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  Serial.println(F("\n=== LCD I2C Test ==="));
  Serial.printf("Using address 0x%02X, SDA=GPIO21, SCL=GPIO22.\n",LCD_ADDR);
  showHelp();
  runDemo();
}

void loop() {
  if (Serial.available()) {
    String command=Serial.readStringUntil('\n');
    command.trim(); command.toUpperCase();
    if(command=="HELP") showHelp();
    else if(command=="CLEAR") { lcd.clear(); Serial.println(F("LCD cleared.")); }
    else if(command=="BACKLIGHT ON") { lcd.backlight(); Serial.println(F("Backlight ON.")); }
    else if(command=="BACKLIGHT OFF") { lcd.noBacklight(); Serial.println(F("Backlight OFF.")); }
    else if(command=="DEMO") runDemo();
    else if(command.length()) Serial.println(F("Unknown command. Type HELP."));
  }
  if(millis()-lastDemo>=1000) {
    lastDemo=millis();
    ++counter;
    lcd.setCursor(0,1);
    lcd.print("Count: ");
    lcd.print(counter);
    lcd.print("       ");
  }
}
