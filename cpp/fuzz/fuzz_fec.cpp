// libFuzzer harness: forward-error-correction decode entry points.
//
// Secondary targets that all take a buffer: CRC-24Q verify/calculate, the hard-
// and soft-decision BCH decoders, the block deinterleaver, and the LDPC decoder.
// The soft decoders must tolerate non-finite LLRs; the harness feeds raw byte
// patterns reinterpreted as floats so NaN/Inf/denormals are exercised directly.
//
// Build:  cmake -DENABLE_FUZZING=ON (requires Clang/libFuzzer)
// Run:    build/fuzz/fuzz_fec corpus/fec

#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

using namespace lunalink::signal;

namespace {

// Fill a float buffer (cycled) from raw fuzz bytes via bit reinterpretation,
// so arbitrary IEEE-754 patterns (NaN/Inf/denormal) reach the soft decoders.
void fill_floats(std::span<float> out, std::span<const uint8_t> data) {
  for (size_t i = 0; i < out.size(); ++i) {
    float v = 0.0F;
    if (data.size() >= sizeof(float)) {
      std::array<uint8_t, sizeof(float)> raw{};
      for (size_t b = 0; b < raw.size(); ++b) {
        raw[b] = data[((i * sizeof(float)) + b) % data.size()];
      }
      std::memcpy(&v, raw.data(), raw.size());
    }
    out[i] = v;
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }
  const uint8_t selector = data[0];
  const std::span<const uint8_t> rest(data + 1, size - 1);

  switch (selector % 6U) {
  case 0U: {
    // CRC-24Q verify over an arbitrary-length bit array.
    (void)crc24_verify(rest);
    break;
  }
  case 1U: {
    // CRC-24Q calculate over an arbitrary-length bit array.
    std::array<uint8_t, 24> crc{};
    (void)crc24_calculate(rest, crc);
    break;
  }
  case 2U: {
    // Hard-decision BCH over exactly 52 symbols (cycled, forced to {0,1}).
    std::array<uint8_t, 52> in{};
    for (size_t i = 0; i < in.size(); ++i) {
      in[i] = rest.empty() ? 0U : static_cast<uint8_t>(rest[i % rest.size()] & 1U);
    }
    (void)bch_decode(in);
    break;
  }
  case 3U: {
    // Soft-decision BCH over 52 LLRs, including non-finite values.
    std::array<float, 52> llr{};
    fill_floats(llr, rest);
    (void)bch_decode_soft(llr);
    break;
  }
  case 4U: {
    // Soft block deinterleave: fixed 5880-symbol in/out.
    auto in = std::make_unique<std::array<float, kInterleaverSize>>();
    auto out = std::make_unique<std::array<float, kInterleaverSize>>();
    fill_floats(*in, rest);
    (void)block_deinterleave_soft(*in, *out);
    break;
  }
  case 5U: {
    // LDPC decode (SF2: 2400 symbols -> 1200 bits). Heap workspace (~342 KB).
    auto symbols = std::make_unique<std::array<float, 2400>>();
    auto out = std::make_unique<std::array<uint8_t, 1200>>();
    auto workspace = std::make_unique<LdpcWorkspace>();
    fill_floats(*symbols, rest);
    const uint8_t iterations = static_cast<uint8_t>((rest.empty() ? 0U : rest[0] % 8U) + 1U);
    (void)ldpc_decode(LdpcSubframe::kSF2, *symbols, iterations, *out, *workspace);
    break;
  }
  default:
    break;
  }
  return 0;
}
