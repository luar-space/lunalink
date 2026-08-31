# Fuzz harnesses

Coverage-guided [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harnesses for
the LunaLink entry points that consume **externally-supplied data** — the place
where malformed input is most likely to trigger undefined behaviour. They
complement the existing assurance stack: clang-tidy/cppcheck/Clang-SA are
heuristic and ASan/UBSan only see executed paths, whereas fuzzing actively
*searches* the input space for the bytes that break those checks.

This is robustness testing of the navigation-data ingest path, supporting the
defensive-input requirements behind ECSS-Q-ST-80C §5.5 software verification.

## Harnesses

| Target | Entry points | Input |
|---|---|---|
| `fuzz_message_parser` | `parse_sb2` / `parse_sb3` / `parse_sb4`, `extract_bits`, `validate_spare_bits`, `type_field_to_message_type` | decoded nav bits (one bit per byte) |
| `fuzz_frame_decode` | `frame_decode` — full RX pipeline (sync → BCH → deinterleave → LDPC → CRC) | soft-decision symbols (arbitrary floats, incl. NaN/Inf) |
| `fuzz_fec` | `crc24_verify` / `crc24_calculate`, `bch_decode` / `bch_decode_soft`, `block_deinterleave_soft`, `ldpc_decode` | byte buffers / float LLRs |

Each harness reaches both the size-validation/rejection paths and the deep
decode logic, and the soft decoders are fed raw byte patterns reinterpreted as
IEEE-754 floats so non-finite LLRs are exercised directly.

## Requirements

LLVM **Clang with the libFuzzer runtime**. Apple Clang does **not** ship it and
GCC has no libFuzzer — use the Homebrew LLVM toolchain (`brew install llvm`).
The build fails fast with a clear message otherwise.

## Running

```sh
task fuzz                 # 60 s per harness (default)
FUZZ_TIME=300 task fuzz   # 5 min per harness
```

Or directly:

```sh
cmake -B build/fuzz -S . \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" \
  -DENABLE_FUZZING=ON -DBUILD_TESTING=OFF -DBUILD_PYTHON_BINDINGS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fuzz --parallel
./build/fuzz/fuzz_message_parser corpus/message_parser
```

`ENABLE_FUZZING` is OFF by default and never affects the flight library — it only
instruments `lunalink_core` for coverage + ASan/UBSan inside the fuzz build dir.
It is mutually exclusive with `ENABLE_SANITIZERS` (which it already implies).

## Seed corpus

A committed seed corpus lives in `cpp/fuzz/seeds/<target>/`. It is generated
from a *valid* encoded frame (`scripts/fuzz_seed_corpus.py`, via the LunaLink
encoder) and serialised into each harness's exact input layout, so the fuzzer
starts from inputs that already reach deep code — a decodable frame, convergent
LDPC/BCH codewords, real decoded subframe bits — rather than the early
size/sync rejection paths. Regenerate with `task fuzz-seed`.

`task fuzz` passes the seed dir as a read-only second corpus; new finds are
written only to the growing (git-ignored) `corpus/<target>/`.

The generator has no dedicated unit test by design: the frame it builds is
asserted to decode at generation time, and the regression gate below replays
every seed end-to-end on each PR — so a broken generator surfaces immediately.

## Regression gate

`task fuzz-replay` (and the `Fuzz Smoke` CI workflow on PRs touching `cpp/`)
replays every committed seed once with `-runs=0` — no mutation — and fails on
any crash/leak/UB. This is the fast PR-time gate; `fuzz.yml` is the slow nightly
search whose corpus accumulates across runs.

## Findings

Crash reproducers land in `fuzz-artifacts/` (git-ignored) and are replayed with:

```sh
./build/fuzz/fuzz_frame_decode fuzz-artifacts/fd-crash-<hash>
```

## Note on flight code

Harnesses are **non-flight test code** (like the SWIFI suite) — no FMEA / safety-
plan entry is required for the harness sources themselves. Any crash they surface
in `lunalink_core`, however, is a flight-code defect and must be triaged as one.
