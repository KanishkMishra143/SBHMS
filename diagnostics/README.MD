BRIDGE HEALTH MONITOR — DIAGNOSTICS SUITE
==========================================

IMPORTANT ABOUT FILE EXTENSIONS
The ESP32 Arduino framework compiles sketches as C++, not plain C. These files use
Arduino APIs such as Wire, WiFi, WebServer and LiquidCrystal_I2C, so they are supplied
as .ino files. Renaming them to .c will not make them compile as C.

FILES
-----
01_I2C_Scanner.ino
  Scans the I2C bus on GPIO21/22. Expected addresses: LCD usually 0x27, ADXL345 0x53
  (sometimes 0x1D). Start here if either I2C device is missing.

02_ADXL345_Diagnostic.ino
  Verifies ADXL345 DEVID=0xE5, configures the sensor and prints raw and g values.

03_LCD_I2C_Test.ino
  Tests the LCD and backlight using LiquidCrystal_I2C at address 0x27.

04_RGB_Buzzer_Test.ino
  Tests the common-anode RGB LED (active LOW) and buzzer driver (GPIO25 active HIGH).

05_HX711_Raw_Diagnostic.ino
  Tests HX711 readiness and prints raw ADC counts. Does not calibrate weight.
  Requires an HX711 library compatible with the bogde HX711 API.

06_WiFi_HTTP_Diagnostic.ino
  Tests Wi-Fi, IP address, mDNS and a small HTTP server. Enter your Wi-Fi credentials
  in that file before uploading. It does not run the final dashboard.

HOW TO USE
----------
1. Keep a backup of the locked final .ino sketch. These diagnostic sketches do not replace it.
2. Open Arduino IDE and select the ESP32 board/COM port.
3. Open ONE diagnostic .ino at a time. Uploading a diagnostic temporarily replaces the
   program running on the ESP32; upload the locked final sketch again when finished.
4. Open Serial Monitor at 115200 baud. Choose Newline as the line ending where commands
   are used. Type HELP in each sketch for its commands.
5. Close Serial Monitor before starting an upload if the port is busy.
6. If an I2C scan finds no devices, check power, ground, GPIO21 SDA, GPIO22 SCL, and wiring.
7. Run I2C scanner first, then ADXL345 and LCD tests, then outputs, HX711, and Wi-Fi.

PIN MAP FROM THE LOCKED BASELINE
--------------------------------
I2C SDA: GPIO21
I2C SCL: GPIO22
RGB red: GPIO14 (common-anode, active LOW)
RGB green: GPIO26 (common-anode, active LOW)
RGB blue: GPIO27 (common-anode, active LOW)
Buzzer driver: GPIO25 (active HIGH)
HX711 DOUT: GPIO33 (reserved/not read by the locked final sketch)
HX711 SCK: GPIO32 (reserved/not read by the locked final sketch)

LIMITATIONS
-----------
- Test one subsystem at a time; a passing diagnostic does not prove the entire system is safe.
- The HX711 test reports counts only. Calibration and mechanical mounting remain separate tasks.
- The ADXL345 demo's approximate g conversion is for diagnosis, not validated metrology.
- Wi-Fi test needs your own SSID/password and a 2.4 GHz network supported by the ESP32.
- No diagnostic sketch writes to or changes the locked final source file.
