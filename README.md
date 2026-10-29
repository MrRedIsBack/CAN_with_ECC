# Bare-Metal CAN 2.0 Network with Custom Error-Correcting Code (ECC)

## Overview
A bare-metal implementation of the Controller Area Network (CAN 2.0B) protocol.

---

## Hardware Architecture
* **Node 1:** ESP32 
* **Node 2:** TI TM4C123GH6PM
* **Physical Layer:** 2x TI SN65HVD230 (VP230) 3.3V CAN Transceivers

### Interconnect & Pin Mapping
| Signal | TM4C123 (`CAN0`) | VP230 #1 | VP230 #2 | ESP32 (`TWAI`) | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **VCC** | `+3.3V` | `3.3V` | `3.3V` | `3V3` | Native 3.3V logic levels |
| **GND** | `GND` | `GND` | `GND` | `GND` | Common ground reference required across nodes |
| **CANTX** | `PB5` (`CAN0Tx`) | `CTX` | `CTX` | `GPIO 21` | Routed via GPIO Matrix (ESP32) / `PCTL` Mux `0x8` (TM4C) |
| **CANRX** | `PB4` (`CAN0Rx`) | `CRX` | `CRX` | `GPIO 22` | Routed via GPIO Matrix (ESP32) / `PCTL` Mux `0x8` (TM4C) |
| **CAN_H** | — | `CANH` | `CANH` | — | $120\ \Omega$ termination enabled on both endpoints ($60\ \Omega$ nominal) |
| **CAN_L** | — | `CANL` | `CANL` | — | Differential pair |

---

## Repository Structure
```text
├── ESP32/CAN/       # ESP-IDF v6.1 CMake project (TWAI direct register access)
└── TM4C123/         # Keil uVision MDK-ARM project (C_CAN bare-metal)
```

---

## Toolchain & Build Setup

### 1. ESP32 Node (VS Code + ESP-IDF v6.1)
1. Install the **ESP-IDF** extension in VS Code and run `ESP-IDF: Open ESP-IDF Installation Manager`[cite: 3].
2. Provision the toolchain via **Easy Installation** (installs Xtensa GCC, CMake, Ninja, and ESP-IDF v6.1 to the host environment)[cite: 3, 5].
3. Reload the window (`Developer: Reload Window`) and generate a blank project in `ESP32/CAN` via `ESP-IDF: New Project` (`template-app`)[cite: 4].
4. Open `ESP32/CAN` as the workspace root so CMake and `clangd` resolve `compile_commands.json`[cite: 4, 5].
5. Set target architecture to `esp32`, build the project to index register headers (`soc/twai_struct.h`), and flash via UART[cite: 5].

### 2. TM4C123 Node (Keil uVision MDK-ARM)
1. Install the **`Keil::TM4C_DFP`** device family pack via Pack Installer.
2. Create a new project in `TM4C123/` targeting **`TM4C123GH6PM`**.
3. In the Run-Time Environment (RTE) manager, include only **`CMSIS::CORE`** and **`Device::Startup`** (`startup_TM4C123.s` and `system_TM4C123.c`—no peripheral driver libraries).
4. Set the debug and flash probe to **Stellaris ICDI** under *Target Options $\rightarrow$ Debug / Utilities*.

# My Notes
## Registers
DPORT_PERIP_CLK_EN_REG = 00...

DPORT_PERIP_RST_EN_REG = 11...