# Requirements Specification
## STM32 Environmental Monitoring System

**Document Version:** 1.0
**Date:** 2026-07-16
**Author:** Yan

---

## 1. Purpose

Design and implement an STM32-based environmental monitoring system
that acquires ambient light intensity and internal chip temperature,
displays real-time status on an LCD with actual date/time, and
transmits data to a host PC over UART for logging, visualization,
and automated validation.

## 2. System Overview

[Light Sensor] ─┐
├─→ [STM32 ADC] ─→ [RTC] ─→ [LCD Display]
[Internal Temp] ─┘         │
└─→ [UART] ─→ [Python Host: Storage + GUI + Tests]

## 3. Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| REQ-ACQ-001 | System shall sample light intensity and internal temperature once per second | High |
| REQ-ACQ-002 | Temperature shall be reported with 0.1°C resolution | High |
| REQ-ACQ-003 | Light intensity shall be reported as a percentage (0-100%) | High |
| REQ-DISP-001 | LCD shall display the current real-time clock (RTC), updated at least once per second | High |
| REQ-DISP-002 | LCD shall display the most recent light and temperature readings | Medium |
| REQ-COMM-001 | System shall transmit each sample over UART as human-readable text | High |
| REQ-COMM-002 | UART transmission shall occur once per second, synchronized with acquisition | High |
| REQ-SYS-001 | System shall continue normal operation (no reset/hang) during continuous 24-hour operation | High |

## 4. Data Format Specification

### 4.1 UART Output Format

Plain-text, comma-separated, newline-terminated:
TIME:HH:MM:SS,TEMP:25.3,LIGHT:67

| Field | Format | Range | Notes |
|-------|--------|-------|-------|
| TIME | HH:MM:SS | 00:00:00–23:59:59 | From RTC |
| TEMP | ##.# | Chip-dependent (~-40 to 85°C) | Internal temp sensor, 1 decimal |
| LIGHT | ### | 0–100 | Percentage, higher = brighter |

*(Exact format may be refined during implementation; this spec will be updated accordingly.)*

## 5. Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| REQ-NFR-001 | UART baud rate: 115200, 8N1 |
| REQ-NFR-002 | System shall recover automatically from a hung main loop (watchdog) |
| REQ-NFR-003 | LCD refresh shall not visibly flicker under normal operation |

## 6. Out of Scope (for this iteration)

- Absolute lux calibration for the light sensor (percentage-based only)
- Data logging/GUI on the host side (defined separately in host-side spec)
- Network connectivity (WiFi/Ethernet)
- Multi-sensor expansion beyond light + internal temperature

## 7. Open Questions / To Be Determined

- [ ] Exact ADC channel/pin mapping for the light sensor (to be confirmed via CubeMX)
- [ ] RTC initial time-setting mechanism (hardcoded at build time, or set via UART command?)
- [ ] Behavior when a reading is out of expected range (clamp, flag, or discard?)

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial draft |