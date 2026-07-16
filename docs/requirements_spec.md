# Requirements Specification
## STM32 Environmental Monitoring System

**Document Version:** 1.1
**Date:** 2026-07-16
**Author:** Yan
**Status:** Baseline

---

## 1. Purpose

Design and implement an STM32-based environmental monitoring system that acquires ambient light intensity and internal chip temperature, displays real-time status on an LCD with actual date/time, and transmits data to a host PC over UART for logging, visualization, and automated validation.

## 2. Glossary

| Term | Definition |
|------|------------|
| ADC | Analog-to-Digital Converter |
| RTC | Real-Time Clock |
| UART | Universal Asynchronous Receiver/Transmitter |
| LCD | Liquid Crystal Display |
| FSMC | Flexible Static Memory Controller (STM32 peripheral used here for a parallel-interface LCD) |
| usmart | Dynamic C-function-call-over-UART debug utility, reused from a prior project in this codebase |
| 8N1 | 8 data bits, No parity, 1 stop bit (UART framing) |
| Watchdog (IWDG/WWDG) | Hardware timer that resets the MCU if the main loop fails to "kick" it periodically |
| Clamp | Force an out-of-range value to the nearest valid boundary |
| Sentinel value | A deliberately unrealistic placeholder value used to signal "not yet set" |

## 3. System Overview

```
[Light Sensor]  ─┐
                 ├─→ [STM32 ADC] ─→ [RTC] ─→ [LCD Display]
[Internal Temp] ─┘         │
                           └─→ [UART] ─→ [Python Host: Storage + GUI + Tests]
```

## 4. Hardware Interfaces

| Item | Specification |
|------|----------------|
| MCU / Dev Board | STM32F407ZGT6/ATK-DNF407 V3.4 |
| Light Sensor | Onboard photoresistor (analog), sampled via ADC on the board's designated light-sensor channel |
| Temperature Source | Internal STM32 temperature sensor (ADC internal channel), no external sensor; onboard DS18B20/DHT11 digital sensor interface is present but **not used** in this project |
| LCD | ATK-MD0430, 4.3", NT35510 driver, **FSMC 16-bit parallel interface** |
| Debug/Utility Console | usmart (dynamic C-function-call-over-UART tool) — used for the RTC time-set command (§6.3) |
| UART Interface | USART1, connected to host PC via USB-to-serial adapter |
| Power Supply | 5V via USB (dev board onboard regulation supplies MCU/LCD logic) |

*(Exact ADC channel/GPIO pin for the light sensor to be confirmed via CubeMX once the board schematic is checked; this table is the authoritative source of truth once locked.)*

## 5. Functional Requirements

