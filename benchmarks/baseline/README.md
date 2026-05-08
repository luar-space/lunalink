# Benchmark Baseline

Committed reference measurements for the FFT-acceleration work described in
[`docs/shaping/cislunar_nav/fft-acceleration.md`](../../docs/shaping/cislunar_nav/fft-acceleration.md).

## Purpose

Every acceleration slice (A2-A5 in the shaping doc) is evaluated against
these files.  The regression gate in `scripts/bench_regression_check.py`
compares `benchmarks/latest/detections.jsonl` against
`benchmarks/baseline/detections.jsonl` within tolerances:

| Check | Tolerance |
|-------|-----------|
| Detection list (PRN × op_id × signal) | exact match |
| SNR per detection | ±0.5 dB |
| Code-phase per detection | ±1 sample |

`*.csv` files track wall-clock and FFT counts for speedup ratios; the
regression gate is bit-exact on the CSV columns where the slice is meant
to be invariant.

## Files

| File | Produced by | What it holds |
|------|-------------|---------------|
| `campaign_bench.csv` | `task bench-campaign` | per-recording wall-clock + FFT counts + detection count for the canonical subset |
| `detections.jsonl` | `task bench-campaign` | flat JSONL — one detection per line — from the same run |
| `hw_fingerprint.json` | `scripts/write_hw_fingerprint.sh` | chip, perf/eff core counts, macOS, compiler identity + version, CMake version + flags + build type, commit SHA, and capture timestamp — full provenance for the baseline run |
| `fft_bench.csv` | `task bench-fft` | ns/FFT × size × backend × thread mode |
| `acquisition_bench.csv` | `task bench-acq` | µs/bin and µs/acquire per variant |

## How to refresh the baseline

1. Check out a branch representing the new reference point (usually `main`
   after a deliberate speedup merge).
2. `task bench-campaign` — produces `benchmarks/latest/*`.
3. `task bench-baseline-update` — copies `latest/*` over `baseline/*`.
4. `git diff benchmarks/baseline/` — sanity-check the delta.
5. Update this README's "Baseline captured from" section below.
6. Commit.

**Never** refresh the baseline on the branch that's meant to be evaluated
against it — that hides the exact speedup you were trying to measure.

## Baseline captured from

- Commit (of `worktree-fft-acceleration` at the time the CSVs in this
  directory were captured): see `hw_fingerprint.json` for exact
  `commit_sha`.  The committed fingerprint is regenerated at every
  baseline-update via `task bench-baseline-update`, so it always
  matches the CSVs beside it — if a future operator promotes new CSVs
  without re-running the fingerprint script, the SHA mismatch is the
  audit trail.
- Backend layout:
  - Tier 1 (flight, RTEMS / RV64GC) — **scalar Cooley-Tukey**, no
    cache, no atomic counters.  Structurally unaffected by A2 / A3.
  - Tier 2/3 (CI / dev) — **`vdsp_dft`** (A2) + **rx-FFT cache** (A3,
    bench-only behind `LUNALINK_DEV_RX_FFT_CACHE`).  Numbers below
    reflect this configuration.
- **A2 → A3 cumulative speedup vs A1 baseline: 1.91× campaign wall-clock** (148.19 s → 77.46 s, bit-exact detection list)
- Headline: 77.46 s wall-clock for 3 recordings; **94,584** forward FFTs
  (the A1 baseline had ~1.99M forward FFTs over the same workload —
  see the A1.V1 commit for the captured CSV); 1,987,290 inverse FFTs;
  7 detections.  The "95% reduction" framing is approximate and the
  precise A1-baseline forward count is in the A1.V1 commit's CSV.
- Per-recording (variant=`baseline`, `--shared-rx-fft` enabled):
  - `OP21_0` (L1, 600 ms) — 42.51 s, 47,286 fwd / 988,245 inv FFTs, 3 dets (G24, E11, S131)
  - `OP17_0` (L1, 400 ms) — 34.82 s, 47,298 fwd / 999,045 inv FFTs, 4 dets (G11, G30, S144, S134)
  - `OP9_0` (L5, 200 ms) — 0.12 s, 0 FFTs, 0 dets *(L5 dispatch INVALID_INPUT at default t_coh — captured faithfully)*
- Acquisition meso-bench post-A3 (single Correlator, cache fully effective):
  - GPS L1 C/A: **23.9 ms / acquire** (was 42.7 ms post-A2, 64.2 ms pre-A2) — 1.79× from A3 alone, **2.69× cumulative**
  - Galileo E1-B: 301.8 ms / acquire (was 562.5 ms) — 1.86× from A3, **2.02× cumulative**
- FFT microbench (`fft_bench.csv`, single-threaded vs 10-thread; backend=`vdsp_dft` post-A2):
  - 2048: 1.8 µs / 62.4 GFLOPS single → 5.6 µs / 20.2 GFLOPS under 10-way contention (was 4.7 / 8.0 µs pre-A2)
  - 4096: 3.5 µs / 70.7 GFLOPS → 9.4 µs / 26.1 GFLOPS (was 10.0 / 18.5 µs)
  - 8192: 7.1 µs / 74.9 GFLOPS → 22.1 µs / 24.1 GFLOPS (was 26.3 / 36.3 µs)
  - 16384: 19.1 µs / 60.1 GFLOPS → 53.7 µs / 21.3 GFLOPS (was 57.9 / 87.6 µs)
  - 32768: 101.2 µs / 24.3 GFLOPS → 100.0 µs / 24.6 GFLOPS (was 131.3 / 220.5 µs)
  - 65536: 161.1 µs / 32.5 GFLOPS → 203.4 µs / 25.8 GFLOPS (was 249.0 / 467.4 µs)
  - **A2 size-by-size speedup**: 2.58× / 2.88× / 3.70× / 3.03× / 1.30× / 1.55× single-thread. Wins are huge at 2K-16K (2.5-3.7×), modest at 32K-64K (likely L2-cache spill into a different DFT kernel).
- Acquisition meso-bench (`acquisition_bench.csv`, single-threaded; SNR=1000):
  - GPS L1 C/A (Fft<8192>): **42.7 ms / acquire** (was 64.2 ms), 1521 FFTs / acquire, 28.1 µs / FFT
  - Galileo E1-B (Fft<32768>): **562.5 ms / acquire** (was 609 ms), 3027 FFTs / acquire, 185.8 µs / FFT
  - Per-FFT cost in `correlate_primary_complex` runs ~1.6× the raw vDSP FFT cost (wipeoff + code multiply + peak detection on top of the round-trip FFT). Means A4's freq-domain-Doppler win is bounded by FFT-share-of-total ≈ 60% of per-bin cost, not 100% — so A4 saves ~30–40% per Doppler bin, not 50%.
- Canonical subset: `OP9_0`, `OP17_0`, `OP21_0`
- Variant: `baseline` (no A2-A4 optimisations applied)
- Build flags: `-DLUNALINK_BENCH_COUNT_FFTS=ON`, Release, scikit-build-core

## Scope boundaries

- **Not CI-enforced**: runs locally on Marco's M4 MacBook Air only.
  See `docs/shaping/cislunar_nav/fft-acceleration.md` open-question answer on
  CI strategy.
- **Not reproducible across machines**: core layout, clock scaling, and
  thermal state all move the absolute numbers.  `fft_forward_count` /
  `fft_inverse_count` / detection outputs ARE reproducible and are what the
  regression gate checks.
