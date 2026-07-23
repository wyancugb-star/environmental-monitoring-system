# Host-Side Design Document
## STM32 Environmental Monitoring System — Python Host Application

**Document Version:** 1.1
**Date:** 2026-07-23 (updated from 2026-07-16 baseline)
**Author:** Yan
**Status:** Baseline
**Parent Document:** `requirements_spec.md` v1.1 §6 (UART data format is the interface contract this entire document builds against). Firmware-side `design.md` is a sibling document, not a dependency — this document only needs the wire format, not firmware internals.

---

## 1. Purpose & Scope

This document designs the host-side Python application: serial acquisition, local storage, a PyQt5 GUI, and a pytest-based automated test suite. It covers `host/` only. Firmware is out of scope here (see `design.md`).

## 2. Architectural Overview

```
                    ┌───────────────────────────────────────┐
                    │              main.py                   │
                    │   (wires modules together, starts GUI) │
                    └───┬───────┬────────┬─────────┬─────────┘
                        │       │        │         │
                 ┌──────┘  ┌────┘   ┌────┘    ┌────┘
                 ▼         ▼        ▼         ▼
           ┌──────────┐┌────────┐┌───────┐┌─────────┐
           │  serial  ││ parser ││storage││   gui   │
           │  reader  ││        ││       ││ (PyQt5) │
           └──────────┘└────────┘└───────┘└─────────┘
                                                │
                                          ┌─────┘
                                          ▼
                                     ┌─────────┐
                                     │ pandas /│
                                     │matplotlib│
                                     │(analysis)│
                                     └─────────┘
```

`serial_reader` and `parser` can be driven by either a **real serial port** or a **simulated data source** (§6), so GUI/storage/test development doesn't block on firmware being ready.

## 3. Module Breakdown

### 3.1 `serial_reader` — Data Source Abstraction

**Responsibility:** produce a stream of raw text lines, regardless of whether they come from a real UART or a simulator.

```python
class SerialReader:
    """Reads lines from a real serial port via pyserial. Runs in its own QThread so it never blocks the GUI event loop."""
    def __init__(self, port: str, baudrate: int = 115200): ...
    def read_line(self) -> str: ...   # blocks until a line is available or a timeout elapses
    def write_line(self, text: str) -> None: ...   # sends a command over UART, e.g. the GUI's SET_TIME: (§6.4)

class SimulatedReader:
    """Generates lines matching requirements_spec.md §6.1/§6.2 at a configurable interval, for development without hardware."""
    def __init__(self, interval_s: float = 1.0, inject_errors: bool = False): ...
    def read_line(self) -> str: ...
    def write_line(self, text: str) -> None: ...   # no real device to send to; prints what would have been sent
```

Both implement the same `read_line()` interface, so `parser` and everything downstream is agnostic to which one is in use. This is the mechanism that lets host development proceed in parallel with firmware (per our earlier discussion) — swap `SerialReader` for `SimulatedReader` in `main.py` and nothing else changes.

- **Both classes also implement `write_line(text)`**, used by the GUI's "Set RTC Time" control (§3.4) to send `SET_TIME:` commands. `SerialReader.write_line()` writes bytes to the real port; `SimulatedReader.write_line()` has no device to send to, so it just prints what would have been sent — this keeps the two classes' interfaces symmetric (read *and* write), not just read-only as originally scoped.

- **`SimulatedReader` generates data via a small random walk each call** (temp drifts by roughly ±0.3°C, light by roughly ±5%, both starting from a fixed baseline), rather than picking arbitrary values. After drifting, the resulting value is checked against the same boundaries the firmware uses (temp: -40.0 to 85.0°C; light: 0 to 100%) — if drifting would push the value past a boundary, it's clamped back to the boundary and flagged with `_ERR`, mirroring the firmware's `acq_read()` clamp logic (REQ-SYS-002) rather than an arbitrary simulated fault.
- **This clamp/flag behavior is intentionally independent of `inject_errors`** — it reflects a property of the data itself (did this value need clamping), not an artificially injected fault, so it can occur even with `inject_errors=False` (rarely, since the random walk needs many steps to reach a boundary from the baseline). Landing exactly on a boundary via normal drift (e.g. light reaches exactly 100 without overshooting) does **not** count as clamped and does not get `_ERR` — only values that would have gone past the boundary do.
- **`inject_errors=True` controls a separate, independent behavior**: roughly a 10% chance per call of emitting a non-data line (a simulated usmart echo, e.g. `"RTC SET OK: ..."`) instead of a data frame, exercising the parser's ability to skip non-frame traffic (§6.4). It does not control the clamp/`_ERR` behavior above.

