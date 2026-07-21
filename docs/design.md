# Design Document
## Environmental Monitoring System

**Document Version:** 1.1
**Date:** 2026-07-16
**Author:** Yan
**Status:** Baseline
**Parent Document:** `requirements_spec.md` v1.0 — every design element below traces back to a REQ-ID from that document; see §8 for the full mapping.

---

## 1. Purpose & Scope

This document translates the requirements in `requirements_spec.md` into a concrete firmware architecture: module boundaries, data structures, function interfaces, and control flow. It covers the **firmware side only** (`firmware/`). The host-side Python design (storage/GUI/tests) is out of scope here and will be covered in a separate `host_design.md` when that work starts.

## 2. Architectural Overview

The firmware is a single-threaded, super-loop design (no RTOS) — consistent with the existing `01-uart-iwdg` / `02-timer-pwm-capture` / `03-lcd-fsmc-usmart` projects in this codebase. Six modules sit around a thin main loop:

```
                    ┌─────────────────────────────────────────┐
                    │                main.c                   │
                    │   (1s scheduler via HAL_GetTick, IWDG)   │
                    └───┬───────┬───────┬───────┬───────┬─────┘
                        │       │       │       │       │
                 ┌──────┘  ┌────┘  ┌────┘  ┌────┘  ┌────┘
                 ▼         ▼       ▼       ▼       ▼
           ┌─────────┐┌────────┐┌──────┐┌──────┐┌───────┐
           │ acq_adc ││ rtc_mgr││ lcd  ││ uart ││ usmart│
           │ (light +││(RTC +  ││ disp ││ tx   ││(RTC   │
           │ int temp││sentinel││      ││      ││ set   │
           │ + clamp)││ flag)  ││      ││      ││ cmd)  │
           └─────────┘└────────┘└──────┘└──────┘└───────┘
```

Each box is a `.c`/`.h` pair under `firmware/Core/App/` (new subfolder, sits alongside the CubeMX-generated `Core/Src`, `Core/Inc`). CubeMX-generated HAL init code stays untouched; application logic lives in `App/`.

## 3. Module Breakdown

### 3.1 `acq_adc` — Acquisition Module

**Responsibility:** read raw ADC channels for light + internal temperature, convert to engineering units, apply clamping (REQ-SYS-002).

```c
/**
 * @brief  Sensor reading, post-conversion, post-clamp.
 */
typedef struct {
    float temp_c;       ///< Temperature in °C, 1 decimal precision
    uint8_t temp_err;    ///< 1 if temp was clamped (REQ-SYS-002), else 0
    uint8_t light_pct;   ///< Light intensity 0-100%
    uint8_t light_err;   ///< 1 if light was clamped, else 0
} SensorData_t;

/// @brief Read + convert + clamp both channels in one call. Blocking, ADC in polling mode is fine at 1 Hz.
SensorData_t acq_read(void);
```

- Internal temperature conversion formula and typical constants are per `requirements_spec.md` §9 (`V_25 ≈ 0.76V`, `Avg_Slope ≈ 2.5mV/°C`).
- Clamp bounds: temp `[-40.0, 85.0]`, light `[0, 100]`. Clamping logic lives inside `acq_read()`, not in the caller — keeps the "what counts as out-of-range" decision in one place, per REQ-SYS-002.
- Traces to: REQ-ACQ-001, REQ-ACQ-002, REQ-ACQ-003, REQ-SYS-002.

### 3.2 `rtc_mgr` — RTC Management Module

**Responsibility:** RTC init with sentinel-default detection, formatted read for LCD/UART, and the `set_rtc_time()` entry point exposed to usmart.

```c
/// @brief Init RTC; if backup-domain flag is unset, write sentinel 2000-01-01 00:00:00 (REQ-SYS-003).
void rtc_mgr_init(void);

/// @brief Get current RTC time as separate date/time strings for display/UART.
void rtc_mgr_get(char *date_out, char *time_out); // date_out: "YYYY-MM-DD", time_out: "HH:MM:SS", both caller-allocated, min 11 and 9 bytes respectively

/**
 * @brief  Set RTC time. Registered with usmart so it's callable over UART (REQ-COMM-003).
 * @note   Prints "RTC SET OK: <date> <time>" over UART on success (§6.3).
 */
void set_rtc_time(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
```

