# Debug Log
## STM32 Environmental Monitoring System

---

## #001 — usmart command deadlock in UART RX interrupt context

**Date:** 2026-07-20
**Symptom:** Calling `light_adc_raw_print()` via usmart caused a full
system lock — LCD stopped refreshing, no further UART output.

**Root Cause:** `usmart_scan()` was invoked directly inside
`HAL_UART_RxCpltCallback()`. `light_adc_raw_print()` calls `HAL_Delay()`,
which spins on `HAL_GetTick()` — only advanced by the SysTick interrupt.
SysTick couldn't preempt while still inside the UART RX ISR, so the wait
condition never became true. Permanent deadlock.

**Fix:** UART RX callback now only sets a flag; `usmart_scan()` runs
from the main loop instead (normal thread context, SysTick can preempt
normally).

**Related:** Anticipated in `design.md` §4 (Known Pitfall Carried
Forward) before this bug occurred — design doc predicted both the
mechanism and the fix correctly.

## #002 — sprintf("%.1f", ...) produced no output on LCD

**Date:** 2026-07-20
**Symptom:** Temperature field silently failed to display on LCD after
switching from LCD_ShowxNum to sprintf+LCD_ShowString (needed for
float/negative value support). No crash, no error, just missing text.

**Root Cause:** newlib-nano (the default reduced C library linked by
arm-none-eabi-gcc for STM32 projects) excludes floating-point format
specifier support (%f, %.1f, etc.) by default to save flash space.
sprintf() silently failed to produce the expected characters.

**Fix:** Added `-u _printf_float` to the linker options in
CMakeLists.txt, forcing the linker to include newlib's float-capable
printf/sprintf implementation. Required a full clean rebuild, not
just incremental, since this is a link-stage change.

**Note:** Ruled out UART/波特率 and VS Code debugger artifacts first
before finding the real cause -- see troubleshooting steps in this
session for reference.