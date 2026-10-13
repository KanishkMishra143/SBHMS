/*
  BRIDGE HEALTH MONITOR — HX711 CALIBRATION FACTOR DIAGNOSTIC
  Board: ESP32-WROOM-32 / ESP32 DevKit

  HX711 wiring:
    DOUT / DT -> GPIO 33
    SCK / CLK -> GPIO 32
    VCC and GND -> according to the HX711 breakout's requirements

  Optional LCD:
    16x2 I2C LCD backpack, address 0x27
    SDA -> GPIO 21, SCL -> GPIO 22

  Libraries:
    - HX711 by Bogdan Necula (bogde)
    - LiquidCrystal_I2C compatible with lcd.init() and lcd.backlight()

  PURPOSE
    1. Tare the unloaded platform.
    2. Place a known mass on the platform.
    3. Calculate a calibration factor in raw counts per gram.
    4. Apply it temporarily and display/check the measured mass.

  COMMANDS (Serial Monitor 115200 baud, Newline):
    HELP       - print instructions
    INFO       - print pins and calibration state
    TARE       - tare with the platform unloaded
    CAL 1000   - calculate factor using a 1000 g known mass
    WEIGH      - take a reading using the current factor
    RAW        - print a raw reading
    CLEAR      - clear the current factor (does not erase saved settings)

  IMPORTANT
    - Replace 1000 with the actual known mass in grams.
    - Calibration is held in RAM only and is lost after reset/power-off.
    - Record the printed factor if you want to enter it into another sketch.
    - A negative factor is possible and can be correct for the chosen wiring polarity.
    - This is a calibration aid, not a certified weighing instrument.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>

static const int HX_DOUT = 33;
static const int HX_SCK  = 32;
static const uint8_t LCD_ADDRESS = 0x27;
static const int LCD_COLUMNS = 16;
static const int LCD_ROWS = 2;
static const int SAMPLE_COUNT = 20;
static const unsigned long READY_TIMEOUT_MS = 15000;

HX711 scale;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

bool tareValid = false;
bool calibrationValid = false;
long tareRaw = 0;
float calibrationFactorCountsPerGram = 0.0f;

void lcdMessage(const char *line1, const char *line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

bool readAverageBounded(int requested, long &average, int &obtained) {
  long long sum = 0;
  obtained = 0;
  const unsigned long started = millis();

  while (obtained < requested && (millis() - started) < READY_TIMEOUT_MS) {
    if (scale.is_ready()) {
      sum += scale.read();
      obtained++;
      delay(2);
    } else {
      delay(5);
    }
  }

  if (obtained == 0) return false;
  average = (long)(sum / obtained);
  return obtained == requested;
}

bool collectAverage(const char *label, long &average) {
  int obtained = 0;
  Serial.printf("\n%s: collecting %d samples...\n", label, SAMPLE_COUNT);
  lcdMessage("Reading HX711", label);

  const bool complete = readAverageBounded(SAMPLE_COUNT, average, obtained);
  Serial.printf("Samples acquired: %d/%d\n", obtained, SAMPLE_COUNT);
  if (obtained > 0) Serial.printf("Average raw count: %ld\n", average);

  if (!complete) {
    Serial.println(F("ERROR: HX711 did not provide enough readings before timeout."));
    lcdMessage("HX711 timeout", "Check wiring");
    return false;
  }
  return true;
}

void printHelp() {
  Serial.println(F("\n=== HX711 Calibration Factor Diagnostic ==="));
  Serial.println(F("Commands (Serial Monitor: 115200 baud, Newline):"));
  Serial.println(F("  HELP      - show this help"));
  Serial.println(F("  INFO      - show pins and calibration state"));
  Serial.println(F("  TARE      - platform EMPTY; capture zero reference"));
  Serial.println(F("  CAL 1000  - platform LOADED with known 1000 g mass; calculate factor"));
  Serial.println(F("  WEIGH     - measure current mass using the calculated factor"));
  Serial.println(F("  RAW       - read one raw ADC value"));
  Serial.println(F("  CLEAR     - clear current calibration factor in RAM"));
  Serial.println(F("Formula: factor = (loadedRawAverage - tareRaw) / knownMassGrams"));
  Serial.println(F("Units: counts/gram; displayed mass is in grams."));
  Serial.println(F("Calibration is not saved to flash; record the factor manually."));
}

void doTare() {
  Serial.println(F("\nTARE: remove ALL weight from the platform and keep it still."));
  Serial.println(F("Starting tare capture in 3 seconds..."));
  lcdMessage("Remove all load", "Tare in 3 sec");
  delay(3000);

  long measured = 0;
  if (!collectAverage("TARE", measured)) return;

  tareRaw = measured;
  tareValid = true;
  calibrationValid = false;
  scale.set_offset(tareRaw);
  scale.set_scale(1.0f);

  Serial.printf("TARE_RAW=%ld\n", tareRaw);
  Serial.println(F("Tare captured. Now place a known mass and enter CAL <grams>."));
  lcdMessage("Tare complete", "Place known mass");
}

void doCalibration(float knownMassGrams) {
  if (!tareValid) {
    Serial.println(F("ERROR: Run TARE first with the platform unloaded."));
    lcdMessage("Run TARE first", "Platform empty");
    return;
  }
  if (!(knownMassGrams > 0.0f) || knownMassGrams > 100000.0f) {
    Serial.println(F("ERROR: known mass must be greater than 0 and at most 100000 grams."));
    return;
  }

  Serial.printf("\nCALIBRATION: known mass = %.3f g\n", knownMassGrams);
  Serial.println(F("Place the known mass centrally on the platform and keep it still."));
  lcdMessage("Known mass on", "Capturing data");
  delay(1500);

  long loadedRaw = 0;
  if (!collectAverage("LOADED", loadedRaw)) return;

  const long difference = loadedRaw - tareRaw;
  const float factor = (float)difference / knownMassGrams;
  if (factor == 0.0f) {
    Serial.println(F("ERROR: raw difference is zero; cannot calculate a factor."));
    Serial.println(F("Check load-cell wiring, mechanical mounting, and known mass."));
    lcdMessage("No raw change", "Check load cell");
    return;
  }

  calibrationFactorCountsPerGram = factor;
  calibrationValid = true;
  scale.set_offset(tareRaw);
  scale.set_scale(calibrationFactorCountsPerGram);

  Serial.println(F("\n========== CALIBRATION RESULT =========="));
  Serial.printf("TARE_RAW=%ld\n", tareRaw);
  Serial.printf("LOADED_RAW=%ld\n", loadedRaw);
  Serial.printf("RAW_DIFFERENCE=%ld\n", difference);
  Serial.printf("KNOWN_MASS_GRAMS=%.3f\n", knownMassGrams);
  Serial.printf("CALIBRATION_FACTOR_COUNTS_PER_GRAM=%.6f\n", calibrationFactorCountsPerGram);
  Serial.println(F("Copy the factor above exactly, including its sign."));
  Serial.println(F("Calibration is active only until this ESP32 resets or loses power."));
  Serial.println(F("Remove the known mass, then type WEIGH to check zero; reapply it to verify."));

  char line2[17];
  snprintf(line2, sizeof(line2), "Factor %.3f", calibrationFactorCountsPerGram);
  lcdMessage("Calibration set", line2);
}

void doWeigh() {
  if (!tareValid || !calibrationValid) {
    Serial.println(F("ERROR: Run TARE, then CAL <known_mass_grams>, first."));
    lcdMessage("Not calibrated", "TARE then CAL");
    return;
  }

  int obtained = 0;
  long rawAverage = 0;
  if (!readAverageBounded(SAMPLE_COUNT, rawAverage, obtained)) {
    Serial.printf("ERROR: only %d/%d samples; HX711 timeout.\n", obtained, SAMPLE_COUNT);
    lcdMessage("HX711 timeout", "Check wiring");
    return;
  }

  const float grams = ((float)(rawAverage - tareRaw)) / calibrationFactorCountsPerGram;
  Serial.printf("WEIGH_RAW=%ld, TARE_RAW=%ld, MASS=%.2f g (%.4f kg), SAMPLES=%d\n",
                rawAverage, tareRaw, grams, grams / 1000.0f, obtained);

  char line1[17];
  char line2[17];
  snprintf(line1, sizeof(line1), "Mass: %.1f g", grams);
  snprintf(line2, sizeof(line2), "Raw:%ld", rawAverage);
  lcdMessage(line1, line2);
}

void doRaw() {
  if (!scale.is_ready()) {
    Serial.println(F("NOT READY: HX711 has no completed conversion."));
    lcdMessage("HX711 not ready", "Check wiring");
    return;
  }
  const long raw = scale.read();
  Serial.printf("RAW=%ld\n", raw);
}

void printInfo() {
  Serial.printf("HX711 DOUT=GPIO%d, SCK=GPIO%d\n", HX_DOUT, HX_SCK);
  Serial.printf("LCD I2C address=0x%02X, SDA=GPIO21, SCL=GPIO22\n", LCD_ADDRESS);
  Serial.printf("HX711 readiness=%s\n", scale.is_ready() ? "READY" : "NOT READY");
  Serial.printf("Tare valid=%s, tare raw=%ld\n", tareValid ? "YES" : "NO", tareRaw);
  Serial.printf("Calibration valid=%s, factor=%.6f counts/gram\n",
                calibrationValid ? "YES" : "NO", calibrationFactorCountsPerGram);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcdMessage("HX711 Calib.", "Starting...");
  delay(300);

  scale.begin(HX_DOUT, HX_SCK);
  Serial.println(F("\n=== HX711 Calibration Factor Diagnostic ==="));
  Serial.printf("Initial HX711 readiness: %s\n", scale.is_ready() ? "READY" : "NOT READY");
  Serial.println(F("LCD: I2C 0x27, SDA GPIO21, SCL GPIO22"));
  Serial.println(F("HX711: DOUT GPIO33, SCK GPIO32"));
  printHelp();
  lcdMessage("Type TARE", "Serial Monitor");
}

void loop() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();
  if (!command.length()) return;

  String upper = command;
  upper.toUpperCase();

  if (upper == "HELP") {
    printHelp();
  } else if (upper == "INFO") {
    printInfo();
  } else if (upper == "TARE") {
    doTare();
  } else if (upper == "WEIGH") {
    doWeigh();
  } else if (upper == "RAW") {
    doRaw();
  } else if (upper == "CLEAR") {
    calibrationValid = false;
    calibrationFactorCountsPerGram = 0.0f;
    scale.set_scale(1.0f);
    Serial.println(F("Calibration factor cleared from RAM. Tare reference retained."));
    lcdMessage("Factor cleared", "Run CAL again");
  } else if (upper.startsWith("CAL ")) {
    const float knownMassGrams = command.substring(4).toFloat();
    doCalibration(knownMassGrams);
  } else {
    Serial.println(F("Unknown command. Type HELP."));
  }
}