- The backup-domain sentinel flag reuses the standard `RTC_ReadBackupRegister`/`RTC_WriteBackupRegister(BKP_DR1, 0xA5A5)` pattern to distinguish "never set" from "set, but happens to read as epoch."
- Traces to: REQ-DISP-001, REQ-COMM-003, REQ-SYS-003.

- **Persistence mechanism:** the sentinel flag and RTC registers live in the Backup Domain, which is powered by VBAT independently of VDD. This is why `rtc_mgr_init()`'s sentinel check works correctly across reset/reflash — Backup Domain state survives them as long as VBAT stays powered. It does *not* survive a full VBAT loss (no backup battery installed, or VBAT tied to VDD) — that case is functionally identical to true first-boot and is handled by the same sentinel-check code path, no special-casing needed. Traces to: REQ-SYS-004.

### 3.3 `lcd_disp` — Display Module

**Responsibility:** render RTC time + latest sensor reading to the 4.3" NT35510 panel via FSMC, without visible flicker (REQ-NFR-003).

```c
/// @brief One-time LCD init: driver init, static bar outlines + min/max labels drawn once.
void lcd_disp_init(void);

/// @brief Redraw only the fields that changed since last call (partial refresh) — this is what prevents flicker.
void lcd_disp_update(const char *date, const char *time, const SensorData_t *data);
```

- **Anti-flicker design decision:** `lcd_disp_update()` does **not** clear-and-redraw the whole screen every second. Static content (bar outlines, min/max axis labels) is drawn once in `lcd_disp_init()`, never repainted. Per-field text and bar fills track their previously-drawn state internally and only repaint the delta. A full-screen clear at 1 Hz on a parallel FSMC LCD is the single most common cause of visible flicker in this class of project — worth calling out explicitly since it's a direct, non-obvious link back to REQ-NFR-003.

**Custom large-font text rendering (date/time/temp/light):**

The driver's built-in ASCII bitmap font tops out at size 24, too small for a clearly-readable display. Date, time, temperature, and light are rendered with a custom glyph set (`Font_48`/`Font_72`, generated by LCD-Image-Converter from a system font, covering only the characters actually needed: `0-9 : . - %` plus one calibration/degree symbol) instead.

```c
/// @brief Look up a character's glyph in a tFont table (Font_48 or Font_72).
static const tImage *font_find_glyph(const tFont *font, char ch);

/// @brief Draw one glyph, clearing its full bounding box to background first.
static void draw_glyph(uint16_t x, uint16_t y, const tImage *img, uint16_t color);

/// @brief Draw a string once, no delta-tracking -- for static content (bar min/max labels).
static void draw_string_once(uint16_t x, uint16_t y, const char *str, uint16_t color, const tFont *font);

/// @brief Draw a string, redrawing only character positions that changed since
///        the last call for this state instance -- used for date/time/temp/light,
///        which change every tick but usually only in a few character positions.
static void draw_string_delta(uint16_t x, uint16_t y, const char *str, uint16_t color,
                               BigStringState_t *state, const tFont *font);
```

- These helpers are private to `lcd_disp.c` (not exposed in `lcd_disp.h`) since this module is their only consumer.
- `draw_glyph()` unconditionally clears its glyph's bounding box before painting -- a narrower new character must not leave remnants of a wider old one. See `debug_log.md` #003.
- `draw_string_delta()` keeps a per-field `BigStringState_t` (one static instance each for date/time/temp/light) so each field's delta-redraw is independent -- changing the temperature display doesn't force a redraw of the time display.
- Date uses `Font_48`; time, temperature, and light use `Font_72` (larger, since these are the primary at-a-glance values).

**Progress bars (light and temperature):**

```c
/// @brief Draw/update a horizontal bar's fill, repainting only the delta segment.
static void draw_bar_delta(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            uint8_t new_pct, uint8_t *last_pct, uint16_t fg_color);
```