### 3.2 `parser` — Frame Parsing & Validation

**Responsibility:** turn a raw line into a structured reading, or correctly recognize and discard non-data lines.

```python
@dataclass
class Reading:
    date: str          # "YYYY-MM-DD"
    time: str           # "HH:MM:SS"
    temp_c: float
    temp_err: bool
    light_pct: int
    light_err: bool

def parse_line(line: str) -> Reading | None:
    """Splits the line on commas, then each field on its first colon, building a
    dict keyed by field name (DATE/TIME/TEMP/LIGHT). Returns a Reading only if all
    four required keys are present and TEMP/LIGHT convert cleanly to float/int;
    otherwise returns None (missing fields, non-numeric values, or non-data lines
    like usmart echoes, per §6.4, are all treated as expected non-matches, not
    errors -- caught via a targeted except ValueError around the conversion step,
    not a bare except, so a real bug in the parser itself isn't silently masked)."""
```

- Uses string `split(',')` then `split(':', 1)` per field, not a regex -- more verbose but each step is individually inspectable/debuggable, which mattered more here than regex's compactness given this module was hand-written end-to-end rather than generated.
- Missing-field detection checks all four required keys are present (`all(key in data for key in required_keys)`) before proceeding; malformed numeric values (e.g. `TEMP:abc`) are caught via `ValueError` on the `float()`/`int()` conversion. Both paths return `None`, matching the "non-data line" contract in §6.4.
- `_ERR` suffix detection strips the suffix and sets the corresponding `*_err` flag before converting to `float`/`int`, so downstream code always works with clean numeric types plus a separate boolean flag — mirrors the `SensorData_t` split on the firmware side (`design.md` §3.1), keeping the two sides' data models conceptually aligned even though they're different languages.

### 3.3 `storage` — Local Persistence

**Responsibility:** append parsed readings to a durable local store for later analysis.

```python
class Storage:
    def __init__(self, db_path: str = "readings.db"): ...
    def append(self, reading: Reading) -> None: ...
    def query_range(self, start: str, end: str) -> list[Reading]: ...
    def close(self) -> None: ...
```

- **Format decision: SQLite**, not CSV. Rationale: `query_range()` needs indexed, timestamp-based lookups for the GUI's live chart and for pandas analysis; SQLite gives that without hand-rolling file-seeking logic, and it's a single file that's trivial to commit an example of to the repo for demo purposes. CSV export is still offered as a one-way output for sharing/inspection (§3.5), just not as the primary store.
- Each `append()` call is a single transaction — no batching — since writes only happen once per second (per REQ-COMM-002/REQ-ACQ-001) and durability matters more than write throughput here.
- The `timestamp` column is built by concatenating `reading.date + " " + reading.time` at **write time** (inside `append()`), not computed at query time — this lets plain string comparison (`>=`/`<=` in the `WHERE` clause) correctly reflect chronological order without a real date/time comparison, and lets the column be indexed. This depends on the fixed `"YYYY-MM-DD HH:MM:SS"` format; passing a differently-formatted string to `query_range(start, end)` will not raise an error -- it will silently return wrong or empty results, since the comparison has no awareness of what a "date" is, only of string ordering.
- Table schema uses `IF NOT EXISTS` so repeated `Storage(db_path)` calls against an existing file don't error and don't clear prior data — existing rows are preserved across runs, which is the intended persistence behavior, not an accident.

### 3.4 `gui` — PyQt5 Application

**Responsibility:** live display of current readings, historical chart, pass/fail-style status coloring, and the RTC-set control (§6.4 GUI channel).

Reused patterns from prior serial-tool GUI work in this codebase: QThread-based serial reading (keeps UI responsive), color-coded status (green/red for in-range vs. `_ERR`-flagged readings), progress bar widgets mirroring the firmware LCD's light/temp bars (`design.md` §3.3) for visual consistency between device and host views.

