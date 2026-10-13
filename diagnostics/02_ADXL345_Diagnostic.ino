/*
  BRIDGE HEALTH MONITOR — ADXL345 DIAGNOSTIC
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: SDA = GPIO21, SCL = GPIO22
  Address: tries 0x53, then 0x1D
  Purpose: Checks ADXL345 device ID, configures measurement mode, and streams raw counts
           plus approximate acceleration in g. This is a sensor test, not the final detector.
  Operation:
    1. Upload this sketch by itself; keep LCD/I2C wiring as-is if desired.
    2. Open Serial Monitor at 115200 baud, Newline.
    3. Commands: HELP, READ, INFO, RATE 1, RATE 5, RATE 10.
  Expected device ID register 0x00: 0xE5.
  Notes: Keep the sensor still first; expect gravity to appear on one axis (~1 g).
*/

#include <Wire.h>
#include <math.h>

static const int SDA_PIN = 21, SCL_PIN = 22;
static const uint8_t ADDR_A = 0x53, ADDR_B = 0x1D;
static const float G_PER_LSB = 0.0039f;
static uint8_t adxlAddress = 0;
static unsigned long intervalMs = 200;
static unsigned long lastRead = 0;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(adxlAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(adxlAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(adxlAddress, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool readXYZ(int16_t &x, int16_t &y, int16_t &z) {
  Wire.beginTransmission(adxlAddress);
  Wire.write(0x32); // DATAX0, auto-increment through six data bytes
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(adxlAddress, (uint8_t)6) != 6) return false;
  uint8_t x0=Wire.read(), x1=Wire.read();
  uint8_t y0=Wire.read(), y1=Wire.read();
  uint8_t z0=Wire.read(), z1=Wire.read();
  x=(int16_t)(((uint16_t)x1<<8)|x0);
  y=(int16_t)(((uint16_t)y1<<8)|y0);
  z=(int16_t)(((uint16_t)z1<<8)|z0);
  return true;
}

bool initADXL() {
  uint8_t id = 0;
  adxlAddress = ADDR_A;
  if (!readReg(0x00, id) || id != 0xE5) {
    adxlAddress = ADDR_B;
    if (!readReg(0x00, id) || id != 0xE5) {
      adxlAddress = 0;
      Serial.println(F("FAIL: ADXL345 not found at 0x53 or 0x1D."));
      Serial.println(F("Run the I2C scanner; check power, GND, SDA and SCL."));
      return false;
    }
  }

  Serial.printf("ADXL345 found at 0x%02X; DEVID=0x%02X\n", adxlAddress, id);
  bool ok = writeReg(0x31, 0x08); // full resolution, +/-2 g
  ok = writeReg(0x2C, 0x0B) && ok; // 200 Hz
  ok = writeReg(0x2D, 0x08) && ok; // measurement mode
  delay(20);
  uint8_t verify = 0;
  if (!ok || !readReg(0x00, verify) || verify != 0xE5) {
    Serial.println(F("FAIL: register setup/readback failed."));
    return false;
  }
  Serial.println(F("PASS: configured full-resolution +/-2 g, 200 Hz, measurement mode."));
  return true;
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  HELP    - show help"));
  Serial.println(F("  READ    - print one sample immediately"));
  Serial.println(F("  INFO    - print address and configuration"));
  Serial.println(F("  RATE 1  - stream every 1 second"));
  Serial.println(F("  RATE 5  - stream every 5 seconds"));
  Serial.println(F("  RATE 10 - stream every 10 seconds"));
  Serial.println(F("Continuous samples are printed at the selected interval."));
}

void printSample() {
  if (!adxlAddress) {
    Serial.println(F("No sensor configured. Reset board after checking wiring."));
    return;
  }
  int16_t x,y,z;
  if (!readXYZ(x,y,z)) {
    Serial.println(F("READ ERROR: I2C transaction failed."));
    return;
  }
  float gx=x*G_PER_LSB, gy=y*G_PER_LSB, gz=z*G_PER_LSB;
  float magnitude=sqrtf(gx*gx+gy*gy+gz*gz);
  Serial.printf("RAW X=%d Y=%d Z=%d | g X=%.3f Y=%.3f Z=%.3f | MAG=%.3f g\n",
                x,y,z,gx,gy,gz,magnitude);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(300);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Serial.println(F("\n=== ADXL345 Diagnostic ==="));
  printHelp();
  initADXL();
  lastRead = millis();
}

void loop() {
  if (Serial.available()) {
    String command=Serial.readStringUntil('\n');
    command.trim(); command.toUpperCase();
    if (command=="HELP") printHelp();
    else if (command=="READ") printSample();
    else if (command=="INFO") {
      Serial.printf("Address: 0x%02X; SDA=GPIO21; SCL=GPIO22; scale approx %.4f g/LSB.\n",
                    adxlAddress, G_PER_LSB);
    } else if (command=="RATE 1") intervalMs=1000;
    else if (command=="RATE 5") intervalMs=5000;
    else if (command=="RATE 10") intervalMs=10000;
    else if (command.length()) Serial.println(F("Unknown command. Type HELP."));
  }
  if (millis()-lastRead>=intervalMs) {
    lastRead=millis();
    printSample();
  }
}