| ID | Requirement | Priority | Verification Method |
|----|-------------|----------|----------------------|
| REQ-ACQ-001 | System shall sample light intensity and internal temperature once per second | High | Log UART stream for 5 min; confirm sample timestamps are spaced 1.0 s ± 50 ms |
| REQ-ACQ-002 | Temperature shall be **displayed** with 0.1°C resolution (see note on accuracy in §7.2) | High | Inspect 1000 consecutive UART samples; confirm TEMP field always has exactly 1 decimal digit |
| REQ-ACQ-003 | Light intensity shall be reported as a percentage (0–100%) | High | Sweep light source from dark to bright; confirm LIGHT field stays within [0,100] and is monotonic with illuminance |
| REQ-DISP-001 | LCD shall display the current real-time clock (RTC), updated at least once per second | High | Visual inspection with stopwatch; compare LCD time to UART TIME field over 60 s |
| REQ-DISP-002 | LCD shall display the most recent light and temperature readings | Medium | Visual inspection; cross-check displayed values against corresponding UART frame |
| REQ-COMM-001 | System shall transmit each sample over UART as human-readable text | High | Capture raw UART bytes on host; confirm ASCII-decodable per §6.1 format |
| REQ-COMM-002 | UART transmission shall occur once per second, synchronized with acquisition | High | Timestamp each received UART frame on host; confirm inter-frame interval 1.0 s ± 50 ms |
| REQ-COMM-003 | System shall support setting the RTC time via a UART-issued command, using the usmart function-call mechanism (`set_rtc_time(year, month, day, hour, min, sec)`) | High | Issue `set_rtc_time` via usmart console; confirm echoed `RTC SET OK: ...` line and confirm subsequent UART DATE/TIME fields match the set value |
| REQ-SYS-001 | System shall continue normal operation (no reset/hang) during continuous 24-hour operation | High | Run 24 h soak test; log UART stream continuously; confirm zero unexpected gaps >2 s and zero unintended resets |
| REQ-SYS-002 | Out-of-range readings shall be clamped to the nearest valid boundary and flagged | High | Inject out-of-range condition (e.g. cover sensor / extreme temp); confirm value clamps to boundary and `_ERR` suffix appears (see §6.2) |
| REQ-SYS-003 | On first boot (no valid RTC backup flag set), the RTC shall initialize to a sentinel default time (`2000-01-01 00:00:00`) so an unset clock is visibly distinguishable from a real timestamp | Medium | Clear backup domain / power-cycle without calling `set_rtc_time`; confirm UART DATE/TIME reads the sentinel value until `set_rtc_time` is issued |
| REQ-SYS-004 | RTC time set via `set_rtc_time()` shall persist across MCU reset/reflash without re-calibration, as long as VBAT (backup-domain supply) remains powered; if VBAT is lost, the system shall fall back to the sentinel default (REQ-SYS-003) and require re-calibration | Medium | Reset/reflash without cutting VBAT; confirm RTC retains set time. Separately, cut VBAT (remove backup battery / power) and confirm RTC reverts to sentinel default on next boot |

## 6. Data Format Specification

### 6.1 UART Output Format (Normal)

Plain-text, comma-separated, newline-terminated:

```
DATE:2026-07-16,TIME:14:25:38,TEMP:25.3,LIGHT:67
```

| Field | Format | Range | Notes |
|-------|--------|-------|-------|
| DATE | YYYY-MM-DD | RTC-supported | From RTC |
| TIME | HH:MM:SS | 00:00:00–23:59:59 | From RTC |
| TEMP | ##.# | ~-40 to 85°C (chip-dependent) | Internal temp sensor, 1 decimal, see §7.2 for accuracy caveat |
| LIGHT | ### | 0–100 | Percentage, higher = brighter |

### 6.2 Out-of-Range Handling

Readings outside the expected range are **clamped to the nearest boundary** and **flagged** with an `_ERR` suffix on the affected field, e.g.:

```
DATE:2026-07-16,TIME:14:25:38,TEMP:85.0_ERR,LIGHT:67
```

