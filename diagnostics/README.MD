# ESP32 Bridge Health Monitor — Hardware Diagnostic Sketches

A collection of standalone Arduino sketches for checking the individual hardware modules used in an **ESP32-WROOM-32–based Bridge Health Monitoring System**.

These sketches are intended for **bring-up, wiring verification, and troubleshooting**. They test one subsystem at a time so a problem can be isolated before running the full bridge-monitoring firmware.

> **Important:** These are diagnostic programs, not the final integrated Bridge Health Monitor firmware. Uploading one replaces the program currently running on the ESP32. After testing, upload your main project sketch again.

---

## Table of Contents

- [What Is Included](#what-is-included)
- [Hardware and Wiring Reference](#hardware-and-wiring-reference)
- [Software Requirements](#software-requirements)
- [Before You Upload a Sketch](#before-you-upload-a-sketch)
- [Diagnostic Sketches: Step-by-Step](#diagnostic-sketches-step-by-step)
  - [01 — I²C Scanner](#01--i²c-scanner)
  - [02 — ADXL345 Diagnostic and Serial Plotter](#02--adxl345-diagnostic-and-serial-plotter)
  - [03 — 16×2 I²C LCD Test](#03--16x2-i2c-lcd-test)
  - [04 — RGB LED and Buzzer Test](#04--rgb-led-and-buzzer-test)
  - [05 — HX711 Raw Diagnostic](#05--hx711-raw-diagnostic)
  - [06 — Wi-Fi and HTTP Diagnostic](#06--wi-fi-and-http-diagnostic)
  - [07 — HX711 Calibration Factor](#07--hx711-calibration-factor)
- [Recommended Test Order](#recommended-test-order)
- [Troubleshooting](#troubleshooting)
- [Safety and Limitations](#safety-and-limitations)
- [Suggested Repository Layout](#suggested-repository-layout)
- [Project Context](#project-context)

---

## What Is Included

| File | What It Checks | Main Result |
|---|---|---|
| `01_I2C_Scanner.ino` | I²C bus communication | Lists I²C addresses that acknowledge the ESP32 |
| `02_ADXL345_Diagnostic.ino` | ADXL345 accelerometer | Checks the device ID and reports X/Y/Z acceleration; supports a live graph |
| `03_LCD_I2C_Test.ino` | 16×2 I²C LCD | Checks initialization, backlight, text, and repeated screen updates |
| `04_RGB_Buzzer_Test.ino` | RGB LED and buzzer driver | Tests LED channels and buzzer control independently |
| `05_HX711_Raw_Diagnostic.ino` | HX711 load-cell ADC interface | Checks readiness and reports raw ADC counts without applying calibration |
| `06_WiFi_HTTP_Diagnostic.ino` | Wi-Fi, mDNS, and HTTP | Checks network connection and serves a small diagnostic web page |
| `07_HX711_Calibration_Factor.ino` | HX711 calibration | Calculates counts-per-gram from a known mass and verifies readings |

Each `.ino` file is a **separate Arduino sketch**. Upload and test them individually; do not combine all seven files into one Arduino sketch.

## Hardware and Wiring Reference

The sketches target an **ESP32 DevKit using the ESP32-WROOM-32 module**. Pin assignments below match the wiring documented in these sketches. If your physical wiring differs, update the relevant pin constants in the sketch before uploading.

### Pin Map

| Module / Signal | ESP32 Connection | Notes |
|---|---:|---|
| I²C SDA — LCD backpack | GPIO 21 | Shared I²C data line |
| I²C SCL — LCD backpack | GPIO 22 | Shared I²C clock line |
| I²C SDA — ADXL345 | GPIO 21 | Shares the I²C bus with the LCD |
| I²C SCL — ADXL345 | GPIO 22 | Shares the I²C bus with the LCD |
| RGB LED — Red | GPIO 14 | Active LOW; one 180 Ω series resistor |
| RGB LED — Green | GPIO 26 | Active LOW; one 180 Ω series resistor |
| RGB LED — Blue | GPIO 27 | Active LOW; one 180 Ω series resistor |
| Buzzer driver control | GPIO 25 | Drives a BC547 transistor through a 1 kΩ base resistor |
| HX711 DOUT / DT | GPIO 33 | Data-ready/data output from HX711 |
| HX711 SCK / CLK | GPIO 32 | Clock input to HX711 |

**All modules must share a common ground with the ESP32.** Check the power requirements of each breakout board before connecting it. Do not assume that every module is safe to power from 3.3 V or that every module's I²C pull-ups are compatible with the ESP32.

### Expected I²C Addresses

| Device | Common / Expected Address | Details |
|---|---:|---|
| 16×2 LCD I²C backpack | `0x27` | Some backpacks use a different address; use the scanner result |
| ADXL345 | `0x53` | Common when the address-selection pin is LOW |
| ADXL345 alternate | `0x1D` | Possible when the address-selection pin is HIGH |

The I²C scanner only confirms that a device acknowledges an address. It **does not** prove that the device is configured correctly or that all its functions work.

### RGB LED and Buzzer Wiring Assumptions

The RGB test sketch assumes a **common-anode RGB LED**:

- Connect the common anode to **3.3 V**.
- Connect each colour channel to its own **180 Ω series resistor**, then to the specified ESP32 GPIO.
- The code uses active-LOW control: a colour is turned on by driving its GPIO LOW.
- If your LED is common-cathode, this logic and wiring will need to be changed.

The buzzer test assumes a transistor driver:

- Buzzer positive terminal → **5 V**.
- Buzzer negative terminal → BC547 collector.
- BC547 emitter → GND.
- GPIO 25 → **1 kΩ resistor** → BC547 base.
- A **10 kΩ base-to-emitter pull-down** is recommended to keep the transistor off during startup.
- ESP32 GND, transistor emitter GND, and the buzzer supply ground must be common.

**Never connect a 5 V buzzer directly to an ESP32 GPIO.** The GPIO controls the transistor; it does not supply the buzzer's power.

### HX711 and Load Cell

Connect the HX711 module to the ESP32 using DOUT on GPIO 33 and SCK on GPIO 32. Connect the load cell to the HX711 terminals according to the load cell's wiring documentation and the HX711 board labels (`E+`, `E-`, `A+`, and `A-` for a common arrangement).

Load-cell wire colours are not universal. Do not rely on colour alone when identifying excitation and signal wires. This diagnostic checks raw counts and readiness; it does not determine a correct calibration factor or convert counts into kilograms.

---

## Software Requirements

### Required Development Environment

1. Install the **Arduino IDE**.
2. Install Espressif's **ESP32 board package** using Boards Manager.
3. Select the appropriate ESP32 DevKit / ESP32-WROOM-32 board profile.
4. Select the serial port corresponding to your ESP32.
5. Use **115200 baud** in Serial Monitor for these sketches.

The `Wire`, `WiFi`, `WebServer`, and `ESPmDNS` libraries are supplied with the ESP32 Arduino core. They generally do not need to be installed separately when the correct ESP32 board package is installed.

### Additional Libraries

Install these through **Arduino IDE → Library Manager** when testing the corresponding module:

- **`LiquidCrystal_I2C`** — required by `03_LCD_I2C_Test.ino`. Use a version compatible with the API used in the sketch (`LiquidCrystal_I2C lcd(address, columns, rows)`, `lcd.init()`, and `lcd.backlight()`).
- **`HX711` by Bogdan Necula (`bogde`)**, or a compatible library exposing the same API — required by `05_HX711_Raw_Diagnostic.ino` and `07_HX711_Calibration_Factor.ino`.

The I²C scanner and ADXL345 diagnostic use `Wire`; the RGB/buzzer sketch uses standard GPIO functions; and the Wi-Fi diagnostic uses the ESP32 Arduino core's networking libraries.

---

## Before You Upload a Sketch

Follow these steps each time:

1. **Inspect the wiring with power disconnected.** Check that SDA and SCL are not swapped, power and ground are correct, and there are no loose wires or shorts.
2. Connect the ESP32 to your computer using a suitable USB data cable.
3. Open the `.ino` file you want to test in Arduino IDE. Keep each sketch in its own folder with the matching `.ino` filename if Arduino IDE asks for that structure.
4. Under **Tools → Board**, select the correct ESP32 board profile. Under **Tools → Port**, select the ESP32's serial port.
5. Click **Verify** (compile) before uploading. If compilation fails, read the first meaningful error and check the board package and required library.
6. Click **Upload** and wait for the upload-complete message.
7. Open **Tools → Serial Monitor** and select **115200 baud**. For command-driven sketches, set the line ending to **Newline** (or an option that sends a newline).
8. Read the startup messages. Type `HELP` where supported to see the commands available in that sketch.
9. Test one module at a time and record the result before moving on.

Some ESP32 boards reset when the serial port is opened or when switching between Serial Monitor and Serial Plotter. This is normal on many development boards. If a diagnostic seems to restart, wait for its startup message before interpreting the output.

---

## Diagnostic Sketches: Step-by-Step

### 01 — I²C Scanner

**File:** `01_I2C_Scanner.ino`

**Purpose:** Scans the I²C address range and reports devices that acknowledge a transaction. Run this first when checking the LCD or ADXL345 wiring.

#### Wiring

- SDA → GPIO 21
- SCL → GPIO 22
- Connect power and ground as required by the breakout boards.
- If both the LCD and ADXL345 are connected, they can share the same SDA/SCL pair, provided their I²C addresses do not conflict.

#### How to Run It

1. Upload `01_I2C_Scanner.ino`.
2. Open Serial Monitor at **115200 baud** with **Newline** selected.
3. Read the initial scan results. The sketch scans automatically at startup.
4. Type `SCAN` and press Enter to scan again.
5. Type `INFO` for the expected pin and address notes.
6. Type `HELP` to display the command list.

#### Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `SCAN` | Scans the I²C bus again |
| `INFO` | Prints wiring and expected-address notes |

#### What Success Looks Like

- The LCD backpack commonly responds at `0x27`.
- The ADXL345 commonly responds at `0x53`, or possibly `0x1D`.
- If both devices are connected, you would normally expect two addresses.

#### If No Devices Are Found

- Confirm the selected GPIOs: SDA 21 and SCL 22.
- Check power, ground, and common ground.
- Check for loose jumper wires and swapped SDA/SCL.
- Verify the voltage and pull-up arrangement of the breakout boards.
- Try connecting one I²C device at a time to isolate the problem.

A detected address is only a first-level communication check. Continue with the device-specific diagnostic to verify functionality.

### 02 — ADXL345 Diagnostic and Serial Plotter

**File:** `02_ADXL345_Diagnostic.ino`

**Purpose:** Checks for an ADXL345 accelerometer, verifies its device ID, configures it, and reports three-axis acceleration. It supports readable text output and a live Serial Plotter stream.

#### Wiring

- SDA → GPIO 21
- SCL → GPIO 22
- Connect the breakout's power and GND according to its specifications.
- The LCD may remain connected to the same I²C bus if its address does not conflict.

#### Sensor Configuration Used by the Sketch

- Device ID register `0x00` should return `0xE5`.
- It tries address `0x53` first, then `0x1D`.
- Full-resolution mode, ±2 g range.
- Output data rate configured to 200 Hz.
- Approximate conversion scale used by the sketch: `0.0039 g/LSB`.

#### Choose Text Mode or Graph Mode

At the top of the file, find:

```cpp
static const bool START_IN_PLOT_MODE = true;
```

- Set it to `false` to start in **text mode**, then upload and open Serial Monitor.
- Set it to `true` to start in **plot mode**, then upload and open **Tools → Serial Plotter** at 115200 baud.

The supplied sketch currently sets this option to `true`, so it starts in plot mode unless you change that line. Compile-time selection is the most reliable way to choose the initial mode because opening a serial tool can reset some ESP32 boards. Serial Monitor and Serial Plotter cannot normally use the same serial port simultaneously.

#### Text Mode Commands

| Command | Action |
|---|---|
| `HELP` | Prints commands and mode guidance |
| `READ` | Prints one sample immediately |
| `INFO` | Prints detected address, pins, data rate, and scale |
| `RATE 1` | Prints a text sample every 1 second |
| `RATE 5` | Prints a text sample every 5 seconds |
| `RATE 10` | Prints a text sample every 10 seconds |
| `MODE TEXT` | Selects text mode until reset |
| `MODE PLOT` | Selects plot mode until reset |

For command input, use Serial Monitor at **115200 baud** with **Newline** selected. In plot mode, command text is intentionally ignored to avoid contaminating the graph stream; choose the mode before opening the Plotter.

#### Understanding Text Output

A text sample includes:

- `RAW X`, `RAW Y`, `RAW Z`: signed raw sensor counts.
- `g X`, `g Y`, `g Z`: approximate acceleration along each axis in units of *g*.
- `MAG`: vector magnitude, calculated as \(\sqrt{X^2 + Y^2 + Z^2}\), using the converted axis values.

With the sensor stationary and its Z axis vertical, one axis will commonly be near ±1 g while the other two are closer to 0 g. The exact readings depend on orientation, mounting, sensor offset, and noise. The magnitude of gravity is commonly near 1 g when stationary, but this is a sanity check, not a precision calibration.

#### Understanding Serial Plotter Output

The graph stream contains labelled values:

```text
X:0.012,Y:-0.020,Z:0.998,MAG:0.999
```

The plotter should show four series: `X`, `Y`, `Z`, and `MAG`. Values are expressed in *g*. The sketch streams at approximately 50 lines per second for plotting; the sensor itself is configured for a 200 Hz output data rate.

If a sensor read fails in plot mode, the sketch emits `nan` values so it does not insert diagnostic prose into the graph stream.

#### If the Sensor Is Not Detected

1. Run `01_I2C_Scanner.ino` first.
2. Confirm that the scanner sees `0x53` or `0x1D`.
3. Recheck SDA, SCL, power, and GND.
4. Check whether the breakout's address-selection pin changes its I²C address.
5. Confirm the breakout is actually an ADXL345-compatible module.

This sketch tests communication and basic readings; it is not a calibrated vibration-analysis instrument.

### 03 — 16×2 I²C LCD Test

**File:** `03_LCD_I2C_Test.ino`

**Purpose:** Verifies LCD initialization, backlight control, text rendering, and periodic screen updates.

#### Wiring

- SDA → GPIO 21
- SCL → GPIO 22
- LCD backpack address in the sketch: `0x27`

If the I²C scanner finds a different address, update `LCD_ADDR` in the sketch to match it. The sketch uses the `LiquidCrystal_I2C` library.

#### How to Run It

1. Install a compatible `LiquidCrystal_I2C` library.
2. Confirm the backpack's address using the I²C scanner.
3. Update `LCD_ADDR` if necessary.
4. Upload the sketch.
5. Open Serial Monitor at **115200 baud** with **Newline** selected.
6. The sketch should write a demonstration message and then update the second row with a counter.

#### Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `CLEAR` | Clears the LCD |
| `BACKLIGHT ON` | Turns the backlight on |
| `BACKLIGHT OFF` | Turns the backlight off |
| `DEMO` | Writes the demonstration message |

The counter updates automatically, so the second row will subsequently show a count.

#### If the Backlight Is On but Text Is Invisible

- Adjust the small contrast potentiometer on the LCD backpack slowly.
- Confirm the I²C address is correct.
- Run the scanner and confirm the backpack responds.
- Check that the library installed is compatible with the sketch's API.
- Check SDA/SCL and the module's power requirements.

A lit backlight does not necessarily mean I²C communication or text rendering is working.

### 04 — RGB LED and Buzzer Test

**File:** `04_RGB_Buzzer_Test.ino`

**Purpose:** Tests each RGB LED channel and the buzzer output independently. It does not run the main project's alert classification or alert timing logic.

#### Pin Assignment

- Red: GPIO 14
- Green: GPIO 26
- Blue: GPIO 27
- Buzzer driver: GPIO 25

This sketch assumes a common-anode RGB LED and a transistor-driven 5 V buzzer, as described in the wiring reference above.

#### How to Run It

1. Check the LED type and verify that each colour has its own 180 Ω resistor.
2. Check the transistor wiring and base resistor before connecting the buzzer supply.
3. Upload the sketch.
4. Open Serial Monitor at **115200 baud** with **Newline** selected.
5. Type `HELP` to display commands.
6. Test one colour at a time, turn the channels off, then test the buzzer.
7. Use `TEST` to run the short automatic sequence.

#### Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `R` | Turns on red only |
| `G` | Turns on green only |
| `B` | Turns on blue only |
| `RGB` | Turns on all three channels |
| `OFF` | Turns off all LED channels and the buzzer |
| `BUZZER ON` | Activates the buzzer output |
| `BUZZER OFF` | Deactivates the buzzer output |
| `TEST` | Runs a short red → green → blue → all-colours → buzzer sequence |

The RGB LED is controlled active LOW. If the colours behave inverted or do not match the commands, check whether the LED is common-anode or common-cathode and verify the channel pinout.

If the buzzer does not sound, check the transistor pinout against the actual BC547 part/package, the 5 V supply, common ground, base resistor, and buzzer polarity. Transistor lead order can vary by package or manufacturer; do not assume the pinout from physical orientation alone.

### 05 — HX711 Raw Diagnostic

**File:** `05_HX711_Raw_Diagnostic.ino`

**Purpose:** Checks whether the HX711 is returning data and prints raw ADC counts. This is an initial communication and stability test, not a weighing calibration program.

#### Wiring

- HX711 DOUT / DT → GPIO 33
- HX711 SCK / CLK → GPIO 32
- HX711 GND → ESP32 GND
- Connect the load cell to the HX711 according to the board labels and the load cell's own wiring documentation.

Install the `HX711` library by Bogdan Necula (`bogde`) or a compatible library exposing the same API.

#### How to Run It

1. Check the HX711 power, ground, DOUT, and SCK wiring.
2. Install the library.
3. Upload the sketch.
4. Open Serial Monitor at **115200 baud** with **Newline** selected.
5. Read the initial readiness result.
6. Type `RAW` to request a reading.
7. Type `AVG 10` or `AVG 25` to average a chosen number of readings.
8. Apply and remove a small, safe load to see whether the raw counts respond.

#### Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `RAW` | Reads one raw ADC value if the HX711 is ready |
| `AVG n` | Averages `n` raw readings; valid range is 1–100 |
| `TARE` | Reads a 10-sample average as an informational reference |
| `INFO` | Prints DOUT/SCK pins and current readiness |

#### Important Interpretation Notes

- The sketch reports raw counts, not kilograms or grams.
- `TARE` prints a reference value for inspection; it does not apply a persistent tare offset.
- No calibration factor is used.
- Raw counts may be negative or positive depending on signal wiring, load direction, and ADC polarity.
- A reading that changes when load is applied is a useful first sign, but does not prove the mechanical setup or calibration is correct.
- Noise, drift, unstable mounting, wiring strain, electrical interference, and power quality can all affect readings.

#### If the HX711 Reports `NOT READY`

- Confirm DOUT is connected to GPIO 33 and SCK to GPIO 32.
- Verify HX711 power and common ground.
- Check solder joints and breadboard/jumper connections.
- Check that the load cell is connected to the correct HX711 terminals.
- Try a shorter, tidier signal-wire arrangement and keep it away from noisy power wiring.
- Confirm the expected HX711 library is installed.

Do not use this diagnostic as a calibrated weighing instrument or as evidence that a final load threshold is accurate. After raw readings are stable, use `07_HX711_Calibration_Factor.ino` to calculate a factor from a known mass.

### 06 — Wi-Fi and HTTP Diagnostic

**File:** `06_WiFi_HTTP_Diagnostic.ino`

**Purpose:** Checks Wi-Fi association, IP address assignment, signal strength reporting, mDNS startup, and a small HTTP server. It is separate from the main monitoring dashboard.

#### Before Uploading: Configure Credentials

Open the file and replace these placeholders:

```cpp
static const char* WIFI_SSID = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Use the SSID and password of a Wi-Fi network the ESP32 is allowed to join. The ESP32-WROOM-32 supports 2.4 GHz Wi-Fi; a network available only on 5 GHz will not work with this board.

**Credential hygiene:** Do not commit real Wi-Fi passwords to a public repository. For public GitHub use, keep placeholders in the committed example and enter your own credentials locally. If you change the sketch to use a separate secrets file, ensure that file is excluded from Git and document the required local setup.

#### How to Run It

1. Replace the placeholders locally.
2. Upload the sketch.
3. Open Serial Monitor at **115200 baud**.
4. Wait for the connection attempt to finish.
5. If connection succeeds, note the printed IP address and mDNS result.
6. On a device connected to the same local network, open the printed IP address in a browser.
7. You can also try `http://esp32-diagnostic.local` if mDNS works on your network and client device.
8. Open `/status` on the same host to inspect the JSON status response.

#### HTTP Endpoints

| Endpoint | Expected Response |
|---|---|
| `/` | Small HTML page showing a diagnostic success message, IP address, and RSSI |
| `/status` | JSON containing connection status, IP address, RSSI, and mDNS status |

For example, if the ESP32 prints `192.168.1.50`, open `http://192.168.1.50/` and `http://192.168.1.50/status`. Use the actual IP address printed by your board; the example address is illustrative only.

#### Serial Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `STATUS` | Prints Wi-Fi connection state, IP, RSSI, and mDNS status |
| `RECONNECT` | Attempts to reconnect using the configured credentials |

The server uses HTTP port 80. The diagnostic server is intended for a trusted local test network; it does not implement authentication or HTTPS. Do not expose it directly to the public internet.

#### If Wi-Fi Does Not Connect

- Recheck the SSID and password, including capitalization.
- Confirm the network provides 2.4 GHz Wi-Fi.
- Check whether the router requires a captive portal or enterprise authentication; this simple sketch may not support those setups.
- Move the board closer to the access point.
- Use the printed IP address if mDNS resolution fails.
- Confirm the computer or phone used for testing is on the same network and is not isolated by guest-network or client-isolation settings.

---

### 07 — HX711 Calibration Factor

**File:** `07_HX711_Calibration_Factor.ino`

**Purpose:** Calculates a calibration factor for the load cell and HX711 using a known reference mass. Unlike the raw diagnostic, this sketch is intended to establish the counts-per-gram factor needed to convert raw ADC changes into mass units in a later integrated firmware implementation.

#### Wiring

- HX711 DOUT / DT → GPIO 33
- HX711 SCK / CLK → GPIO 32
- HX711 GND → ESP32 GND
- Connect the load cell to the HX711 according to the module's terminal labels and the load cell documentation.
- If the LCD is connected, it uses SDA GPIO 21 and SCL GPIO 22.

#### Calibration Procedure

1. Ensure the load cell is mounted securely and the platform is not touching anything except the intended load-cell mounting points.
2. Upload `07_HX711_Calibration_Factor.ino`.
3. Open Serial Monitor at **115200 baud** and select **Newline**.
4. Remove all load from the platform and enter `TARE`. The sketch takes a set of readings to establish a zero reference.
5. Place a known mass on the platform. Use the mass in grams; for example, a 1 kg reference mass is entered as `CAL 1000`.
6. Record the reported calibration factor, including its sign.
7. Remove the mass and check the zero reading. Reapply the known mass and verify that the measured result is close to the reference value.
8. Repeat the procedure if readings drift, fluctuate substantially, or do not return near zero.

#### Commands

| Command | Action |
|---|---|
| `HELP` | Prints available commands |
| `INFO` | Prints pin assignments and calibration guidance |
| `RAW` | Prints a raw reading when the HX711 is ready |
| `TARE` | Establishes the unloaded reference from averaged readings |
| `CAL <grams>` | Calculates the calibration factor using the currently applied known mass in grams |
| `WEIGH` | Reports the current mass using the factor calculated during this session |
| `CLEAR` | Clears the current calibration factor/reference state, if supported by the sketch |

Use the exact commands printed by the sketch's `HELP` output if the installed version differs.

#### Calibration Formula

The factor is calculated from the change between loaded and unloaded raw readings:

\[
\text{Calibration factor} =
\frac{\text{loaded raw average} - \text{tare raw average}}
{\text{known mass in grams}}
\]

The unit is **raw counts per gram**. A negative factor can be valid because it depends on signal polarity and wiring. Preserve the sign exactly as reported by the sketch.

#### Important Notes

- Use a known, stable reference mass and enter its value in grams.
- The platform must be mechanically stable and must not rub against the frame or wiring.
- Keep load-cell signal wires away from noisy power wiring where possible.
- Do not assume that a calibration factor from one mounting arrangement will remain valid after mechanical changes.
- The factor is a diagnostic result; record it before resetting the ESP32. Do not assume it is permanently saved unless the sketch explicitly says so.
- Calibration accuracy depends on the load cell, HX711 module, mounting, power, noise, and reference mass.
- This sketch does not automatically integrate calibrated measurements into `BHMS.ino`.

## Recommended Test Order

Testing in this order helps separate wiring, sensor, output, and network issues:

1. **I²C scanner** — confirm that the LCD and ADXL345 respond on the bus.
2. **LCD test** — confirm the display can initialize and render text.
3. **ADXL345 diagnostic** — confirm device ID and plausible acceleration readings; use the Plotter if helpful.
4. **RGB LED and buzzer test** — verify each output and the buzzer transistor stage.
5. **HX711 raw diagnostic** — confirm readiness and observe whether raw counts respond to load changes.
6. **HX711 calibration-factor diagnostic** — once raw readings are stable, calculate and verify the factor with a known mass.
7. **Wi-Fi and HTTP diagnostic** — confirm connectivity and browser access.

Once each module has passed its isolated test, upload the main Bridge Health Monitor firmware and verify the integrated behavior again. A module passing its standalone test does not guarantee the full system will work without pin conflicts, power issues, or software integration problems.

## Troubleshooting

### The Sketch Will Not Compile

- Confirm the correct ESP32 board package is installed and selected.
- Confirm the correct `.ino` file is open.
- Install the library listed for that sketch.
- If a library error names a missing header, install the corresponding library rather than changing unrelated code.
- If a library API differs from the one used by the sketch, install a compatible version or deliberately adapt the code.

### The ESP32 Port Is Missing

- Use a USB data cable, not a charge-only cable.
- Try another USB port and cable.
- Check whether the board requires a USB-to-UART driver on your operating system.
- Close other programs that may be holding the serial port.
- Recheck the selected port after reconnecting the board.

### Serial Output Is Unreadable or Absent

- Set baud rate to **115200**.
- Select **Newline** for command-driven sketches.
- Confirm that upload completed successfully.
- Press the ESP32 reset button and watch for startup messages.
- For the ADXL345 graph, open Serial Plotter instead of Serial Monitor and ensure the sketch was compiled with `START_IN_PLOT_MODE = true`.

### I²C Device Missing

- Run the I²C scanner with one device connected at a time.
- Verify GPIO 21 (SDA), GPIO 22 (SCL), power, and ground.
- Confirm the expected address instead of assuming it.
- Check the breakout's voltage and pull-up requirements.

### A Sensor Is Detected but Its Readings Are Implausible

- Confirm the module is the expected sensor model.
- Recheck its power, bus wiring, and mounting.
- Test with the module stationary and then move it gently.
- Treat diagnostics as preliminary checks; calibration and mechanical validation are separate tasks.

---

## Safety and Limitations

- **Disconnect power before changing wiring.**
- Check every breakout's operating voltage and logic-level requirements before connecting it to the ESP32.
- ESP32 GPIOs use 3.3 V logic and are not 5 V tolerant.
- Do not drive a high-current buzzer directly from a GPIO; use the specified transistor driver or an appropriately designed driver circuit.
- Use a separate current-limiting resistor for each RGB LED channel.
- Ensure the RGB LED common-anode assumption matches the actual component.
- Keep grounds common where modules share signals or supplies.
- Do not treat raw HX711 counts as calibrated mass measurements.
- Do not treat basic ADXL345 readings as validated structural-health or vibration-safety measurements.
- The Wi-Fi diagnostic is an unauthenticated local HTTP server and should not be exposed to untrusted networks.
- The sketches have been documented for the wiring and APIs described here, but should be compiled and tested with your actual board, installed libraries, and hardware before relying on the results.

## Suggested Repository Layout

Keep the sketches and this README together, for example:

```text
bridge-monitor-diagnostics/
├── README.md
├── 01_I2C_Scanner/
│   └── 01_I2C_Scanner.ino
├── 02_ADXL345_Diagnostic/
│   └── 02_ADXL345_Diagnostic.ino
├── 03_LCD_I2C_Test/
│   └── 03_LCD_I2C_Test.ino
├── 04_RGB_Buzzer_Test/
│   └── 04_RGB_Buzzer_Test.ino
├── 05_HX711_Raw_Diagnostic/
│   └── 05_HX711_Raw_Diagnostic.ino
├── 06_WiFi_HTTP_Diagnostic/
│   └── 06_WiFi_HTTP_Diagnostic.ino
└── 07_HX711_Calibration_Factor/
    └── 07_HX711_Calibration_Factor.ino
```

Arduino IDE generally expects a sketch folder to contain a primary `.ino` file with the same name as the folder. If you prefer to keep all seven `.ino` files directly in one directory for GitHub browsing, that is fine too; open each sketch individually when using Arduino IDE.

---

## Project Context

These diagnostics support the hardware bring-up of an **IoT-Based Smart Bridge Health Monitoring System** built around an ESP32-WROOM-32. The system's sensor path includes an ADXL345 accelerometer for acceleration/vibration measurements and a load cell interfaced through the HX711 ADC for load-related measurements. A 16×2 I²C LCD, RGB LED, buzzer, and Wi-Fi connection provide local feedback and network connectivity.

This repository contains only the isolated diagnostic sketches described above. It does not, by itself, implement the complete bridge-monitoring logic, structural-health assessment, validated alarm thresholds, or production-ready safety functions.

