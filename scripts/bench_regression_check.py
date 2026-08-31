"""Bench regression gate — compare latest vs baseline detection JSONL.

A1.V4 of the FFT acceleration plan
(docs/shaping/cislunar_nav/fft-acceleration.md).

Compares benchmarks/latest/detections.jsonl against
benchmarks/baseline/detections.jsonl on the (op_id, prn, signal) tuple,
within tolerances:

    - SNR (cn0_db): ±0.5 dB
    - Doppler: ±500 Hz (≈ one bin at the default acquisition grid)
    - Code-phase: ±1 sample — SKIPPED in V4 (Detection schema does not
      carry code_phase yet; will be added when A4 cares about it)

Exit code 0 on pass, 1 on regression. Run via `task bench-regression` or
directly: `python scripts/bench_regression_check.py`.

This is the local-only gate from the slices doc — no CI involvement.
"""

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

SNR_TOLERANCE_DB = 0.5
# A2/A3 are bit-exact in detection list; A4 (freq-domain Doppler shift)
# may move detected Doppler by up to one search bin (~500 Hz at the
# default grid).  500 Hz is generous enough that a real bin slip surfaces
# as `missing` first, and tight enough that two-bin slips still register
# as regressions.
#
# Signal-uniform: 500 Hz comes from the GPS L1 C/A bin spacing
# (~624 Hz at the default integration length) and is conservative for
# Galileo E1B (~244 Hz) and BDS B1C (~244 Hz) — those signals' bin
# slips trip earlier, which is the safe direction.  When A4 lands and
# bin width becomes detection-relevant (R1.3, code-phase tolerance),
# this constant must move into a per-signal lookup keyed on the signal
# prefix; tracked as B1 in
# ``docs/shaping/cislunar_nav/fft-acceleration-slices.md``.
DOPPLER_TOLERANCE_HZ = 500.0


@dataclass(frozen=True)
class DetectionKey:
    """Unique identifier for a detection across runs."""

    op_id: str
    prn: int
    signal: str


@dataclass
class Detection:
    """Loaded detection row + raw dict for downstream tolerance checks."""

    key: DetectionKey
    cn0_db: float
    doppler_hz: float
    raw: dict


def load_jsonl(path: Path) -> dict[DetectionKey, Detection]:
    """Load a flat detection JSONL into a dict keyed by (op_id, prn, signal)."""
    out: dict[DetectionKey, Detection] = {}
    with path.open() as f:
        for line in f:
            stripped = line.strip()
            if not stripped:
                continue
            row = json.loads(stripped)
            key = DetectionKey(
                op_id=row["op_id"],
                prn=int(row["prn"]),
                signal=str(row["signal"]),
            )
            out[key] = Detection(
                key=key,
                cn0_db=float(row["cn0_db"]),
                doppler_hz=float(row["doppler_hz"]),
                raw=row,
            )
    return out


