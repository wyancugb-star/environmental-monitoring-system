# Host-Side Test Plan
## STM32 Environmental Monitoring System — Python Host Application

**Document Version:** 1.2
**Date:** 2026-07-23 (updated from v1.1)
**Author:** Yan
**Status:** Baseline
**Parent Documents:** `requirements_spec.md` v1.1 §6, `host_design.md` v1.0

---

## 1. Purpose & Scope

This document defines test cases for the host-side application (`host/`), mirroring the structure of the firmware `test_plan.md`. Most cases here run **without hardware attached**, using `SimulatedReader` (`host_design.md` §3.1) as the data source — this is a deliberate design property, not a limitation, and is called out per-case below. A small, explicitly marked set requires a real board (`@pytest.mark.hardware`).

## 2. Test Environment & Equipment

| Item | Purpose |
|------|---------|
| Python environment with `pyserial`, `pytest`, `PyQt5`, `pandas`, `matplotlib` installed | Run host application and test suite |
| `SimulatedReader` (built into `host/`) | Primary data source for the majority of test cases — no hardware required |
| ATK 探索者 STM32F407ZGT6 board, flashed with target firmware, connected via USB-serial | Required only for `@pytest.mark.hardware`-marked cases |

## 3. Test Case Format

Same structure as the firmware `test_plan.md` §3: **ID**, **Traces to**, **Preconditions**, **Steps**, **Expected Result**, plus one additional field here:

| Field | Meaning |
|-------|---------|
| **Hardware Required** | `No` (runs via `SimulatedReader`, part of default `pytest` run) or `Yes` (`@pytest.mark.hardware`, opt-in) |

## 4. Test Cases — Parser (`parser.py`)

### TC-HOST-PARSE-001: Well-formed frame parses correctly
**Traces to:** §6.1 UART format, `host_design.md` §3.2
**Hardware Required:** No
**Preconditions:** None.
**Steps:**
1. Call `parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:25.3,LIGHT:67")`.

**Expected Result:** Returns a `Reading` with `date="2026-07-16"`, `time="14:25:38"`, `temp_c=25.3`, `temp_err=False`, `light_pct=67`, `light_err=False`.

### TC-HOST-PARSE-002: `_ERR` suffix is parsed into the error flag, not left in the value
**Traces to:** §6.2 out-of-range handling, `host_design.md` §3.2
**Hardware Required:** No
**Preconditions:** None.
**Steps:**
1. Call `parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:85.0_ERR,LIGHT:67")`.

**Expected Result:** Returns a `Reading` with `temp_c=85.0` (clean float, no `_ERR` text in the numeric field), `temp_err=True`.

### TC-HOST-PARSE-003: Non-data lines are silently skipped, not treated as errors
**Traces to:** §6.4 UART channel sharing, `host_design.md` §3.2
**Hardware Required:** No
**Preconditions:** None.
**Steps:**
1. Call `parse_line("RTC SET OK: 2026-07-16 14:00:00")`.
2. Call `parse_line("")` (blank line).
3. Call `parse_line("usmart>")` (usmart prompt echo, or similar non-`DATE:` text).

**Expected Result:** All three calls return `None`. No exception raised in any case.

### TC-HOST-PARSE-004: Malformed data-like line does not crash the parser
**Traces to:** Robustness of `host_design.md` §3.2
**Hardware Required:** No
**Preconditions:** None.
**Steps:**
1. Call `parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:abc,LIGHT:67")` (corrupted numeric field, e.g. from a UART bit error).

**Expected Result:** Returns `None` (caught via `except ValueError`, not a bare `except`, so an unrelated bug in the parser itself would still surface as a real crash rather than being silently absorbed). This protects the GUI/storage pipeline from a single corrupted frame taking down the whole application.

### TC-HOST-PARSE-005: Empty or field-incomplete line does not crash the parser
**Traces to:** Robustness of `parse_line()` (found during manual testing, not originally anticipated)
**Hardware Required:** No
**Preconditions:** None.
**Steps:**
1. Call `parse_line("")`.
2. Call `parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:25.3")` (missing the LIGHT field entirely).

**Expected Result:** Both calls return `None`, no exception raised. (An earlier implementation raised `IndexError` on empty-string input, because splitting an empty string on `:` yields a single-element list; the fix checks the split result has exactly two parts before treating it as a valid `key:value` pair, so a malformed field is skipped rather than indexed into out of range.)

## 5. Test Cases — SimulatedReader (`serial_reader.py`)

### TC-HOST-SIM-001: Simulated output matches the wire format
**Traces to:** `host_design.md` §3.1, cross-checks against §6.1
**Hardware Required:** No
**Preconditions:** `SimulatedReader(interval_s=0.01, inject_errors=False)` (fast interval for test speed, not the real 1 Hz).
**Steps:**
1. Collect 20+ lines from `read_line()`.
2. Run each through `parse_line()`.

