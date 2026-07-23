"""
soak_test_verify.py
Verifies a soak-test run against the acceptance criteria in
test_plan.md TC-SYS-001-01:
  - Zero gaps between consecutive frames greater than 2 seconds
  - Frame count within 0.1% of expected (interval_s * duration)
  - RTC time strictly increasing throughout (no reset -- a jump backward
    toward the sentinel default, or any decrease, indicates an
    unintended reset per REQ-SYS-003/004)

Usage:
    python soak_test_verify.py --db environmental_monitoring_system.db \
        --start "2026-07-23 10:00:00" --end "2026-07-23 11:00:00"
"""

import argparse
from datetime import datetime

from storage import Storage


def verify(db_path: str, start: str, end: str, interval_s: float = 1.0) -> None:
    store = Storage(db_path)
    readings = store.query_range(start, end)
    store.close()

    print(f"Queried range: {start} to {end}")
    print(f"Frames found: {len(readings)}\n")

    if len(readings) == 0:
        print("FAIL: no readings found in this range -- check --db path and --start/--end values.")
        return

    # --- expected frame count -------------------------------------------------
    start_dt = datetime.strptime(start, "%Y-%m-%d %H:%M:%S")
    end_dt = datetime.strptime(end, "%Y-%m-%d %H:%M:%S")
    duration_s = (end_dt - start_dt).total_seconds()
    expected_frames = duration_s / interval_s

    frame_count_ok = abs(len(readings) - expected_frames) <= 0.001 * expected_frames
    print(f"Expected ~{expected_frames:.0f} frames (duration {duration_s:.0f}s / {interval_s}s interval)")
    print(f"Frame count check: {'PASS' if frame_count_ok else 'FAIL'} "
          f"({len(readings)} actual vs {expected_frames:.0f} expected, "
          f"{abs(len(readings) - expected_frames) / expected_frames * 100:.2f}% difference)\n")

    # --- gap check + RTC continuity check, in one pass over consecutive pairs --
    timestamps = [
        datetime.strptime(f"{r.date} {r.time}", "%Y-%m-%d %H:%M:%S")
        for r in readings
    ]

    max_gap_s = 0.0
    gaps_over_2s = 0
    non_increasing_count = 0

    for i in range(1, len(timestamps)):
        delta = (timestamps[i] - timestamps[i - 1]).total_seconds()

        if delta > max_gap_s:
            max_gap_s = delta
        if delta > 2.0:
            gaps_over_2s += 1
            print(f"  GAP: {timestamps[i-1]} -> {timestamps[i]}  ({delta:.1f}s)")

        if delta <= 0:
            non_increasing_count += 1
            print(f"  NON-INCREASING TIME: {timestamps[i-1]} -> {timestamps[i]}  "
                  f"(delta {delta:.1f}s -- possible reset per REQ-SYS-003/004)")

    print(f"\nMax gap between consecutive frames: {max_gap_s:.1f}s")
    print(f"Gaps > 2s: {gaps_over_2s}  ({'PASS' if gaps_over_2s == 0 else 'FAIL'})")
    print(f"Non-increasing timestamps (possible resets): {non_increasing_count}  "
          f"({'PASS' if non_increasing_count == 0 else 'FAIL'})")

    # --- sentinel check: did the RTC ever revert to the boot default? ---------
    sentinel_hits = sum(1 for r in readings if r.date == "2000-01-01")
    print(f"Sentinel-default (2000-01-01) readings: {sentinel_hits}  "
          f"({'PASS' if sentinel_hits == 0 else 'FAIL -- RTC reverted to sentinel, indicates reset'})")

    print("\n--- Overall ---")
    overall = frame_count_ok and gaps_over_2s == 0 and non_increasing_count == 0 and sentinel_hits == 0
    print("PASS" if overall else "FAIL -- see details above")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="environmental_monitoring_system.db")
    ap.add_argument("--start", required=True, help='e.g. "2026-07-23 10:00:00"')
    ap.add_argument("--end", required=True, help='e.g. "2026-07-23 11:00:00"')
    ap.add_argument("--interval", type=float, default=1.0, help="expected seconds between frames")
    args = ap.parse_args()

    verify(args.db, args.start, args.end, args.interval)
