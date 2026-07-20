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