**Expected Result:** All lines parse successfully into valid `Reading` objects (none return `None`), confirming the simulator and parser agree on the format — this is the test that keeps the two from silently drifting apart as either is modified. `inject_errors=False` is required here; with it enabled, occasional non-data lines are expected (see TC-HOST-SIM-002) and would cause a false failure.

### TC-HOST-SIM-002: Error injection produces `_ERR`-flagged and non-data lines
**Traces to:** `host_design.md` §3.1
**Hardware Required:** No
**Preconditions:** `SimulatedReader(interval_s=0.01, inject_errors=True)`.
**Steps:**
1. Collect a large number of lines (e.g. 200+; the exact count needed depends on the random-walk step size and boundary distance, and was found by trial to need more than the originally-planned 200 for reliable triggering -- see TC-HOST-SIM-003 for the underlying mechanism).
2. Classify each via `parse_line()`.

**Expected Result:** At least one line returns `None` from `parse_line()` (simulated non-data traffic, e.g. a fake `RTC SET OK` line, gated by `inject_errors`). At least one line parses with `temp_err=True`, and at least one with `light_err=True` (see TC-HOST-SIM-003 for why these are *not* directly gated by `inject_errors`).

### TC-HOST-SIM-003: `_ERR` flag reflects actual clamping, independent of `inject_errors`
**Traces to:** `host_design.md` §3.1 (design correction found via testing)
**Hardware Required:** No
**Preconditions:** `SimulatedReader(interval_s=0, inject_errors=True)`, run for a large number of iterations (e.g. 2000+).
**Steps:**
1. For every line where TEMP or LIGHT equals a boundary value (-40.0/85.0 for temp, 0/100 for light), record whether `_ERR` is present.
2. Separately track the pre-clamp (raw, pre-boundary-check) value for each such case.

**Expected Result:** `_ERR` is present if and only if the raw value would have gone *past* the boundary (i.e. clamping actually occurred). Landing exactly on the boundary via normal random-walk drift, without overshooting, does not get `_ERR` -- this is correct behavior, not a bug, since the boundary value itself is valid and doesn't need clamping. (An earlier implementation used a fixed-probability "forced" branch to set `_ERR`, decoupled from the actual simulated value, which could disagree with the printed number after a subsequent drift step overwrote it -- this was corrected so `_ERR` is derived from checking the real value against the boundary, matching how firmware's `acq_read()` decides to clamp.)

## 6. Test Cases — Storage (`storage.py`)

### TC-HOST-STORE-001: Append then query round-trip
**Traces to:** REQ-COMM-002 (once/s persistence), `host_design.md` §3.3
**Hardware Required:** No
**Preconditions:** Fresh temp SQLite file (pytest fixture, per `host_design.md` §4 `conftest.py`).
**Steps:**
1. Append 10 `Reading` objects with known, distinct timestamps.
2. Call `query_range()` covering the full span.

**Expected Result:** All 10 readings are returned, in the correct order, with all fields matching what was appended (including `_err` flags).

### TC-HOST-STORE-002: `query_range()` correctly excludes out-of-window readings
**Traces to:** `host_design.md` §3.3
**Hardware Required:** No
**Preconditions:** Same as TC-HOST-STORE-001.
**Steps:**
1. Append readings spanning a 10-minute window.
2. Query a 2-minute sub-window in the middle.

**Expected Result:** Only readings whose timestamp falls inside the 2-minute window are returned — none before or after.

### TC-HOST-STORE-003: Storage survives an application restart
**Traces to:** "Durable local store" intent of `host_design.md` §3.3
**Hardware Required:** No
**Preconditions:** Named (non-temp) SQLite file.
**Steps:**
1. Append 5 readings, close the `Storage` instance / end the process.
2. Open a new `Storage` instance pointing at the same file.
3. Query the full range.

**Expected Result:** All 5 readings are present — confirms data isn't only living in an in-memory structure that happens to also touch disk.

## 7. Test Cases — GUI (`gui.py`)

GUI testing is inherently harder to automate exhaustively; these cases favor targeted, high-value checks over full UI automation, consistent with keeping the fast test suite fast.

