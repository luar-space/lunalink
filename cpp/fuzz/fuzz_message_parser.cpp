// libFuzzer harness: navigation-message parsers (SB2/SB3/SB4) and bit extraction.
//
// These functions consume decoded navigation data — one bit per byte — that
// ultimately originates over the air, so they are prime targets for malformed
// input. The harness exercises both the size-validation paths (raw span fed
// directly) and the deep parse logic (canonical-size buffers), and stress-tests
// extract_bits with attacker-controlled offset/width to probe bit-range bounds.
//
// Build:  cmake -DENABLE_FUZZING=ON (requires Clang/libFuzzer)
// Run:    build/fuzz/fuzz_message_parser corpus/message_parser

#include "lunalink/messages/message_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace lunalink::signal;

namespace {

// Build a canonical-size bit buffer (one bit per byte) from arbitrary fuzz
// bytes by cycling the input, so the parser always reaches its deep logic.
std::vector<uint8_t> as_bits(std::span<const uint8_t> data, size_t n_bits) {
  std::vector<uint8_t> bits(n_bits, 0U);
  if (!data.empty()) {
    for (size_t i = 0; i < n_bits; ++i) {
      bits[i] = static_cast<uint8_t>(data[i % data.size()] & 1U);
    }
  }
  return bits;
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
    // Deep path: exact-size SB2 buffer.
    const auto bits = as_bits(rest, kSb2DataBits);
    Sb2ParseResult result;
    (void)parse_sb2(bits, result);
    // Rejection path: raw (usually wrong-size) span.
    Sb2ParseResult result_raw;
    (void)parse_sb2(rest, result_raw);
    break;
  }
  case 1U: {
    const auto bits = as_bits(rest, kSb3DataBits);
    Sb3ParseResult result;
    (void)parse_sb3(bits, result);
    Sb3ParseResult result_raw;
    (void)parse_sb3(rest, result_raw);
    break;
  }
  case 2U: {
    const auto bits = as_bits(rest, kSb4DataBits);
    Sb4ParseResult result;
    (void)parse_sb4(bits, result);
    Sb4ParseResult result_raw;
    (void)parse_sb4(rest, result_raw);
    break;
  }
  case 3U: {
    // extract_bits with attacker-controlled offset and width over a
    // variable-length bit array — the classic out-of-bounds target.
    if (rest.size() < 3U) {
      break;
    }
    const auto offset = static_cast<uint16_t>((static_cast<unsigned>(rest[0]) << 8U) | rest[1]);
    const auto n_bits = rest[2];
    uint32_t out = 0;
    (void)extract_bits(rest.subspan(3), offset, n_bits, out);
    break;
  }
  case 4U: {
    (void)validate_spare_bits(rest);
    break;
  }
  case 5U: {
    // Type-field mapping over the full 0..255 range (only 0..63 are valid).
    for (const uint8_t raw : rest) {
      (void)type_field_to_message_type(raw);
    }
    break;
  }
  default:
    break;
  }
  return 0;
}
