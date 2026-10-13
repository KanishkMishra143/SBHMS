/*
  BRIDGE HEALTH MONITOR — ADXL345 DIAGNOSTIC + SERIAL PLOTTER
  Board: ESP32-WROOM-32 / ESP32 DevKit
  Pins: SDA = GPIO21, SCL = GPIO22
  Address: tries 0x53, then 0x1D
  Purpose:
    - Checks ADXL345 device ID (expected 0xE5).
    - Configures full-resolution +/-2 g mode at 200 Hz.
    - Provides readable raw counts and acceleration in g in TEXT mode.
    - Provides labelled X/Y/Z/MAG traces in Arduino Serial Plotter mode.
  Operation:
    1. Upload this sketch by itself.
    2. Open Serial Monitor at 115200 baud, Newline, then type HELP.
    3. Default is TEXT mode. Commands in Serial Monitor (115200 baud, Newline):
         HELP       - show commands
         READ       - print one text sample
         INFO       - show address/configuration
         RATE 1     - text sample every 1 second
         RATE 5     - text sample every 5 seconds
         RATE 10    - text sample every 10 seconds
         MODE TEXT  - readable diagnostics until reset
         MODE PLOT  - labelled graph stream until reset
    4. For the most reliable graph startup, set START_IN_PLOT_MODE to true below,
       upload, then open Tools > Serial Plotter at 115200 baud.
    5. For readable text diagnostics, set START_IN_PLOT_MODE to false, upload,
       then open Serial Monitor. Switching the serial window can reset some ESP32
       boards, so the compile-time setting is the reliable way to select a mode.
  Plot output is one line per sample:
    X:<g>,Y:<g>,Z:<g>,MAG:<g>
  IMPORTANT:
    - Serial Monitor and Serial Plotter cannot use the same COM/serial port at once.
    - Plot mode prints ONLY numeric labelled data lines after the mode changes, so
      diagnostic text does not contaminate the graph.
    - The START_IN_PLOT_MODE setting selects the mode after reset/re-upload.
    - Approximate scale is 0.0039 g/LSB for the configured +/-2 g full-resolution mode.
*/

#include <Wire.h>
#include <math.h>

static const int SDA_PIN = 21;
static const int SCL_PIN = 22;
static const uint8_t ADDR_A = 0x53;
static const uint8_t ADDR_B = 0x1D;
static const float G_PER_LSB = 0.0039f;
// Set true for clean Serial Plotter startup; set false for readable Serial Monitor output.
static const bool START_IN_PLOT_MODE = true;

static uint8_t adxlAddress = 0;
static unsigned long intervalMs = 200;
static unsigned long lastRead = 0;
static unsigned long lastPlot = 0;
static bool plotMode = START_IN_PLOT_MODE;
static bool sensorReady = false;