### TC-HOST-GUI-001: Live readout updates on new data
**Traces to:** REQ-DISP-002 (host-side mirror), `host_design.md` §3.4
**Hardware Required:** No (drive the GUI with `SimulatedReader`)
**Preconditions:** GUI launched in `--simulate` mode (`host_design.md` §6).
**Steps:**
1. Launch GUI, let 3 simulated frames arrive.
2. Inspect the displayed TEMP/LIGHT values (via `pytest-qt` widget inspection, or manual visual check if automated widget testing isn't set up yet).

**Expected Result:** Displayed values match the most recently parsed `Reading`.

### TC-HOST-GUI-002: `_ERR`-flagged readings are visually distinguished
**Traces to:** REQ-SYS-002 (host-side mirror), `host_design.md` §3.4
**Hardware Required:** No
**Preconditions:** GUI launched with `SimulatedReader(inject_errors=True)`.
**Steps:**
1. Wait for an `_ERR`-flagged reading to arrive.
2. Inspect the widget's styling/color at that moment.

**Expected Result:** The affected field is visually flagged (e.g. red text/background), distinguishable from normal readings without reading the raw value.

### TC-HOST-GUI-003: "Set RTC Time" control sends the correct command string
**Traces to:** §6.4 GUI channel, `host_design.md` §3.4
**Hardware Required:** No for the command-formatting check; Yes for confirming the device actually responds correctly (now executable — see firmware `test_plan.md` TC-COMM-004-01, no longer deferred).
**Steps:**
1. (No-hardware part) Set the date/time picker to a known value, click "Set", and intercept/mock the serial write call.
2. (Hardware part, separate case) With a real device connected, repeat and confirm via TC-COMM-004-01 in the firmware test plan that the device accepts it.

**Expected Result:** (No-hardware) The exact string written is `SET_TIME:2026-07-16,14:00:00\n`, matching the format firmware expects to eventually parse. (Hardware) Device responds per firmware-side `TC-COMM-004-01` once that firmware path is implemented.

## 8. Test Cases — Hardware-in-the-Loop

### TC-HOST-HW-001: End-to-end read from a real device
**Traces to:** Full pipeline validation
**Hardware Required:** Yes (`@pytest.mark.hardware`)
**Preconditions:** Real board flashed and running, connected via known COM port.
**Steps:**
1. Run `pytest tests/test_hardware.py --port <COM port>`.
2. `SerialReader` reads 10 lines from the real device; each is passed through `parse_line()`.

**Expected Result:** At least one line parses successfully into a `Reading` (`assert parsed_count > 0`). Without `--port` supplied, the test is skipped (not failed), via a `port` fixture in `conftest.py` reading the `--port` CLI option registered in `pytest.ini`. This is the point where firmware and host-side `requirements_spec.md` §6 conformance is confirmed end-to-end, not just independently. **Status: implemented and passing (2026-07-23).**

## 9. Pass/Fail Summary Template

| Test ID | Hardware Required | Result | Date | Notes |
|---------|---------------------|--------|------|-------|
| TC-HOST-PARSE-001 | No | | | |
| TC-HOST-PARSE-002 | No | | | |
| TC-HOST-PARSE-003 | No | | | |
| TC-HOST-PARSE-004 | No | | | |
| TC-HOST-PARSE-005 | No | | | |
| TC-HOST-SIM-001 | No | | | |
| TC-HOST-SIM-002 | No | | | |
| TC-HOST-SIM-003 | No | | | |
| TC-HOST-STORE-001 | No | | | |
| TC-HOST-STORE-002 | No | | | |
| TC-HOST-STORE-003 | No | | | |
| TC-HOST-GUI-001 | No | | | |
| TC-HOST-GUI-002 | No | | | |
| TC-HOST-GUI-003 | Yes | | | |
| TC-HOST-HW-001 | Yes | | | Implemented; run with `--port <COM>`, skipped otherwise |

## 10. Requirements Traceability

| REQ-ID / Source | Host Test Case(s) |
|-------------------|----------------------|
| §6.1 UART format | TC-HOST-PARSE-001, TC-HOST-PARSE-005, TC-HOST-SIM-001 |
| §6.2 out-of-range `_ERR` | TC-HOST-PARSE-002, TC-HOST-SIM-002, TC-HOST-SIM-003, TC-HOST-GUI-002 |
| §6.4 UART channel sharing | TC-HOST-PARSE-003, TC-HOST-GUI-003 |
| REQ-COMM-002 (persistence cadence) | TC-HOST-STORE-001 |
| REQ-DISP-002 (host mirror) | TC-HOST-GUI-001 |
| Full pipeline / firmware↔host conformance | TC-HOST-HW-001 |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-20 | Initial baseline: parser, SimulatedReader, storage, GUI, and hardware-in-the-loop test cases; pass/fail template; traceability to requirements_spec.md §6 and host_design.md. Includes TC-HOST-PARSE-005 (empty/incomplete-field robustness) and TC-HOST-SIM-003 (`_ERR` reflects actual clamping, independent of `inject_errors`), added after issues were found during manual implementation and testing. |
| 1.1 | 2026-07-23 | TC-HOST-GUI-003 updated: hardware-dependent part is now executable, no longer deferred, now that firmware's SET_TIME: parser is implemented |
| 1.2 | 2026-07-23 | TC-HOST-HW-001 implemented and passing: test_hardware.py + conftest.py --port fixture + pytest.ini marker registration. Updated test description to match actual steps (10 lines via pytest.mark.hardware, not the originally-planned 30s duration). |