**Key widgets:**
- Live numeric readout (DATE/TIME/TEMP/LIGHT), with `_ERR` fields rendered in red.
- Rolling line chart (last N minutes) via `matplotlib` embedded in a Qt canvas, or `pyqtgraph` if smoother real-time updates are needed — **decision deferred**, see §7.
- **"Set RTC Time" control**: a `QDateTimeEdit` + `QPushButton`, enabled only while a reader thread is running (mirrors the Start/Stop state machine — the button is disabled until Start is clicked, and re-disabled on Stop, since there's nothing to send to otherwise). On click, formats the selected date/time into `SET_TIME:YYYY-MM-DD,HH:MM:SS\n` and calls `reader.write_line(...)`. Both `SerialReader` and `SimulatedReader` implement `write_line()` — the simulated version just prints what would have been sent, so the button gives visible feedback in development without a real device attached.

### 3.5 `analysis` — pandas/matplotlib Offline Analysis

**Responsibility:** load a `Storage` export or the live SQLite file into a pandas DataFrame for post-hoc analysis (e.g. plotting the 24-hour soak test results from `test_plan.md` TC-SYS-001-01).

```python
def load_as_dataframe(storage: Storage, start=None, end=None) -> pd.DataFrame: ...
def export_csv(df: pd.DataFrame, path: str) -> None: ...
```

This is deliberately a separate module from `gui` — the GUI needs live, incremental updates; analysis needs bulk, retrospective queries. Conflating them would force one module to serve two different access patterns poorly.

## 4. `tests/` — pytest Suite

Structure mirrors `test_plan.md`'s test-case IDs so there's a direct, greppable link between a `test_plan.md` row and a pytest function:

```
tests/
├── test_parser.py       # TC-COMM-001-01 style checks, but exercising parse_line() directly with crafted strings
├── test_storage.py       # append/query round-trip correctness
├── test_simulated_reader.py   # SimulatedReader produces spec-conformant lines, including _ERR injection
└── conftest.py            # shared fixtures: temp SQLite db, sample Reading objects
```

- **`SimulatedReader` is the backbone of the automated test suite** — it lets `test_parser.py` and `test_storage.py` run in CI (or just without hardware attached) by feeding known-good and deliberately-malformed lines and asserting on the parsed result, including the boundary/clamp cases from `test_plan.md` §7 (TC-SYS-002-01/02) without needing to physically shade a sensor.
- Hardware-in-the-loop tests (actually reading a real serial port) are a separate, explicitly marked pytest group (`@pytest.mark.hardware`) so the default `pytest` run doesn't require a board to be connected — this keeps the fast unit-test suite usable in any environment, with hardware tests opt-in.

## 5. Data Flow Summary

```
SerialReader / SimulatedReader (QThread)
   │  raw line
   ▼
parse_line()
   │  Reading | None
   ▼ (if Reading)
   ├──→ storage.append()          → SQLite
   └──→ GUI live-update signal    → numeric readout, bars, chart

GUI "Set RTC Time" button
   └──→ serial write "SET_TIME:...\n" → device (firmware side, §6.4)
```

## 6. Development Without Hardware

Per our earlier discussion on parallelizing firmware and host work: `main.py` selects `SerialReader` vs. `SimulatedReader` via a config flag or CLI arg (e.g. `--simulate`). This is the mechanism, stated concretely, that lets `gui`, `storage`, and the pytest suite all be built and verified before the firmware side is flashable — the only shared contract is the wire format in `requirements_spec.md` §6, which is already frozen at v1.1.

## 7. Open Implementation Details (to confirm during coding)

- `matplotlib` vs. `pyqtgraph` for the live chart — matplotlib is more familiar from prior data-analysis work, pyqtgraph is smoother for real-time 1 Hz updates over long sessions (e.g. the 24h soak test). Leaning matplotlib initially for consistency with `analysis.py`, revisit if update smoothness becomes a visible problem.
- SQLite schema specifics (single `readings` table with columns matching `Reading`, vs. splitting `_err` flags into a separate table) — leaning single flat table, simplest for the query patterns in §3.3.

## 8. Requirements Traceability

| REQ-ID | Host Design Element(s) |
|--------|--------------------------|
| REQ-COMM-001 | §3.2 `parse_line()` |
| REQ-COMM-002 | §3.3 `storage.append()` (one write per received frame) |
| REQ-COMM-003 (GUI channel) | §3.4 "Set RTC Time" control |
| REQ-SYS-002 (`_ERR` handling) | §3.2 `_err` flag parsing, §3.4 red-highlighted display |
| §6.1 UART format | §3.2 `parse_line()` split-based field parsing |
| §6.4 UART channel sharing | §3.2 required-field check (non-data lines silently skipped, no exception) |

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-07-20 | Initial baseline: module breakdown (serial_reader, parser, storage, gui, analysis), SimulatedReader as the hardware-independent development/testing mechanism, pytest suite structure, and requirements traceability. Reflects the actual hand-written implementation: split-based (not regex) parsing, SimulatedReader's random-walk-plus-real-clamp behavior (decoupled from `inject_errors`), and write-time timestamp concatenation in storage. |
| 1.1 | 2026-07-23 | §3.4 "Set RTC Time" control updated from design intent to actual implementation: QDateTimeEdit + QPushButton, enabled only while a reader thread is running. §3.1 updated to show write_line() on both SerialReader and SimulatedReader (previously only read_line() was documented, missed when write_line() was first implemented). |
