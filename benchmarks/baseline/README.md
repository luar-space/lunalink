# Benchmark Baseline

Committed reference measurements for the FFT backend microbenchmark.

## Files

| File | Produced by | What it holds |
|------|-------------|---------------|
| `fft_bench.csv` | `task bench-fft` | ns/FFT × size × backend × thread mode |

## How to refresh the baseline

1. Check out a branch representing the new reference point (usually `main`
   after a deliberate speedup merge).
2. `task bench-fft` — produces `benchmarks/latest/fft_bench.csv`.
3. `task bench-baseline-update` — copies `latest/*.csv` over `baseline/`.
4. `git diff benchmarks/baseline/` — sanity-check the delta.
5. Commit.

**Never** refresh the baseline on the branch that's meant to be evaluated
against it — that hides the exact speedup you were trying to measure.

## Scope boundaries

- **Not CI-enforced**: absolute timings vary with host core layout, clock
  scaling, and thermal state, so the committed CSV is a reference point for
  like-for-like comparison on one machine, not a portable gate.
- Wall-clock performance requirements are validated separately by the
  `[wcet]`-tagged tests (`task wcet`) and the BER/throughput acceptance
  suite (`task performance`).
