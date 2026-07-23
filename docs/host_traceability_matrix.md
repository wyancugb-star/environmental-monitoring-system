# Host-Side Traceability Matrix
## STM32 Environmental Monitoring System — Python Host Application

**Document Version:** 1.1
**Date:** 2026-07-23 (updated from 2026-07-16 baseline)
**Author:** Yan
**Status:** Baseline
**Source Documents:** `requirements_spec.md` v1.1 §6, `host_design.md` v1.0, `host_test_plan.md` v1.0
**Sibling Document:** `traceability_matrix.md` (firmware-scope) — the two are kept separate rather than merged, since firmware and host are independently developed and tested (per `host_design.md` §6). §3 below is the explicit bridge between them.

---

## 1. Purpose

Consolidated view tying each host-side requirement/interface item to its design element and test case(s), mirroring the firmware `traceability_matrix.md` in structure and intent.

## 2. Full Traceability Matrix

| Source Item | Summary | Design Element (`host_design.md`) | Test Case(s) (`host_test_plan.md`) | Coverage Status |
|---------------|---------|-------------------------------------|----------------------------------------|-------------------|
| §6.1 UART format | Parse `DATE:...,TIME:...,TEMP:...,LIGHT:...` frames | §3.2 `parse_line()` | TC-HOST-PARSE-001, TC-HOST-SIM-001 | ✅ Full |
| §6.2 Out-of-range `_ERR` | Parse and surface `_ERR`-flagged fields | §3.2 `_err` flag parsing, §3.4 red-highlighted display | TC-HOST-PARSE-002, TC-HOST-SIM-002, TC-HOST-GUI-002 | ✅ Full |
| §6.4 UART channel sharing | Ignore non-`DATE:` lines (usmart echoes); send `SET_TIME:` GUI commands | §3.2 prefix filtering, §3.4 "Set RTC Time" control | TC-HOST-PARSE-003, TC-HOST-GUI-003 | ⚠️ Partial — command-formatting covered without hardware; device-accepts-it confirmation depends on firmware-side `TC-COMM-004-01`, which is itself deferred |
| REQ-COMM-002 (host mirror) | Persist each received frame | §3.3 `storage.append()` | TC-HOST-STORE-001, TC-HOST-STORE-002, TC-HOST-STORE-003 | ✅ Full |
| REQ-DISP-002 (host mirror) | Display most recent reading | §3.4 live readout, progress bars | TC-HOST-GUI-001 | ✅ Full |
| Parser robustness (no formal REQ-ID; design-level concern) | Malformed/corrupted frames must not crash the app | §3.2 `parse_line()` failure handling | TC-HOST-PARSE-004 | ✅ Full |
| Development-without-hardware capability (no formal REQ-ID; design-level concern) | `gui`/`storage`/tests must be verifiable before firmware is flashable | §3.1 `SimulatedReader`, §6 `--simulate` flag | TC-HOST-SIM-001, TC-HOST-SIM-002, all `No`-hardware cases in `host_test_plan.md` | ✅ Full |
| End-to-end firmware↔host conformance | Real device output actually matches what the host expects | N/A (integration point, not a single module) | TC-HOST-HW-001 | ⚠️ Partial — blocked on hardware availability/firmware completion, not a design gap |

## 3. Firmware ↔ Host Bridge Points

These are the specific places where the two independently-developed sides must agree, and where the corresponding test case on each side is listed side by side so a mismatch is easy to spot:

| Contract | Firmware Side | Host Side |
|----------|-----------------|-------------|
| Normal data frame format | `design.md` §3.4 `uart_tx_frame()`; tested by `test_plan.md` TC-COMM-001-01 | `host_design.md` §3.2 `parse_line()`; tested by `host_test_plan.md` TC-HOST-PARSE-001 |
| `_ERR` suffix convention | `design.md` §3.1/§3.4; tested by `test_plan.md` TC-SYS-002-01/02 | `host_design.md` §3.2; tested by `host_test_plan.md` TC-HOST-PARSE-002 |
| usmart/data-frame coexistence on shared UART | `design.md` §3.5; tested by `test_plan.md` TC-COMM-005-01 | `host_design.md` §3.2 prefix filtering; tested by `host_test_plan.md` TC-HOST-PARSE-003 |
| GUI-issued `SET_TIME:` command | `design.md` §3.5 (implemented, try_handle_set_time_command); `test_plan.md` TC-COMM-004-01 (now executable) | `host_design.md` §3.4 (implemented); `host_test_plan.md` TC-HOST-GUI-003 |
| Full end-to-end conformance | `test_plan.md` §7 24h soak (TC-SYS-001-01) exercises the firmware side continuously | `host_test_plan.md` TC-HOST-HW-001 is the one case that actually connects both sides together |

The `SET_TIME:` command, previously the only contract item blocked on both sides simultaneously, is now fully implemented on both ends.

## 4. Coverage Gap Analysis

- **TC-HOST-HW-001:** The only host-side test that cannot run purely against `SimulatedReader`. This is expected — some amount of "does the real thing actually work end-to-end" verification can never be fully replaced by simulation, and this matrix exists partly to make that boundary explicit rather than implicit.

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial baseline: host-side traceability table, firmware↔host bridge-point cross-reference, and coverage gap analysis |
| 1.1 | 2026-07-23 | §6.4 GUI channel (SET_TIME:) updated to Full coverage on both firmware and host sides. Removed from coverage gap analysis. |
