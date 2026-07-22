# Environmental Monitoring System

An STM32-based environmental monitoring system that samples ambient light and internal chip temperature, displays real-time status on an LCD, and streams data over UART to a Python host application for logging, storage, and (in progress) visualization.

This project follows a full V-model workflow — requirements → design → implementation → test — on both the firmware and host sides, and is built as a portfolio project demonstrating embedded firmware development, host/firmware integration, and validation/test engineering practices.

## Overview

```
[Light Sensor]  ─┐
                 ├─→ [STM32 ADC] ─→ [RTC] ─→ [LCD Display]
[Internal Temp] ─┘         │
                           └─→ [UART] ─→ [Python Host: Parse + Store + (GUI, in progress)]
```

- **Firmware** (`firmware/`): STM32F407-based, HAL library, CMake + VS Code toolchain. Samples light intensity and internal temperature once per second, displays them alongside RTC time on a 4.3" TFT LCD, and transmits a plain-text data frame over UART.
- **Host** (`host/`): Python application that reads the UART stream (or a hardware-independent simulator), parses it, and persists readings to a local SQLite database.

## Hardware

| Item | Detail |
|------|--------|
| MCU / Dev Board | ATK 探索者 (Explorer) STM32F407ZGT6 (正点原子) |
| LCD | ATK-MD0430, 4.3", NT35510 driver, FSMC 16-bit parallel interface |
| Light Sensor | Onboard photoresistor, ADC3 |
| Temperature Source | Internal STM32 temperature sensor, ADC1 (no external sensor) |
| Debug Console | usmart (dynamic function-call-over-UART), reused from prior projects |
| UART | USART1, 115200 baud, 8N1 |

## Features

- **1 Hz acquisition** of light intensity (0–100%) and internal temperature (0.1°C resolution)
- **Out-of-range clamping**: readings outside valid bounds are clamped and flagged with an `_ERR` suffix in the UART frame, rather than silently discarded or crashing the pipeline
- **RTC time management**: settable at runtime via usmart (`set_rtc_time()`), with a sentinel default time (`2000-01-01 00:00:00`) distinguishing "never set" from a real timestamp, and persistence across reset/reflash as long as the backup-domain supply (VBAT) is powered
- **Custom large-font LCD rendering**: a 72pt/48pt glyph set (generated externally, since the driver's built-in font tops out at size 24) for a clearly legible display of time, temperature, and light
- **Anti-flicker display**: partial/delta redraw for progress bars and large-font fields, avoiding full-screen clears at 1 Hz
- **Watchdog-protected main loop**: independent watchdog (IWDG) fed every loop iteration; usmart command handling runs from the main loop rather than an interrupt context, avoiding a deadlock class of bug documented in `docs/debug_log.md`
- **Host-side pipeline**: a serial reader (real hardware or a hardware-independent simulator), a resilient line parser, and SQLite-backed storage with time-range queries — developed and tested independently of firmware availability

## Data Format

UART output is plain-text, comma-separated, newline-terminated:

```
DATE:2026-07-16,TIME:14:25:38,TEMP:25.3,LIGHT:67
```

Out-of-range fields carry an `_ERR` suffix on the value, e.g. `TEMP:85.0_ERR`. Full specification: [`docs/requirements_spec.md`](docs/requirements_spec.md) §6.

## Project Structure

```
environmental-monitoring-system/
├── docs/
│   ├── requirements_spec.md          # Requirements (firmware + shared UART contract)
│   ├── design.md                     # Firmware design
│   ├── test_plan.md                  # Firmware test plan
│   ├── traceability_matrix.md        # Firmware requirements traceability
│   ├── host_design.md                # Host-side (Python) design
│   ├── host_test_plan.md             # Host-side test plan
│   ├── host_traceability_matrix.md   # Host-side traceability + firmware↔host contract map
│   └── debug_log.md                  # Bugs found and fixed during bring-up, with root cause
├── firmware/                          # STM32CubeMX + CMake project (VS Code)
│   └── Core/Src/, Core/Inc/           # HAL-generated code + hand-written application modules
├── host/                              # Python host application
│   ├── serial_reader.py               # SerialReader (real hardware) + SimulatedReader
│   ├── parser.py                      # UART line → Reading parsing
│   ├── storage.py                     # SQLite persistence
│   ├── main.py                        # Wires reader → parser → storage into a runnable pipeline
│   └── tests/                         # pytest suite
└── README.md
```

## Getting Started

### Firmware

1. Open `firmware/` in VS Code with the STM32Cube extension (or import into STM32CubeIDE).
2. Build via CMake (presets are provided in `CMakePresets.json`).
3. Flash to an ATK Explorer STM32F407ZGT6 board wired per `docs/requirements_spec.md` §4.
4. Connect over USART1 (115200 8N1) with a serial terminal (or the host app below) to see live data.
5. Set the RTC time via usmart: `set_rtc_time(2026,7,16,14,0,0)`.

### Host

```bash
cd host
pip install -r requirements.txt

# Run against real hardware:
python main.py --port COM4

# Or without any hardware attached:
python main.py --simulate
```

Readings are persisted to `environmental_monitoring_system.db` (SQLite) as they arrive.

Run the test suite (no hardware required):
```bash
pytest tests/
```

## Documentation

This project follows a V-model documentation set. Each document traces to the ones around it:

| Document | Covers |
|----------|--------|
| [`requirements_spec.md`](docs/requirements_spec.md) | Functional/non-functional requirements, UART data format (the firmware↔host contract) |
| [`design.md`](docs/design.md) | Firmware module breakdown, main loop scheduling, known pitfalls |
| [`test_plan.md`](docs/test_plan.md) | Firmware test cases, mapped to requirements |
| [`traceability_matrix.md`](docs/traceability_matrix.md) | Firmware requirements ↔ design ↔ test cross-reference |
| [`host_design.md`](docs/host_design.md) | Host-side module breakdown (serial reading, parsing, storage) |
| [`host_test_plan.md`](docs/host_test_plan.md) | Host-side test cases |
| [`host_traceability_matrix.md`](docs/host_traceability_matrix.md) | Host-side traceability + firmware↔host contract bridge points |
| [`debug_log.md`](docs/debug_log.md) | Real bugs found during bring-up, with root cause and fix |

## Roadmap / Known Gaps

- **Host GUI** (PyQt5): live readout, chart, RTC-set control — not yet implemented
- **`SerialReader` hardware-in-the-loop test**: deferred, requires a connected board (`@pytest.mark.hardware`, not yet written)
- **24-hour soak test**: planned per `test_plan.md` TC-SYS-001-01, not yet executed
- **GUI-issued RTC set command** (`SET_TIME:` over UART, alongside usmart): reserved in the design, not yet implemented on the firmware side

## Out of Scope (this iteration)

- Absolute lux calibration for the light sensor (percentage-based only)
- Internal temperature sensor calibration (factory-typical conversion constants only, see `requirements_spec.md` §7.2 for the accuracy caveat)
- Network connectivity (WiFi/Ethernet)
- Multi-device / multi-host UART fan-in