bool writeReg(uint8_t reg, uint8_t value) {
  if (!adxlAddress) return false;
  Wire.beginTransmission(adxlAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t &value) {
  if (!adxlAddress) return false;
  Wire.beginTransmission(adxlAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(adxlAddress, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

bool readXYZ(int16_t &x, int16_t &y, int16_t &z) {
  if (!adxlAddress) return false;
  Wire.beginTransmission(adxlAddress);
  Wire.write(0x32); // DATAX0; six consecutive data bytes follow
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(adxlAddress, (uint8_t)6) != 6) return false;

  uint8_t x0 = Wire.read(), x1 = Wire.read();
  uint8_t y0 = Wire.read(), y1 = Wire.read();
  uint8_t z0 = Wire.read(), z1 = Wire.read();

  x = (int16_t)(((uint16_t)x1 << 8) | x0);
  y = (int16_t)(((uint16_t)y1 << 8) | y0);
  z = (int16_t)(((uint16_t)z1 << 8) | z0);
  return true;
}

bool initADXL() {
  uint8_t id = 0;
  adxlAddress = ADDR_A;

  if (!readReg(0x00, id) || id != 0xE5) {
    adxlAddress = ADDR_B;
    if (!readReg(0x00, id) || id != 0xE5) {
      adxlAddress = 0;
      sensorReady = false;
      if (!plotMode) {
        Serial.println(F("FAIL: ADXL345 not found at 0x53 or 0x1D."));
        Serial.println(F("Run the I2C scanner; check power, GND, SDA and SCL."));
      }
      return false;
    }
  }

  Serial.printf("ADXL345 found at 0x%02X; DEVID=0x%02X\n", adxlAddress, id);

  bool ok = writeReg(0x31, 0x08);          // Full resolution, +/-2 g
  ok = writeReg(0x2C, 0x0B) && ok;         // 200 Hz output data rate
  ok = writeReg(0x2D, 0x08) && ok;         // Measurement mode
  delay(20);

  uint8_t verify = 0;
  if (!ok || !readReg(0x00, verify) || verify != 0xE5) {
    sensorReady = false;
    if (!plotMode) Serial.println(F("FAIL: ADXL345 register setup/readback failed."));
    return false;
  }

  sensorReady = true;
  if (!plotMode) Serial.println(F("PASS: full-resolution +/-2 g, 200 Hz, measurement mode."));
  return true;
}

bool getSample(int16_t &x, int16_t &y, int16_t &z,
               float &gx, float &gy, float &gz, float &magnitude) {
  if (!sensorReady || !readXYZ(x, y, z)) return false;

  gx = x * G_PER_LSB;
  gy = y * G_PER_LSB;
  gz = z * G_PER_LSB;
  magnitude = sqrtf(gx * gx + gy * gy + gz * gz);
  return true;
}

void printHelp() {
  Serial.println(F("\nCommands (Serial Monitor, 115200 baud):"));
  Serial.println(F("  HELP      - show this help"));
  Serial.println(F("  READ      - print one sample immediately"));
  Serial.println(F("  INFO      - show address, pins and scale"));
  Serial.println(F("  RATE 1    - text sample every 1 second"));
  Serial.println(F("  RATE 5    - text sample every 5 seconds"));
  Serial.println(F("  RATE 10   - text sample every 10 seconds"));
  Serial.println(F("  MODE TEXT - readable raw counts and g values (until reset)"));
  Serial.println(F("  MODE PLOT - labelled X/Y/Z/MAG graph stream (until reset)"));
  Serial.println(F("Plot labels are X, Y, Z, MAG; values are acceleration in g."));
  Serial.println(F("Reliable plot startup: set START_IN_PLOT_MODE=true and upload."));
}

void printTextSample() {
  if (!sensorReady) {
    Serial.println(F("SENSOR ERROR: ADXL345 is not initialized. Reset to retry."));
    return;
  }

  int16_t x, y, z;
  float gx, gy, gz, magnitude;
  if (!getSample(x, y, z, gx, gy, gz, magnitude)) {
    Serial.println(F("READ ERROR: I2C transaction failed."));
    return;
  }

  Serial.printf("RAW X=%d Y=%d Z=%d | g X=%.3f Y=%.3f Z=%.3f | MAG=%.3f g\n",
                x, y, z, gx, gy, gz, magnitude);
}

void printPlotSample() {
  int16_t x, y, z;
  float gx, gy, gz, magnitude;
  if (!getSample(x, y, z, gx, gy, gz, magnitude)) {
    // Keep plot output numeric-only. Emit NaN-labelled fields if sensor reads fail.
    Serial.println("X:nan,Y:nan,Z:nan,MAG:nan");
    return;
  }

  // Arduino Serial Plotter interprets label:value pairs as named data series.
  Serial.print("X:"); Serial.print(gx, 3);
  Serial.print(",Y:"); Serial.print(gy, 3);
  Serial.print(",Z:"); Serial.print(gz, 3);
  Serial.print(",MAG:"); Serial.println(magnitude, 3);
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "HELP") {
    if (!plotMode) printHelp();
    return;
  }

  if (command == "MODE PLOT") {
    plotMode = true;
    lastPlot = 0;
    Serial.println(F("")); // final mode-switch acknowledgement; reset/reopen plotter after this
    return;
  }

  if (command == "MODE TEXT") {
    plotMode = false;
    Serial.println(F("TEXT mode enabled."));
    printHelp();
    return;
  }

  if (plotMode) return; // Do not print text/commands into the graph stream.

  if (command == "READ") {
    printTextSample();
  } else if (command == "INFO") {
    Serial.printf("Address: 0x%02X; SDA=GPIO21; SCL=GPIO22; ODR=200 Hz; scale=%.4f g/LSB.\n",
                  adxlAddress, G_PER_LSB);
  } else if (command == "RATE 1") {
    intervalMs = 1000;
    Serial.println(F("Text sample interval set to 1 second."));
  } else if (command == "RATE 5") {
    intervalMs = 5000;
    Serial.println(F("Text sample interval set to 5 seconds."));
  } else if (command == "RATE 10") {
    intervalMs = 10000;
    Serial.println(F("Text sample interval set to 10 seconds."));
  } else if (command.length()) {
    Serial.println(F("Unknown command. Type HELP."));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!plotMode) {
    Serial.println(F("\n=== ADXL345 Diagnostic + Serial Plotter ==="));
    printHelp();
  }
  initADXL();
  lastRead = millis();
  lastPlot = millis();
}

void loop() {
  // Read serial commands in text mode. Plot mode ignores command text to protect graph data.
  if (Serial.available() && !plotMode) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  if (plotMode) {
    // Stream at 50 samples/s: fast enough for a live diagnostic graph without flooding USB.
    if (millis() - lastPlot >= 20) {
      lastPlot = millis();
      printPlotSample();
    }
  } else if (millis() - lastRead >= intervalMs) {
    lastRead = millis();
    printTextSample();
  }
}
