# Test Plan
## STM32 Environmental Monitoring System

**Document Version:** 1.1
**Date:** 2026-07-23 (updated from 2026-07-16 baseline)
**Author:** Yan
**Status:** Baseline
**Parent Documents:** `requirements_spec.md` v1.1, `design.md` v1.0 — every test case below traces to a REQ-ID; see §7 for the full matrix.

---

## 1. Purpose & Scope

This document defines concrete, executable test cases for every requirement in `requirements_spec.md`. It covers **firmware-level verification only** — tests that can be run against the physical board with a UART terminal, a multimeter/light source, and (where noted) a host-side script. Automated pytest-based test cases (once the host Python tooling exists) will reuse the same test IDs and expected results defined here, so this document is the source of truth for both manual and automated verification.

## 2. Test Environment & Equipment

| Item | Purpose |
|------|---------|
| ATK-DNF407 V3.4 board, flashed with target firmware | Device under test (DUT) |
| USB-to-serial connection, 115200 8N1 | UART capture (REQ-NFR-001) |
| Serial terminal (e.g. PuTTY, RealTerm, or a simple pyserial logging script) | Capture/inspect raw UART frames |
| Light source (lamp, or hand/cloth for shading) capable of covering the full dark→bright range | REQ-ACQ-003, REQ-SYS-002 light-side tests |
| Ambient room thermometer (reference only, not for calibration — see §6.2 accuracy caveat) | Sanity-check TEMP field is plausible, not a calibration reference |
| Stopwatch or phone timer | REQ-ACQ-001, REQ-COMM-002, REQ-DISP-001 timing checks |
| Bench power supply or USB hub with switch (for VBAT loss test) | REQ-SYS-004 |
| CR1220 backup battery (if board's VBAT is battery-backed) | REQ-SYS-004 |

## 3. Test Case Format

Each test case follows this structure:

| Field | Meaning |
|-------|---------|
| **ID** | `TC-<REQ short name>-<seq>`, e.g. `TC-ACQ-001-01` |
| **Traces to** | The REQ-ID(s) under verification |
| **Preconditions** | State the DUT must be in before starting |
| **Steps** | Numbered actions |
| **Expected Result** | Pass criteria, stated as an observable, unambiguous condition |
| **Pass/Fail** | Left blank here; filled in during execution |

## 4. Test Cases — Acquisition

### TC-ACQ-001-01: Sample rate is 1 Hz
**Traces to:** REQ-ACQ-001
**Preconditions:** DUT powered, UART connected, terminal logging with timestamps.
**Steps:**
1. Let the DUT run for 5 minutes while logging all received UART frames with host-side receive timestamps.
2. Compute the time delta between consecutive frames.

**Expected Result:** All inter-frame deltas are 1.0 s ± 50 ms. No delta exceeds 1.1 s or drops below 0.9 s.

### TC-ACQ-002-01: Temperature field has exactly 1 decimal digit
**Traces to:** REQ-ACQ-002
**Preconditions:** DUT running normally, 1000 consecutive frames captured.
**Steps:**
1. Parse the TEMP field from each of 1000 consecutive frames.
2. Check the format matches `-?\d+\.\d` (optional sign, digits, decimal point, exactly one digit).

**Expected Result:** 100% of frames match the format. No frame has 0, 2, or more decimal digits.

### TC-ACQ-003-01: Light percentage stays in range and tracks illuminance
**Traces to:** REQ-ACQ-003
**Preconditions:** Light sensor accessible, DUT running normally.
**Steps:**
1. Fully shade the sensor (hand/cloth) and observe LIGHT field for 10 s.
2. Expose the sensor to bright light (lamp) and observe LIGHT field for 10 s.
3. Sweep gradually from dark to bright over ~30 s and record the LIGHT sequence.

**Expected Result:** LIGHT stays within [0,100] at all times. Value trends upward as illuminance increases (not required to be strictly monotonic sample-to-sample, but the overall trend must track brightness).

## 5. Test Cases — Display

### TC-DISP-001-01: RTC display updates at least once per second
**Traces to:** REQ-DISP-001
**Preconditions:** RTC previously set via `set_rtc_time` (see TC-COMM-003-01) to a known value.
**Steps:**
1. Start a stopwatch and simultaneously note the LCD-displayed time.
2. After 60 s, compare LCD time to stopwatch-elapsed time and to the UART TIME field captured at the same moment.

**Expected Result:** LCD time advances by 60 s ± 1 s over the interval. LCD time matches the corresponding UART TIME field within ±1 s at any sampled instant.

### TC-DISP-002-01: LCD shows the most recent light/temp reading
**Traces to:** REQ-DISP-002
**Preconditions:** DUT running normally.
**Steps:**
1. Change the light level (shade/expose the sensor).
2. Within 2 s, compare the LCD-displayed LIGHT value to the next UART frame's LIGHT field.

**Expected Result:** LCD value matches (or is one sample behind, i.e. matches the previous second's UART frame — never more than 1 sample stale).

### TC-DISP-002-02: Progress bars reflect light/temperature state
**Traces to:** REQ-DISP-002, REQ-NFR-003 (design-level feature, see `design.md` §3.3)
**Preconditions:** DUT running normally.
**Steps:**
1. Sweep light from dark to bright; observe the light bar length grows proportionally, with no full-bar flash/clear.
2. Change the temperature (e.g. cup hands around the board, or note ambient drift); observe the temperature bar length changes proportionally to the [-10, 50]°C normalized range.
3. Confirm both bars show a static black outline and static min/max axis labels ("0"/"100" for light, "-10"/"50" for temperature) that don't flicker or redraw each tick.

**Expected Result:** Light bar length is proportional to LIGHT%, temp bar length is proportional to the normalized temperature, both use a fixed fill color with a visible black outline, and neither bar's outline/labels flicker or redraw on ticks where the underlying value is unchanged.

### TC-DISP-003-01: No visible flicker under normal operation
**Traces to:** REQ-NFR-003
**Preconditions:** DUT running normally, room lighting adequate for observation.
**Steps:**
1. Observe the LCD continuously for 2 minutes during normal 1 Hz updates.
2. Separately, video-record 30 s at 60fps and review frame-by-frame for any full-screen blank/redraw flash.

**Expected Result:** No visible flicker to the naked eye. Frame-by-frame review shows no full-screen clear — only the specific changed regions (text fields, bar deltas) update.

## 6. Test Cases — Communication

### TC-COMM-001-01: UART frame is human-readable ASCII text
**Traces to:** REQ-COMM-001
**Preconditions:** DUT running normally.
**Steps:**
1. Capture raw bytes of 10 consecutive frames.
2. Decode as ASCII and visually inspect against the format in `requirements_spec.md` §6.1.

**Expected Result:** All bytes decode cleanly as ASCII. Each frame matches `DATE:YYYY-MM-DD,TIME:HH:MM:SS,TEMP:##.#,LIGHT:###\n` (with optional `_ERR` suffixes per §6.2).

### TC-COMM-002-01: UART transmission is synchronized with acquisition, 1 Hz
**Traces to:** REQ-COMM-002
**Preconditions:** Same setup as TC-ACQ-001-01.
**Steps:** Reuse the TC-ACQ-001-01 capture; this test reuses that data rather than a separate run.

**Expected Result:** Same as TC-ACQ-001-01 — inter-frame interval 1.0 s ± 50 ms, since acquisition and transmission are driven by the same scheduler tick (`design.md` §3.6).

### TC-COMM-003-01: RTC set via usmart succeeds and persists
**Traces to:** REQ-COMM-003
**Preconditions:** UART terminal connected, usmart console accessible.
**Steps:**
1. Issue `set_rtc_time(2026,7,16,14,0,0)` via the usmart console.
2. Observe the UART echo line.
3. Wait 5 s, then check the next few UART DATE/TIME fields.

**Expected Result:** Echo line reads exactly `RTC SET OK: 2026-07-16 14:00:00`. Subsequent UART frames show DATE/TIME advancing correctly from that set point (e.g. `14:00:05` five seconds later).

### TC-COMM-004-01: GUI-issued `SET_TIME:` command sets the RTC
**Traces to:** REQ-COMM-003 (§6.4 reserved GUI channel), `design.md` §3.5
**Preconditions:** DUT running normally, host GUI connected via `SerialReader` (real hardware mode).
**Steps:**
1. In the GUI, select a date/time via the picker and click "Set RTC Time".
2. Observe the UART output.
3. Wait a few seconds, confirm subsequent DATE/TIME fields advance correctly from the set value.

**Expected Result:** Echo line reads `RTC SET OK: <date> <time>` matching what was selected in the GUI, identical in format to the usmart-issued echo (TC-COMM-003-01). Subsequent frames show DATE/TIME advancing correctly.

### TC-COMM-005-01: usmart and data-frame traffic coexist without corruption
**Traces to:** §6.4 UART Channel Sharing
**Preconditions:** DUT running normally (sending 1 Hz frames).
**Steps:**
1. While frames are being received, issue a `set_rtc_time` command via usmart.
2. Capture the full UART stream across this event.

**Expected Result:** The `RTC SET OK: ...` line appears as its own complete, uncorrupted line, not interleaved mid-character with an adjacent DATE/TIME/TEMP/LIGHT frame. All frames immediately before/after remain well-formed per TC-COMM-001-01.

## 7. Test Cases — System Robustness

### TC-SYS-001-01: 24-hour continuous operation soak test
**Traces to:** REQ-SYS-001
**Preconditions:** DUT freshly flashed and reset, UART logging started at t=0.
**Steps:**
1. Run continuously for 24 hours with UART stream logged to a file (host script or terminal session logging).
2. At the end, analyze the log for: gaps >2 s between frames, frame count vs. expected (~86400), any malformed frames, any sign of MCU reset (e.g. RTC time jumping back toward the sentinel default mid-run).

**Expected Result:** Zero gaps >2 s. Frame count within 0.1% of expected. Zero unintended resets (RTC time is monotonically increasing throughout, aside from the deliberate `set_rtc_time` call if any occurs during the window).

### TC-SYS-002-01: Out-of-range light reading clamps and flags
**Traces to:** REQ-SYS-002
**Preconditions:** DUT running normally.
**Steps:**
1. Attempt to force an out-of-range ADC condition on the light channel (e.g. fully occlude sensor with opaque cover, or expose to an extremely bright direct light source, depending on which end of the range is easier to reach with available equipment).
2. Observe UART LIGHT field.

**Expected Result:** LIGHT value clamps to 0 or 100 (never outside range) and carries the `_ERR` suffix (e.g. `LIGHT:0_ERR`) while the condition persists. Reverts to normal, non-suffixed values once the condition is removed.

### TC-SYS-002-02: Out-of-range temperature reading clamps and flags
**Traces to:** REQ-SYS-002
**Preconditions:** DUT running normally.
**Steps:**
1. If a controlled heat source is available (e.g. brief hairdryer exposure, used carefully and briefly to avoid damaging the board), attempt to push the internal temp reading toward the upper bound. If no safe way to reach the boundary exists, this case may be executed via firmware-level fault injection instead (temporarily forcing `acq_read()` to return an out-of-bound raw ADC value) — record which method was used.

**Expected Result:** TEMP clamps to a value within `[-40.0, 85.0]` and carries `_ERR` suffix while out of range, e.g. `TEMP:85.0_ERR`.

### TC-SYS-003-01: Sentinel default time on true first boot
**Traces to:** REQ-SYS-003
**Preconditions:** Backup domain has never been set (fresh board, or backup domain explicitly cleared via debugger).
**Steps:**
1. Power on without ever having called `set_rtc_time`.
2. Read the UART DATE/TIME field.

**Expected Result:** DATE/TIME reads `2000-01-01 00:00:00` (or is actively counting up from that point since boot).

### TC-SYS-004-01: RTC persists across reset/reflash (VBAT held)
**Traces to:** REQ-SYS-004
**Preconditions:** RTC previously set via `set_rtc_time` to a known value; VBAT supply uninterrupted throughout.
**Steps:**
1. Set RTC to a known time.
2. Press the reset button (do not remove power).
3. Reflash firmware via the debugger (do not remove power / VBAT).
4. Check UART DATE/TIME after each step.

**Expected Result:** RTC time is retained and continues advancing correctly across both the reset and the reflash — it does not revert to the sentinel default.

### TC-SYS-004-02: RTC reverts to sentinel on VBAT loss
**Traces to:** REQ-SYS-004
**Preconditions:** RTC previously set to a known time. Requires knowing whether the board has a backup battery installed (see §2).
**Steps:**
1. Set RTC to a known time.
2. Fully remove all power, including VBAT (unplug USB and remove/disconnect the backup battery if one is installed; if VBAT is tied directly to VDD with no battery, simply removing main power is sufficient).
3. Wait several seconds, then reapply power.
4. Check UART DATE/TIME.

**Expected Result:** RTC reads the sentinel default `2000-01-01 00:00:00`, confirming Backup Domain state was lost as expected — this is a pass condition (expected behavior per REQ-SYS-004), not a defect.

## 8. Test Cases — Non-Functional

### TC-NFR-001-01: UART configured at 115200 8N1
**Traces to:** REQ-NFR-001
**Preconditions:** N/A.
**Steps:**
1. Configure the host terminal for 115200 8N1 and confirm the DUT's frames are received cleanly (no garbled characters).
2. As a negative check, configure the terminal at an incorrect baud rate (e.g. 9600) and confirm output is garbled (sanity check that the DUT isn't silently tolerant of the wrong rate, which would suggest a misconfiguration masking itself).

**Expected Result:** Clean output at 115200 8N1; garbled output at the incorrect rate.

### TC-NFR-002-01: Watchdog recovers from a hung main loop
**Traces to:** REQ-NFR-002
**Preconditions:** A debug build with a deliberately injected infinite loop (e.g. behind a compile-time flag) is available for this test only — this modifies firmware temporarily and must not ship in the release build.
**Steps:**
1. Flash the debug build with the injected hang triggered by a specific condition (e.g. a dedicated usmart command `trigger_hang()`).
2. Trigger the hang and start a stopwatch.
3. Observe UART output and time until the DUT resumes normal 1 Hz frames.

**Expected Result:** DUT resets and resumes normal operation within the configured IWDG timeout (≤ 2 s per `design.md` §3.6) plus reset/reinit overhead. RTC time reflects the gap correctly (does not revert to sentinel, per REQ-SYS-004, since this is a reset, not a VBAT loss).

## 9. Pass/Fail Summary Template

| Test ID | REQ-ID | Result | Date | Notes |
|---------|--------|--------|------|-------|
| TC-ACQ-001-01 | REQ-ACQ-001 | | | |
| TC-ACQ-002-01 | REQ-ACQ-002 | | | |
| TC-ACQ-003-01 | REQ-ACQ-003 | | | |
| TC-DISP-001-01 | REQ-DISP-001 | | | |
| TC-DISP-002-01 | REQ-DISP-002 | | | |
| TC-DISP-002-02 | REQ-DISP-002 | | | |
| TC-DISP-003-01 | REQ-NFR-003 | | | |
| TC-COMM-001-01 | REQ-COMM-001 | | | |
| TC-COMM-002-01 | REQ-COMM-002 | | | |
| TC-COMM-003-01 | REQ-COMM-003 | | | |
| TC-COMM-004-01 | REQ-COMM-003 | Deferred | | Requires host GUI |
| TC-COMM-005-01 | §6.4 | | | |
| TC-SYS-001-01 | REQ-SYS-001 | PASS (partial) | 2026-07-23 | Ran ~58 min (not full 24h). First 1h run showed 3 gaps (8s/14s/45s) coinciding with the board being physically handled for photos — invalidated, not attributable to firmware. Second clean run: 3469 frames, 0.03% frame-count deviation, 0 gaps >2s, 0 non-increasing timestamps, 0 sentinel-default readings. Full 24h run not yet executed. |
| TC-SYS-002-01 | REQ-SYS-002 | | | |
| TC-SYS-002-02 | REQ-SYS-002 | | | |
| TC-SYS-003-01 | REQ-SYS-003 | | | |
| TC-SYS-004-01 | REQ-SYS-004 | | | |
| TC-SYS-004-02 | REQ-SYS-004 | | | |
| TC-NFR-001-01 | REQ-NFR-001 | | | |
| TC-NFR-002-01 | REQ-NFR-002 | | | Requires debug build |

## 10. Requirements Traceability Summary

| REQ-ID | Test Case(s) |
|--------|---------------|
| REQ-ACQ-001 | TC-ACQ-001-01 |
| REQ-ACQ-002 | TC-ACQ-002-01 |
| REQ-ACQ-003 | TC-ACQ-003-01 |
| REQ-DISP-001 | TC-DISP-001-01 |
| REQ-DISP-002 | TC-DISP-002-01, TC-DISP-002-02 |
| REQ-COMM-001 | TC-COMM-001-01 |
| REQ-COMM-002 | TC-COMM-002-01 |
| REQ-COMM-003 | TC-COMM-003-01, TC-COMM-004-01 (deferred) |
| REQ-SYS-001 | TC-SYS-001-01 |
| REQ-SYS-002 | TC-SYS-002-01, TC-SYS-002-02 |
| REQ-SYS-003 | TC-SYS-003-01 |
| REQ-SYS-004 | TC-SYS-004-01, TC-SYS-004-02 |
| REQ-NFR-001 | TC-NFR-001-01 |
| REQ-NFR-002 | TC-NFR-002-01 |
| REQ-NFR-003 | TC-DISP-003-01, TC-DISP-002-02 |

Every REQ-ID from `requirements_spec.md` v1.1 has at least one test case above; none are without coverage. `TC-COMM-004-01` is intentionally deferred (host GUI doesn't exist yet) but is tracked, not dropped.

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial baseline: test environment, test case format, full test case set covering every REQ-ID in requirements_spec.md v1.1 (including REQ-SYS-004 and §6.4), pass/fail summary template, and traceability matrix |
| 1.1 | 2026-07-23 | TC-COMM-004-01 updated from deferred/placeholder to a fully executable test case, now that both the host GUI's Set RTC Time control and the firmware's SET_TIME: command parser are implemented |
