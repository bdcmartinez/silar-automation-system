# Silar Automation System

ESP32 firmware for a motorized XY positioning platform with a local LCD/encoder interface, cloud telemetry, and over-the-air updates.

![firmware](https://img.shields.io/badge/firmware-2.0.3-blue)
![platform](https://img.shields.io/badge/platform-ESP32-informational)
![build](https://img.shields.io/badge/build-PlatformIO-orange)
![license](https://img.shields.io/badge/license-Apache--2.0-green)

## Overview

Silar drives a two-axis sample stage built around an ESP32, two TMC2209 stepper drivers, a 20×4 LCD, and two rotary encoders. The operator programs movement sequences through the on-device Spanish-language menu; the controller logs runs to an SD card with RTC timestamps, batches the data, and uploads it to a cloud ingest endpoint over authenticated HTTPS. Firmware updates itself over the air from a GitHub-hosted manifest.

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

**Pinout** (see `src/main.cpp:25-78`)

| Function       | Pin(s)                   |
|----------------|--------------------------|
| LCD I²C        | SDA 21, SCL 22           |
| Encoder A      | CLK 33, DT 32, BTN 39    |
| Encoder B      | CLK 35, DT 34, BTN 36    |
| Motor X        | DIR 12, PUL 27           |
| Motor Y        | DIR 25, PUL 26           |
| TMC2209 UART   | RX 16, TX 17 (Serial2)   |
| SD card CS     | 5 (SPI)                  |
| Onboard LED    | 2                        |

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
