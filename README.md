# Silar Automation System

This project is a PlatformIO/Arduino firmware for an ESP32-based automation controller. The core application lives in `src/main.cpp` and combines a 20x4 I2C LCD, two rotary encoders, an RTC, an SD card, TMC2209 stepper drivers, and Wi-Fi/API connectivity into one interactive machine controller.

## What `src/main.cpp` does

`src/main.cpp` is effectively the whole application. It contains:

- **Hardware setup** for the LCD, RTC, SD card, encoder pins, stepper pins, and UART-connected TMC2209 drivers.
- **Persistent storage logic** for:
  - saved movement modes under `/modes`
  - current XY position in `/pos.txt`
  - Wi-Fi credentials under `/credentials`
  - telemetry/log files under `/iot/daily` and `/iot/saved`
- **A menu-driven LCD UI** controlled by two rotary encoders and their push buttons.
- **Motor control logic** that converts selected positions into pulses for the X and Y stepper motors.
- **Routine execution logic** that replays saved mode files layer by layer, waiting the configured minutes/seconds at each step.
- **Connectivity features** for logging into an API, uploading stored batches, and switching between saved Wi-Fi networks.
- **Telemetry collection** from the TMC2209 drivers while the process is running.

## High-level architecture

The file is organized around a set of classes, each handling one subsystem.

### 1. Global hardware/configuration section
At the top of the file, the code defines:

- LCD and RTC objects
- SD card chip-select pin
- arrays for movement routine steps (`X`, `Y`, `MINUTOS`, `SEGUNDOS`)
- encoder pin assignments
- stepper driver and pulse/dir pins
- movement scaling constants such as `FACTOR_MOVEMENT`, `GENERAL_STEP_X`, and `GENERAL_STEP_Y`
- Wi-Fi/session/global state variables

These globals are shared across the menu system, file manager, motor movement code, and API upload logic.

### 2. `APIEndPoint`
This class manages network-related operations:

- scanning Wi-Fi networks
- connecting to a selected SSID
- loading/saving Wi-Fi credentials from the SD card
- authenticating against the backend API
- sending JSON batches with a bearer token

In practice, it is the bridge between the ESP32 and the remote backend.

### 3. `SaveSensorData`
This class handles SD-based telemetry logging and upload flows:

- ensures SD storage is available
- creates timestamped files in `/iot/daily`
- writes rows of telemetry data
- uploads files in batches to the API
- moves successfully uploaded files into `/iot/saved`

It is used while the machine is running so driver diagnostics can be captured over time.

### 4. `TimeManager`
This wraps RTC access and provides convenience checks such as:

- whether the RTC is connected
- whether a given number of minutes has elapsed
- whether a specific hour/minute has been reached once per day

### 5. `Files`
This class manages movement-mode files and saved positions:

- reads the last selected mode
- lists available mode files from `/modes`
- parses a mode file into the global step arrays
- stores the current XY motor position in `/pos.txt`
- saves a newly recorded mode with a timestamp-based filename

A saved mode file is effectively a script of motion steps and wait times.

### 6. `Encoder`
This class centralizes the state for the rotary encoders and push buttons:

- tracks current positions used by menus
- converts encoder turns into jog steps for X/Y axes
- debounces button presses
- exposes helper getters/setters for other classes

The global interrupt wrappers at the bottom of the file delegate to this class.

### 7. `MotorMovement`
This class is responsible for actual machine movement:

- generates pulses for each stepper motor
- changes direction pins as needed
- moves from the current persisted XY position to a target XY position
- optionally logs TMC2209 telemetry while a process is executing

### 8. LCD navigation classes
The UI is implemented with a family of LCD classes:

- `ILCDBaseNavigation`: shared navigation behavior
- `LCDLineRefresh`: generic paginated list/menu screen
- `LCDInitialMenu`: main/home menu with the RTC clock
- `LCDRunMode`: asks how many layers to execute
- `LCDRefreshRunMode`: shows runtime status during execution
- `LCDNewModeSteps`: records a new movement step
- `LCDNewModeTime`: records the dwell time for that step
- `LCDsetPassword`: allows entering Wi-Fi passwords character by character

Together these classes build the entire user interface shown on the 20x4 LCD.

## Main runtime flow

### `setup()`
The `setup()` function performs one-time initialization:

1. starts serial output
2. initializes the LCD
3. configures encoder and motor GPIO pins
4. attaches interrupts for encoder turns and button presses
5. verifies RTC and SD card availability
6. initializes SD/RTC helper classes
7. prints SD directory contents for debugging
8. initializes the TMC2209 drivers over UART and prints their status

If the RTC or SD card is missing, startup halts in an infinite loop.

### `loop()`
The `loop()` function is a large menu-driven state machine.

From the main menu, the user can:

- **Run steps**
  - choose how many layers to execute
  - load the selected mode from SD
  - move through each recorded XY step
  - wait the configured time per step
  - log driver telemetry during the run

- **Configuration**
  - change the active saved mode
  - create a new mode by jogging both axes and saving step timing
  - upload pending telemetry files
  - switch or add Wi-Fi credentials

## How a saved mode works

A saved mode file in `/modes` contains lines with four values:

- X position
- Y position
- minutes to wait
- seconds to wait

When a mode is loaded, those values are parsed into arrays. During execution, the firmware converts X/Y positions into step counts using the configured `GENERAL_STEP_X` and `GENERAL_STEP_Y` values, moves the motors to each target, and then waits for the requested time on the LCD run screen.

## Logging and uploads

While a process is running, the code can collect a CSV-like telemetry row from each driver, including fields such as:

- timestamp
- driver tag
- UART communication counters
- driver status registers
- current scale
- microstep configuration
- thermal/short/open-load flags

These rows are written to SD card files and can later be uploaded in API batches.

## Why comments were added in `src/main.cpp`

Inline comments were added to `src/main.cpp` to make the file easier to navigate without changing behavior. The new comments explain:

- the purpose of major classes
- the role of key helper methods
- the overall responsibilities of `setup()` and `loop()`
- how telemetry and motor movement functions fit together

## Project structure

- `src/main.cpp` — full firmware implementation
- `platformio.ini` — PlatformIO project configuration
- `include/`, `lib/`, `test/` — standard PlatformIO folders currently containing placeholder READMEs

## Notes for future maintenance

Because most of the application lives in one file, the easiest future refactor would be to split `src/main.cpp` into modules such as:

- `ui/` or LCD menu classes
- `storage/` for SD and mode handling
- `network/` for Wi-Fi/API code
- `motion/` for motor control and driver telemetry
- `time/` for RTC helpers

That would make the firmware easier to test and maintain, but the current code is still understandable with the added comments and this README.