This keeps the frame format stable (parsers don't break) while still making faults visible to the host-side logger/GUI.

### 6.3 RTC Time-Set Command (usmart)

The RTC is set at runtime via usmart, not via a custom text-command parser. This reuses the existing usmart integration from a prior project in this codebase rather than writing a second parser.

```c
void set_rtc_time(u16 year, u8 month, u8 day, u8 hour, u8 min, u8 sec);
```

- Parameters are passed as discrete numeric values (not a packed string), since usmart parses numeric argument lists more reliably than strings.
- On success, the device echoes a confirmation line over UART: `RTC SET OK: 2026-07-16 14:00:00`.
- Until `set_rtc_time` is called after a cold boot with no valid backup-domain flag, the RTC holds the sentinel default `2000-01-01 00:00:00` (see REQ-SYS-003) so unset time is visually obvious in logs rather than silently wrong.

### 6.4 UART Channel Sharing

`uart_tx` (outgoing 1 Hz data frames, §6.1) and `usmart` (incoming debug/RTC-set commands) share the same USART1 physical interface — TX and RX are independent lines, and the single-threaded main loop processes both sequentially, so no framing conflicts occur. Host-side parsers must filter incoming lines by the `DATE:` prefix and ignore other text (usmart command echoes such as `RTC SET OK: ...`), rather than assuming every line is a data frame.

## 7. Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| REQ-NFR-001 | UART baud rate: 115200, 8N1 |
| REQ-NFR-002 | System shall recover automatically from a hung main loop via independent watchdog (IWDG), timeout ≤ 2 s |
| REQ-NFR-003 | LCD refresh shall not visibly flicker under normal operation |

### 7.1 Assumptions & Constraints

- Ambient operating temperature assumed to be within -10°C to 50°C.
- Light sensor is used for relative brightness percentage only; no absolute lux calibration is performed (see §8).
- One STM32 device communicates with exactly one host PC session at a time (no multi-device fan-in in v1.0).
- Host-side Python application is assumed to be running and listening before/while the device transmits; the device does not buffer missed frames.
- RTC time persistence depends on the backup-domain supply (VBAT) remaining powered; if the board's VBAT pin has no backup battery installed (or is tied directly to VDD), losing main power will reset the RTC to the sentinel default and require re-calibration via `set_rtc_time`.

### 7.2 Temperature Resolution vs. Accuracy

REQ-ACQ-002 specifies a **display resolution** of 0.1°C. This is distinct from **measurement accuracy**: the STM32 internal temperature sensor, without per-unit calibration (single- or dual-point), typically has an absolute accuracy on the order of **±1–2°C** per the datasheet, plus additional drift due to self-heating from the MCU. This system reports factory-calibration-only readings; the 0.1°C figure reflects display granularity, not measurement precision. Calibration is out of scope for v1.0 (see §8).

## 8. Out of Scope (for this iteration)

- Absolute lux calibration for the light sensor (percentage-based only)
- Single/dual-point calibration of the internal temperature sensor
- Data logging/GUI on the host side (defined separately in host-side spec)
- Network connectivity (WiFi/Ethernet)
- Multi-sensor expansion beyond light + internal temperature
- Multi-device / multi-host UART fan-in

## 9. Key Design Decisions

| Topic | Decision |
|-------|----------|
| ADC channel/pin for light sensor | To be confirmed via CubeMX against the Explorer board schematic, see §4 |
| RTC initial time-setting | Set at runtime via usmart `set_rtc_time()` call over UART (REQ-COMM-003, §6.3); sentinel default `2000-01-01 00:00:00` before first set (REQ-SYS-003) |
| 1-second sample/send timing | `HAL_GetTick()` polling in the main loop (`now - last_tick >= 1000`), not a dedicated hardware timer, to avoid conflicting with existing timer usage elsewhere in the codebase |
| Internal temperature ADC conversion | `Temp(°C) = ((V_SENSE − V_25) / Avg_Slope) + 25`, with datasheet-typical `V_25 ≈ 0.76 V`, `Avg_Slope ≈ 2.5 mV/°C`, `V_SENSE = (ADC_RAW/4095) × 3.3V`; accuracy caveat per §7.2 still applies |
| Out-of-range reading behavior | Clamp to boundary + `_ERR` flag suffix, see §6.2 |
| RTC time-set interface (v1.0) | usmart retained as the primary/debug channel for `set_rtc_time()`. A separate GUI-facing command channel (e.g. `SET_TIME:` text prefix on the same UART) is reserved for when host-side GUI development begins — both can coexist on USART1 since usmart parses RX independently of the plain-text data frames sent on TX (§6.1) |

## 10. Requirements Traceability Summary

| Requirement Category | Requirement IDs | Test Coverage |
|-----------------------|------------------|----------------|
| Acquisition | REQ-ACQ-001 – 003 | Covered, §5 |
| Display | REQ-DISP-001 – 002 | Covered, §5 |
| Communication | REQ-COMM-001 – 003 | Covered, §5 |
| System Robustness | REQ-SYS-001 – 003 | Covered, §5 |
| Non-Functional | REQ-NFR-001 – 003 | Verified via bench test / 24h soak / visual inspection |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial baseline: full functional/non-functional requirements, hardware interfaces (ATK-DNF407, ATK-MD0430 NT35510 FSMC LCD), UART data format, RTC set-command spec via usmart, out-of-range handling, key design decisions, and requirements traceability summary |
| 1.1 | 2026-07-16 | Added REQ-SYS-004 (RTC persistence across reset vs. VBAT loss) and corresponding assumption in §7.1; added §6.4 UART Channel Sharing (usmart + uart_tx coexisting on USART1, host-side DATE: prefix filtering); updated §9 to reflect usmart-as-primary + reserved GUI command channel decision|
