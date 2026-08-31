// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// libFuzzer harness: full RX decode pipeline (frame_decode).
//
// frame_decode orchestrates sync detection -> soft BCH -> deinterleave ->
// LDPC -> CRC over a span of soft-decision symbols. Those symbols are derived
// from the received signal, so arbitrary float content (including non-finite
// values) must be handled without crashing — exactly what this harness probes.
//
// LDPC iterations are kept small (input-controlled, 1..8) so the fuzzer gets a
// fast feedback loop; the goal is robustness of the pipeline, not convergence.
//
// Build:  cmake -DENABLE_FUZZING=ON (requires Clang/libFuzzer)
// Run:    build/fuzz/fuzz_frame_decode corpus/frame_decode

#include "lunalink/core/telemetry.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

using namespace lunalink::signal;

namespace {

// A little slack beyond one frame so the sync search has room to slide.
constexpr size_t kRxLen = static_cast<size_t>(kFrameLength) + 128U;

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Heap-allocate: workspace (~375 KB) and rx buffer must not be on the stack.
  auto rx = std::make_unique<std::array<float, kRxLen>>();
  auto workspace = std::make_unique<FrameDecodeWorkspace>();

  // Reinterpret raw bytes (cycled) as IEEE-754 floats so the fuzzer can drive
  // NaN/Inf/denormal soft symbols straight into the pipeline.
  for (size_t i = 0; i < kRxLen; ++i) {
    float sample = 0.0F;
    if (size >= sizeof(float)) {
      std::array<uint8_t, sizeof(float)> raw{};
      for (size_t b = 0; b < raw.size(); ++b) {
        // Loop index is bounded by the container's own size(), so the access is
        // provably in range; the check cannot see that through the loop bound.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        raw[b] = data[((i * sizeof(float)) + b) % size];
      }
      std::memcpy(&sample, raw.data(), raw.size());
    }
    // Loop index is bounded by the container's own size(), so the access is
    // provably in range; the check cannot see that through the loop bound.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    (*rx)[i] = sample;
  }

  const uint8_t iterations = size > 0 ? static_cast<uint8_t>((data[0] % 8U) + 1U) : 1U;

  NullTelemetryHal hal;
  FrameDecodeResult result;
  (void)frame_decode(*rx, result, *workspace, hal, iterations);
  return 0;
}
