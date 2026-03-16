<div align="center">

# ⚗️ SILAR IoT Telemetry Platform

### *Embedded control, telemetry logging, and cloud-ready data pipeline for a SILAR synthesis machine*

<p align="center">
  <img src="./docs/images/banner.png" alt="SILAR project banner" width="100%">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-black?style=for-the-badge&logo=espressif" />
  <img src="https://img.shields.io/badge/Firmware-PlatformIO-orange?style=for-the-badge&logo=platformio" />
  <img src="https://img.shields.io/badge/API-FastAPI-009688?style=for-the-badge&logo=fastapi" />
</p>

<p align="center">
  <img src="https://img.shields.io/github/license/bdcmartinez/silar-automation-system?style=flat-square" />
  <img src="https://img.shields.io/github/stars/bdcmartinez/silar-automation-system?style=flat-square" />
  <img src="https://img.shields.io/github/last-commit/bdcmartinez/silar-automation-system?style=flat-square" />
  <img src="https://img.shields.io/github/repo-size/bdcmartinez/silar-automation-system?style=flat-square" />
</p>

</div>

---

## ✨ Overview

This project automates a **SILAR (Successive Ionic Layer Adsorption and Reaction)** synthesis process using an **ESP32-based embedded control system** with local telemetry logging, WiFi connectivity, and cloud integration.

The platform was designed not only to control the machine, but also to create a **real sensor-to-cloud-to-warehouse pipeline**:

- ⚙️ control of stepper motors and process routines  
- 💾 local telemetry logging to SD card  
- 📶 WiFi connectivity and secure data upload  
- 🔐 authenticated API ingestion  

This repository represents the embedded and system-level foundation of a broader **IoT + Data Engineering** project.

---

## 🎯 Project Goals

- Automate a SILAR deposition workflow
- Record useful operational telemetry during machine execution
- Persist data locally for resilience
- Upload machine data to a backend for centralized storage
- Enable downstream analytics, monitoring, and future ML use cases

---

## 🖼️ Preview

<p align="center">
  <img src="./docs/images/machine-photo.jpg" alt="SILAR machine" width="48%">
  <img src="./docs/images/lcd-menu.gif" alt="LCD navigation demo" width="48%">
</p>

<p align="center">
  <em>Replace these placeholders with real photos, UI screenshots, or GIFs of the machine in action.</em>
</p>

---

## Development Environment

This project was developed using **PlatformIO**, a modern development environment for embedded systems. PlatformIO provides better project organization, dependency management, and build reproducibility compared to the traditional Arduino IDE workflow. It also integrates well with editors such as **Visual Studio Code**, making the development process more scalable and maintainable.

---

## Project Structure

PlatformIO organizes the project into a standard directory layout:

### Folder Description

**src/**  
Contains the main firmware code. The entry point of the program is typically `main.cpp`.

**include/**  
Stores header files (`.h`) used across multiple parts of the project.

**lib/**  
Contains custom libraries or reusable modules that help organize the codebase.

**test/**  
Optional directory for unit tests.

**platformio.ini**  
The main configuration file where the board, framework, and other build settings are defined.


## 🔧 Hardware Components

The automation system is built around an **ESP32 microcontroller** connected to several peripherals responsible for motion control, user interaction, and telemetry logging.  
These components enable the machine to execute automated SILAR deposition routines while collecting operational data.

| Component | Quantity | Description | Product Link |
|----------|----------|-------------|--------------|
| **ESP32 DevKit (WiFi)** | 1 | Main microcontroller responsible for motor control, telemetry logging, LCD interface management, and communication with the backend API. | https://a.co/d/057elhsU |
| **TMC2209 Stepper Driver** | 2 | High-precision stepper motor driver with microstepping support and low-noise operation used to control motor movement. | https://www.mercadolibre.com.mx/p/MLM66091021 |
| **Stepper Motor (0.7A)** | 2 | Stepper motors used to move the substrate holder between solution containers during the SILAR process. | https://es.aliexpress.com/item/4001272086575.html |
| **Mini SD Card Module** | 1 | Module used to store telemetry data locally in CSV format during machine operation. | https://es.aliexpress.com/item/1005005302035188.html |
| **Logic Level Converter** | 2 | Bidirectional voltage level converters used to safely interface components operating at different logic voltages. | https://www.mercadolibre.com.mx/p/MLM2075514440 |
| **20x4 LCD Display** | 1 | Display used to provide a local user interface for machine status, configuration menus, and runtime information. | https://es.aliexpress.com/item/1005008740245517.html |

---

## Hardware Overview

The **ESP32** coordinates the entire system by:

- controlling the **TMC2209 stepper drivers** to execute motion routines  
- managing the **LCD interface** for machine configuration and status display  
- logging telemetry data to the **SD card module**  
- handling communication with external services through **WiFi**  

This modular hardware design allows the system to be extended with additional sensors or control modules in the future.