def compare(
    baseline: dict[DetectionKey, Detection],
    latest: dict[DetectionKey, Detection],
) -> tuple[int, list[str]]:
    """Compare detections; return (regression_count, lines_to_print)."""
    base_keys = set(baseline.keys())
    latest_keys = set(latest.keys())

    missing = base_keys - latest_keys
    extra = latest_keys - base_keys
    common = base_keys & latest_keys

    snr_drops: list[tuple[DetectionKey, float, float]] = []
    snr_gains: list[tuple[DetectionKey, float, float]] = []
    doppler_drifts: list[tuple[DetectionKey, float, float]] = []
    for key in common:
        b_cn0 = baseline[key].cn0_db
        cur_cn0 = latest[key].cn0_db
        delta = cur_cn0 - b_cn0
        if delta < -SNR_TOLERANCE_DB:
            snr_drops.append((key, b_cn0, cur_cn0))
        elif delta > SNR_TOLERANCE_DB:
            snr_gains.append((key, b_cn0, cur_cn0))

        b_dop = baseline[key].doppler_hz
        cur_dop = latest[key].doppler_hz
        if abs(cur_dop - b_dop) > DOPPLER_TOLERANCE_HZ:
            doppler_drifts.append((key, b_dop, cur_dop))

    lines: list[str] = []
    lines.append(f"  Missing (baseline → latest): {len(missing)}")
    for k in sorted(missing, key=lambda x: (x.op_id, x.signal, x.prn)):
        b = baseline[k]
        lines.append(
            f"    {k.op_id} / {k.signal}{k.prn} — baseline cn0={b.cn0_db:.1f} dB lost"
        )

    lines.append(f"  SNR drops > {SNR_TOLERANCE_DB} dB: {len(snr_drops)}")
    drops_sorted = sorted(snr_drops, key=lambda x: (x[0].op_id, x[0].signal, x[0].prn))
    for k, b_cn, cur_cn in drops_sorted:
        lines.append(
            f"    {k.op_id} / {k.signal}{k.prn} — "
            f"{b_cn:.1f} → {cur_cn:.1f} dB ({cur_cn - b_cn:+.2f})"
        )

    lines.append(
        f"  Doppler drifts > {DOPPLER_TOLERANCE_HZ:.0f} Hz: {len(doppler_drifts)}"
    )
    drifts_sorted = sorted(
        doppler_drifts, key=lambda x: (x[0].op_id, x[0].signal, x[0].prn)
    )
    for k, b_dop, cur_dop in drifts_sorted:
        lines.append(
            f"    {k.op_id} / {k.signal}{k.prn} — "
            f"{b_dop:+.0f} → {cur_dop:+.0f} Hz ({cur_dop - b_dop:+.0f})"
        )

    lines.append(f"  Extra (new in latest):       {len(extra)}")
    for k in sorted(extra, key=lambda x: (x.op_id, x.signal, x.prn)):
        cur = latest[k]
        lines.append(
            f"    {k.op_id} / {k.signal}{k.prn} — "
            f"new cn0={cur.cn0_db:.1f} dB (improvement)"
        )

    lines.append(f"  SNR gains > {SNR_TOLERANCE_DB} dB: {len(snr_gains)}")
    gains_sorted = sorted(snr_gains, key=lambda x: (x[0].op_id, x[0].signal, x[0].prn))
    for k, b_cn, cur_cn in gains_sorted:
        lines.append(
            f"    {k.op_id} / {k.signal}{k.prn} — "
            f"{b_cn:.1f} → {cur_cn:.1f} dB ({cur_cn - b_cn:+.2f}, improvement)"
        )

    regressions = len(missing) + len(snr_drops) + len(doppler_drifts)
    return regressions, lines


def main() -> int:
    """Bench regression entrypoint — see module docstring."""
    p = argparse.ArgumentParser(description=(__doc__ or "").split("\n", maxsplit=1)[0])
    p.add_argument(
        "--baseline",
        type=Path,
        default=Path("benchmarks/baseline/detections.jsonl"),
        help="Baseline detections JSONL (default: benchmarks/baseline/...)",
    )
    p.add_argument(
        "--latest",
        type=Path,
        default=Path("benchmarks/latest/detections.jsonl"),
        help="Latest detections JSONL (default: benchmarks/latest/detections.jsonl)",
    )
    args = p.parse_args()

    if not args.baseline.exists():
        print(f"baseline missing: {args.baseline}")
        print("  → run task bench-baseline-update first")
        return 1
    if not args.latest.exists():
        print(f"latest missing: {args.latest} — run task bench-campaign first")
        return 1

    baseline = load_jsonl(args.baseline)
    latest = load_jsonl(args.latest)

    print(f"Regression check: {args.latest} vs {args.baseline}")
    print(f"  baseline detections: {len(baseline)}, latest detections: {len(latest)}")

    regressions, lines = compare(baseline, latest)
    for line in lines:
        print(line)

    if regressions == 0:
        print("  PASS")
        return 0
    print(f"  FAIL — {regressions} regression(s)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