- A single shared bar-fill function is reused for both readings -- only the input value and color differ, not the drawing logic.
- **Light:** `light_pct` is already 0-100%, passed directly.
- **Temperature:** normalized from `[-10°C, 50°C]` (the ambient range assumed in `requirements_spec.md` §7.1) to `[0,100]` before calling `draw_bar_delta()`.
- **Fill color is fixed per bar, not threshold-dependent.** An earlier design considered color-coding the temperature bar by range (blue/green/red), but this was dropped in favor of a single fixed color for both bars -- simpler, and the numeric temperature display already conveys the value precisely.
- Each bar has a black outline and static min/max axis labels (e.g. "-10"/"50" for temperature, "0"/"100" for light), drawn once in `lcd_disp_init()` since they never change.
- Same partial-redraw principle applies: only the delta region of the fill (grown/shrunk segment) is repainted each tick.
- Traces to: REQ-DISP-001, REQ-DISP-002, REQ-NFR-003.

### 3.4 `uart_tx` — Communication Module

**Responsibility:** format and transmit the 1 Hz data frame (REQ-COMM-001/002), including the `_ERR` suffix convention (REQ-SYS-002).

```c
/// @brief Format + transmit one data frame per the spec §6.1/§6.2. Non-blocking (uses HAL UART TX, not polling-wait).
void uart_tx_frame(const char *date, const char *time, const SensorData_t *data);
```

- Frame format matches `requirements_spec.md` §6.1 exactly: `DATE:...,TIME:...,TEMP:...,LIGHT:...\n`, with `_ERR` appended to a field's value when that field's `*_err` flag is set (§6.2).
- Traces to: REQ-COMM-001, REQ-COMM-002, REQ-NFR-001 (baud/framing is a CubeMX config item, not code, but this module is where it manifests).

### 3.5 `usmart` — Debug Console (existing, reused) + Reserved GUI Command Path

usmart remains the primary/debug interface for `set_rtc_time()` in v1.0 (existing integration from `03-lcd-fsmc-usmart`, extended by registering `set_rtc_time()` as a callable command). A separate lightweight text-command path (e.g. `SET_TIME:YYYY-MM-DD,HH:MM:SS\n` parsed in the main loop) is reserved for when host-side GUI development begins, so the GUI doesn't need to speak usmart's call protocol. Both can coexist on USART1 (§6.4) since they parse RX independently and call the same underlying `set_rtc_time()`. Traces to: REQ-COMM-003.

### 3.6 `main.c` — Scheduler & Watchdog

**Responsibility:** the 1-second polling loop and IWDG feeding.

```c
uint32_t last_tick = 0;

while (1) {
    HAL_IWDG_Refresh(&hiwdg);          // feed watchdog every loop iteration, not just every 1s (REQ-NFR-002)

    if (HAL_GetTick() - last_tick >= 1000) {
        last_tick = HAL_GetTick();

        SensorData_t data = acq_read();
        char date[11], time[9];
        rtc_mgr_get(date, time);

        lcd_disp_update(date, time, &data);
        uart_tx_frame(date, time, &data);
    }

    usmart_scan();                      // usmart command polling — must NOT block (see §4 pitfall)
}
```

- **IWDG feed placement is deliberate:** it's fed every loop iteration (fast, unconditional), not inside the `if` block. If it were only fed once per second, a slow blocking call anywhere in `usmart_scan()` or the display/UART path could stall the loop for slightly over a second and undermine the entire point of the watchdog. This directly resolves the "interrupt-blocking caused a watchdog reset" pitfall you hit in `01-uart-iwdg` — same category of bug, being designed out this time instead of debugged after the fact.
- `now - last_tick >= 1000` (not `==`) — a single missed check due to a slightly-late loop iteration won't cause the whole 1 Hz cadence to drift or stall (see `requirements_spec.md` §9).
- Traces to: REQ-ACQ-001, REQ-COMM-002, REQ-NFR-002.

## 4. Known Pitfall Carried Forward

