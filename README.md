# Silar Automation System

ESP32 firmware that automates the **SILAR** (Successive Ionic Layer Adsorption and Reaction) thin-film deposition process, driving a two-axis sample stage through programmed dip sequences with a local LCD/encoder interface, cloud telemetry, and over-the-air updates.

![firmware](https://img.shields.io/badge/firmware-2.0.3-blue)
![platform](https://img.shields.io/badge/platform-ESP32-informational)
![build](https://img.shields.io/badge/build-PlatformIO-orange)
![license](https://img.shields.io/badge/license-Apache--2.0-green)

## Overview

The machine replaces the manual dipping workflow of SILAR with a repeatable, programmable XY stage. An ESP32 drives two TMC2209 stepper drivers to move a substrate holder between precursor and rinse beakers laid out on the work surface; a 20×4 LCD and two rotary encoders let the operator build and save the dip sequence. Every run is logged to an SD card with DS3231 timestamps and batched to a cloud ingest endpoint over authenticated HTTPS. Firmware updates itself over the air from a GitHub-hosted manifest.

## The SILAR method

SILAR is a low-cost, solution-based technique for depositing thin films one atomic/ionic layer at a time. A substrate is cycled through four stations in sequence:

1. **Cation precursor** — the substrate dips into a solution containing the metal ion (e.g. `Cd²⁺`, `Zn²⁺`, `Cu²⁺`); ions adsorb onto the surface.
2. **Rinse 1** — excess, weakly bound cations are washed off in deionized water.
3. **Anion precursor** — the substrate dips into a solution containing the counter-ion (e.g. `S²⁻`, `OH⁻`, `Se²⁻`); it reacts with the adsorbed cation layer to form the target compound (CdS, ZnO, CuS, …).
4. **Rinse 2** — unreacted species are washed off.

One pass through the four beakers deposits a single layer. Film thickness is controlled by the **number of cycles**; stoichiometry and crystallinity are controlled by **dwell time**, **precursor concentration**, and **temperature**. The technique is used for semiconductor thin films (solar absorbers, buffer layers, transparent conductors, photocatalysts, gas-sensing layers) because it is cheap, scalable, and needs no vacuum.

### How this machine helps

Done by hand, SILAR is tedious and a significant source of run-to-run variability — a 100-cycle deposition is 400 manual dips timed with a stopwatch. This controller removes the human from the loop:

- **Repeatable geometry** — the XY stage drives to saved `(x, y)` beaker positions with 400 steps/mm resolution, so the substrate lands in the same place every cycle.
- **Programmable recipes** — up to 20 sequence steps per recipe with independent per-step `(MINUTOS, SEGUNDOS)` dwell times (`src/main.cpp:38-41`), and multiple recipes stored on the SD card (`CAMBIAR MODO` / `MODO NUEVO`).
- **Deterministic timing** — dwell times are driven by the RTC rather than operator stopwatching, so adsorption/reaction windows are consistent across hundreds of cycles.
- **Traceability** — each run is timestamped and logged; `ENVIAR INFO` batches the records to the cloud so process parameters can be correlated with downstream film characterization (XRD, UV-Vis, SEM).
- **Unattended operation** — a long deposition (e.g. 150 cycles × 4 stations × ~30 s dwell ≈ 5 hours) runs without supervision, and `ACTUALIZAR COD` keeps the firmware current via OTA.

In short: the mechanical rig provides the motion, this firmware provides the recipe engine, the logging, and the network plumbing that turn SILAR from a manual craft into a reproducible deposition process.

## Features

- Dual-axis stepper control — 400 steps/mm, 8× microstepping, TMC2209 drivers configured over UART
- Rotary-encoder + 20×4 LCD menu: run a stored process, switch/create motion patterns, upload data, change WiFi, trigger OTA
- SD-card data logging with DS3231 RTC timestamps
- WiFi telemetry — JWT login then batched JSON POST to an ingest endpoint
- OTA firmware updates driven by a `version.json` manifest and GitHub releases

## Hardware

**Bill of materials**

- ESP32 dev board
- 2× NEMA stepper motors (≈0.7 A) with 2× TMC2209 drivers
- 20×4 I²C character LCD (HD44780-compatible, PCF8574 backpack @ `0x27`)
- 2× rotary encoders with pushbutton
- DS3231 RTC module
- SD card module (SPI)

## Pin Map

```
ESP32          Peripheral
─────────────────────────────
GPIO 5    →   SD Card CS
GPIO 12   →   Stepper X DIR
GPIO 27   →   Stepper X PUL
GPIO 25   →   Stepper Y DIR
GPIO 26   →   Stepper Y PUL
GPIO 16   →   TMC2209 RX (UART2)
GPIO 17   →   TMC2209 TX (UART2)
GPIO 33   →   Encoder A CLK
GPIO 32   →   Encoder A DT
GPIO 39   →   Encoder A Button
GPIO 35   →   Encoder B CLK
GPIO 34   →   Encoder B DT
GPIO 36   →   Encoder B Button
SDA/SCL   →   LCD + RTC (I²C bus)
```



## System Architecture

```
┌───────────────────────────────────────────────────┐
│                    ESP32 Firmware                 │
│                                                   │
│  ┌─────────┐   ┌──────────┐   ┌───────────────┐  │
│  │  LCD UI │◄──│ Encoder  │   │  TimeManager  │  │
│  │  Menu   │   │  Input   │   │  (DS3231 RTC) │  │
│  └────┬────┘   └──────────┘   └───────┬───────┘  │
│       │                               │           │
│  ┌────▼────────────────────┐          │           │
│  │     Motion Controller   │          │           │
│  │  (MotorMovement class)  │◄─────────┘           │
│  │   X axis   │   Y axis   │                      │
│  └──────┬─────┴─────┬──────┘                      │
│         │           │                             │
│    TMC2209 X    TMC2209 Y  (UART)                 │
│                                                   │
│  ┌──────────────────────────────────────────┐     │
│  │             SaveSensorData               │     │
│  │  Logs driver telemetry → SD CSV files    │     │
│  │  Batch uploads CSV → REST API (HTTPS)    │     │
│  └──────────────────────────────────────────┘     │
│                                                   │
│  ┌──────────────┐   ┌──────────────────────────┐  │
│  │  OTA Update  │   │   Wi-Fi / APIEndPoint    │  │
│  │  (HTTPUpdate)│   │   JWT Auth + HTTP POST   │  │
│  └──────────────┘   └──────────────────────────┘  │
└───────────────────────────────────────────────────┘
```

## Repository layout

```
.
├── src/main.cpp         # entire firmware (monolithic)
├── include/             # project headers (secrets.h lives here, gitignored)
├── lib/                 # PlatformIO private library slot
├── ota/version.json     # OTA manifest (version + firmware URL)
├── test/                # PlatformIO test slot (no tests yet)
├── platformio.ini       # build configuration
├── firmware.bin         # last built artifact
└── LICENSE              # Apache-2.0
```

## Getting started

### Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) or VS Code with the PlatformIO IDE extension
- USB-serial driver for your ESP32 board (CP210x / CH340 depending on the board)

### Clone

```bash
git clone https://github.com/bdcmartinez/silar-automation-system.git
cd silar-automation-system
```

### Configure secrets

`include/secrets.h` is gitignored. Copy the template and fill it in:

```bash
cp include/secrets.h.example include/secrets.h
```

The firmware expects these symbols (referenced in `src/main.cpp:411`, `573-577`, `2308`):

```cpp
#define LOGIN_ENDPOINT     "https://<your-host>/auth/login"
#define EMAIL              "device@example.com"
#define PASSWORD           "..."
#define INGEST_BATCH_URL   "https://<your-host>/ingest/batch"
#define OTA_MANIFEST_URL   "https://<your-host>/ota/version.json"
```

WiFi SSID and password are **not** compiled in — they are entered on the device via the `MODIFICAR RED` menu and persisted to SD card under `/credentials`.

### Build, flash, monitor

```bash
pio run                    # compile
pio run -t upload          # flash over USB
pio device monitor -b 115200
```

## Operation

The operator interface is driven by the rotary encoders, with feedback on the LCD. Menu strings are Spanish.

**Main menu** (`src/main.cpp:2563-2624`)

- `CORRER PROCESO` — execute the stored movement sequence
- `CONFIGURACION` — configuration submenu:
  - `CAMBIAR MODO` — switch between saved motion patterns
  - `MODO NUEVO` — record a new motion pattern
  - `ENVIAR INFO` — upload logged data to the cloud
  - `MODIFICAR RED` — change WiFi network / credentials
  - `ACTUALIZAR COD` — trigger OTA firmware update

## OTA updates

The firmware periodically fetches the manifest at `OTA_MANIFEST_URL` (handled in `src/main.cpp:2307-2390` via `HTTPUpdate`). The manifest format is:

```json
{
  "version": "2.0.3",
  "url": "https://github.com/bdcmartinez/silar-automation-system/releases/download/v2.0.3/firmware.bin"
}
```

**Release process**

1. Bump `FIRMWARE_VERSION` in `src/main.cpp:23`.
2. `pio run` to produce `.pio/build/esp32dev/firmware.bin`.
3. Create a matching GitHub release tag (e.g. `v2.0.4`) and attach `firmware.bin`.
4. Update `ota/version.json` to point at the new tag and commit.

## Cloud API

- `POST LOGIN_ENDPOINT` with JSON `{"email": "...", "password": "..."}` → response includes `access_token` (`src/main.cpp:446-495`).
- `POST INGEST_BATCH_URL` with header `Authorization: Bearer <token>`, `Content-Type: application/json`, 20 s timeout (`src/main.cpp:400-420`).

> **Security note:** the firmware currently calls `WiFiClientSecure::setInsecure()` (`src/main.cpp:447-448`), so TLS certificates are **not** validated. Pin a root CA before deploying to untrusted networks.

## Troubleshooting

- **WiFi won't connect** — re-enter credentials via `MODIFICAR RED`; the device reads them from `/credentials` on the SD card (`src/main.cpp:261-262`).
- **TMC2209 silent / motors not stepping** — RX/TX of Serial2 are easy to swap (RX 16, TX 17); also verify driver UART addresses `0` and `1` are strapped correctly.
- **Motors stall** — raise the driver current from the default 500 mA toward 600–700 mA; motors are nominally 0.7 A (see `src/main.cpp:2499`).
- **SD card init fails** — confirm CS on pin 5, FAT32 format, and that the card is seated before power-on.
- **OTA reports no update** — the manifest `version` must be lexically greater than `FIRMWARE_VERSION` in the running firmware.

## Roadmap / known limitations

- Replace `WiFiClientSecure::setInsecure()` with a pinned CA.
- No automated tests under `/test`.
- `src/main.cpp` is ~3000 lines — candidate for modular decomposition.

## License

Licensed under the Apache License, Version 2.0 — see [`LICENSE`](LICENSE).
