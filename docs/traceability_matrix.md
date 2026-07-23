# Traceability Matrix
## STM32 Environmental Monitoring System

**Document Version:** 1.1
**Date:** 2026-07-23 (updated from 2026-07-16 baseline)
**Author:** Yan
**Status:** Baseline
**Source Documents:** `requirements_spec.md` v1.1, `design.md` v1.1, `test_plan.md` v1.0

---

## 1. Purpose

This document is the single consolidated view tying every requirement to its design element(s) and test case(s). `requirements_spec.md` §10, `design.md` §8, and `test_plan.md` §10 each already maintain a partial traceability table scoped to their own document; this matrix merges all three into one place so gaps (a requirement with no design coverage, or design with no test coverage) are visible at a glance without cross-referencing three files.

This is a **firmware-scope** matrix. Host-side requirements/design/tests will get their own matrix once `host_design.md` exists, and may later be merged into this one or cross-referenced from it — that decision is deferred until the host-side documents exist.

## 2. Full Traceability Matrix

| REQ-ID | Requirement Summary | Design Element (`design.md`) | Test Case(s) (`test_plan.md`) | Coverage Status |
|--------|----------------------|-------------------------------|---------------------------------|-------------------|
| REQ-ACQ-001 | Sample light + internal temp once per second | §3.6 main loop 1 Hz tick calling `acq_read()` | TC-ACQ-001-01 | ✅ Full |
| REQ-ACQ-002 | Temp displayed with 0.1°C resolution | §3.1 `acq_adc` temp conversion | TC-ACQ-002-01 | ✅ Full |
| REQ-ACQ-003 | Light reported as 0–100% | §3.1 `acq_adc` light conversion | TC-ACQ-003-01 | ✅ Full |
| REQ-DISP-001 | LCD shows RTC, updated ≥1/s | §3.2 `rtc_mgr_get()`, §3.3 `lcd_disp_update()` | TC-DISP-001-01 | ✅ Full |
| REQ-DISP-002 | LCD shows most recent light/temp reading | §3.3 `lcd_disp_update()`, progress-bar sub-design | TC-DISP-002-01, TC-DISP-002-02 | ✅ Full |
| REQ-COMM-001 | UART frame is human-readable text | §3.4 `uart_tx_frame()` | TC-COMM-001-01 | ✅ Full |
| REQ-COMM-002 | UART sent once/s, synced with acquisition | §3.6 main loop 1 Hz tick calling `uart_tx_frame()` | TC-COMM-002-01 | ✅ Full |
| REQ-COMM-003 | RTC set via UART command (usmart primary, GUI channel now implemented via SET_TIME: parser in rtc_mgr.c) | §3.2 `set_rtc_time()`, §3.5 usmart registration + SET_TIME: command parser (try_handle_set_time_command) | TC-COMM-003-01 (usmart), TC-COMM-004-01 (GUI, now fully executable) | ✅ Full |TC-COMM-004-01 (GUI, deferred) | ⚠️ Partial — GUI path deferred pending host tooling |
| REQ-SYS-001 | No reset/hang over 24h continuous operation | §3.6 IWDG feed placement, §4 blocking-call pitfall mitigation | TC-SYS-001-01 | ✅ Full |
| REQ-SYS-002 | Out-of-range readings clamped + flagged | §3.1 clamp logic in `acq_read()`, §3.4 `_ERR` suffix in `uart_tx_frame()` | TC-SYS-002-01 (light), TC-SYS-002-02 (temp) | ✅ Full |
| REQ-SYS-003 | Sentinel default RTC time on true first boot | §3.2 `rtc_mgr_init()` sentinel check | TC-SYS-003-01 | ✅ Full |
| REQ-SYS-004 | RTC persists across reset/reflash (VBAT held); reverts to sentinel on VBAT loss | §3.2 `rtc_mgr_init()` sentinel check (same code path handles both first-boot and VBAT-loss cases) | TC-SYS-004-01 (VBAT held), TC-SYS-004-02 (VBAT lost) | ✅ Full |
| REQ-NFR-001 | UART 115200, 8N1 | §3.4 (CubeMX USART1 config, not application code) | TC-NFR-001-01 | ✅ Full |
| REQ-NFR-002 | Watchdog recovers a hung main loop | §3.6 IWDG feed every loop iteration | TC-NFR-002-01 | ⚠️ Partial — requires a temporary debug build with injected hang; not part of the release firmware |
| REQ-NFR-003 | LCD refresh has no visible flicker | §3.3 partial-redraw anti-flicker design (text + progress bars) | TC-DISP-003-01, TC-DISP-002-02 | ✅ Full |
| §6.4 (informative, not a numbered REQ) | usmart and `uart_tx` coexist on shared USART1 without corruption | §3.5 usmart + `uart_tx` sharing rationale | TC-COMM-005-01 | ✅ Full |

## 3. Coverage Gap Analysis

Two items are intentionally not "Full" coverage, and both are tracked deliberately rather than silently missing:

- **REQ-NFR-002 (watchdog recovery):** `TC-NFR-002-01` requires a deliberately-hung debug build, which is a different firmware image than what ships. This is noted so that whoever runs this test (including future-you) doesn't mistake "requires special build" for a documentation gap — it's a recognized constraint of testing recovery behavior safely.

No requirement in `requirements_spec.md` v1.1 is missing from this matrix, and no design element in `design.md` v1.0 is untested without an explicit, stated reason.

## 4. How to Use This Document

When a requirement changes (new REQ-ID, or an existing one is modified):
1. Update `requirements_spec.md` first (source of truth for *what*).
2. Update `design.md` to reflect *how* it's implemented, if affected.
3. Update `test_plan.md` with a new/revised test case.
4. Update this matrix's row last — it should always be the final, derived summary, not edited independently of the other three.

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial baseline: consolidated traceability across requirements_spec.md v1.1, design.md v1.0, and test_plan.md v1.0; coverage gap analysis for REQ-COMM-003 (GUI channel) and REQ-NFR-002 (watchdog test build) |
| 1.1 | 2026-07-23 | REQ-COMM-003 updated to Full coverage: GUI-issued SET_TIME: command now implemented on both firmware and host sides. Removed from coverage gap analysis. |