From `01-uart-iwdg`: a blocking call inside an interrupt handler stalled the main loop long enough to trip the watchdog. The equivalent risk here is `usmart_scan()` or any UART receive-wait blocking the loop for >1s. Design mitigation: usmart's scan function must be called in **polling mode inside the loop**, never inside a UART RX interrupt handler that blocks waiting for a full command line. This is a design constraint worth stating explicitly rather than discovering it again during debug.

## 5. Data Flow Summary

```
main loop tick (1 Hz)
   │
   ├─→ acq_read()          → SensorData_t (with clamp + _err flags applied)
   ├─→ rtc_mgr_get()        → date/time strings
   ├─→ lcd_disp_update()    → partial-redraw to panel
   └─→ uart_tx_frame()      → text frame out USART1

usmart_scan() (every loop iteration, non-blocking)
   └─→ on "set_rtc_time" command → rtc_mgr internal write → echo "RTC SET OK: ..."
```

## 6. File/Folder Layout (Firmware)

```
firmware/
├── Core/                  (CubeMX-generated, untouched)
├── Drivers/                (CubeMX-generated, untouched)
├── App/                     (new — hand-written application code)
│   ├── acq_adc.c / .h
│   ├── rtc_mgr.c / .h
│   ├── lcd_disp.c / .h
│   ├── uart_tx.c / .h
│   └── app_config.h        (clamp bounds, ADC conversion constants — single source of truth)
├── environmental-monitoring-system.ioc
└── CMakeLists.txt          (updated to include App/*.c in the build)
```

`app_config.h` centralizes the numeric constants from `requirements_spec.md` §9 (V_25, Avg_Slope, clamp bounds, sentinel date) so they exist in exactly one place rather than being duplicated across modules — a `test_plan.md` boundary-value test can target this file directly.

## 7. Open Implementation Details (to confirm during coding, not blocking design sign-off)

- Exact ADC channel/GPIO for the light sensor — pending CubeMX/schematic check (per `requirements_spec.md` §4).
- Whether `uart_tx_frame()` uses `HAL_UART_Transmit` (blocking, short frame so acceptable at 1 Hz) or `HAL_UART_Transmit_IT` (non-blocking) — leaning blocking since the frame is ~50 bytes at 115200 baud (<0.5 ms), negligible against the 1 s budget, but flagging so it's a conscious choice, not a default.

## 8. Requirements Traceability

| REQ-ID | Design Element(s) |
|--------|--------------------|
| REQ-ACQ-001 | §3.6 main loop 1 Hz tick calling `acq_read()` |
| REQ-ACQ-002 | §3.1 `acq_adc` temp conversion |
| REQ-ACQ-003 | §3.1 `acq_adc` light conversion |
| REQ-DISP-001 | §3.2 `rtc_mgr_get()`, §3.3 `lcd_disp_update()` |
| REQ-DISP-002 | §3.3 `lcd_disp_update()` |
| REQ-COMM-001 | §3.4 `uart_tx_frame()` |
| REQ-COMM-002 | §3.6 main loop 1 Hz tick calling `uart_tx_frame()` |
| REQ-COMM-003 | §3.2 `set_rtc_time()`, §3.5 usmart registration |
| REQ-SYS-001 | §3.6 IWDG feed placement, §4 pitfall mitigation |
| REQ-SYS-002 | §3.1 clamp logic in `acq_read()`, §3.4 `_ERR` suffix in `uart_tx_frame()` |
| REQ-SYS-003 | §3.2 `rtc_mgr_init()` sentinel logic |
| REQ-NFR-001 | §3.4 (CubeMX USART1 config, not code) |
| REQ-NFR-002 | §3.6 IWDG feed every loop iteration |
| REQ-NFR-003 | §3.3 partial-redraw anti-flicker design |
| REQ-SYS-004 | §3.2 `rtc_mgr_init()` sentinel check (same code path handles both true first-boot and VBAT-loss cases) |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-16 | Initial baseline: module breakdown, interfaces, main loop scheduling, IWDG placement rationale, anti-flicker LCD design, file layout, and full requirements traceability |
