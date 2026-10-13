/*
  BRIDGE HEALTH MONITOR — I2C SCANNER
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: SDA = GPIO21, SCL = GPIO22
  Purpose: Finds I2C devices and prints their 7-bit addresses.
  Expected with the current setup:
    LCD backpack: commonly 0x27
    ADXL345:      0x53 (or 0x1D if its address-select pin is high)
  Operation:
    1. Upload this sketch by itself.
    2. Open Serial Monitor at 115200 baud; select Newline.
    3. Type HELP for commands, SCAN to scan again, or INFO for wiring notes.
  Safety:
    This only scans the I2C bus; it does not write to device registers.
*/

#include <Wire.h>

static const int I2C_SDA_PIN = 21;
static const int I2C_SCL_PIN = 22;

void scanBus() {
  Serial.println();
  Serial.println(F("Scanning I2C bus on SDA=GPIO21, SCL=GPIO22..."));
  uint8_t found = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  Device found at 0x%02X (%u decimal)", address, address);
      if (address == 0x27) Serial.print(F("  [expected LCD backpack address]"));
      if (address == 0x53) Serial.print(F("  [expected ADXL345 address]"));
      if (address == 0x1D) Serial.print(F("  [possible ADXL345 alternate address]"));
      Serial.println();
      ++found;
    }
  }

  if (!found) {
    Serial.println(F("No I2C devices found."));
    Serial.println(F("Check 3V3/GND, SDA->GPIO21, SCL->GPIO22, common GND, and wiring."));
  } else {
    Serial.printf("Scan complete: %u device(s) found.\n", found);
    if (found && found < 2) {
      Serial.println(F("If both LCD and ADXL345 are connected, expect two addresses."));
    }
  }
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  HELP  - show this help"));
  Serial.println(F("  SCAN  - scan the I2C bus again"));
  Serial.println(F("  INFO  - show expected wiring/addresses"));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  Serial.println(F("\n=== ESP32 I2C Scanner ==="));
  printHelp();
  scanBus();
}

void loop() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();

  if (command == "HELP") printHelp();
  else if (command == "SCAN") scanBus();
  else if (command == "INFO") {
    Serial.println(F("SDA=GPIO21, SCL=GPIO22; LCD usually 0x27; ADXL345 usually 0x53."));
    Serial.println(F("Some ADXL345 boards use 0x1D. Scanner does not prove a device works correctly."));
  } else if (command.length()) {
    Serial.println(F("Unknown command. Type HELP."));
  }
}
