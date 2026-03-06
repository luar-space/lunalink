#include "lunalink/signal/bch.hpp"

#include <bit>

namespace lunalink::signal {

BchStatus bch_encode(
    uint8_t            fid,
    uint8_t            toi,
    std::span<uint8_t> out) noexcept {
  if (out.size() < kBchCodewordLength) [[unlikely]] {
    return BchStatus::kOutputTooSmall;
  }
  if (fid > kBchFidMax) [[unlikely]] {
    return BchStatus::kInvalidFid;
  }
  if (toi > kBchToiMax) [[unlikely]] {
    return BchStatus::kInvalidToi;
  }

  // SB1 = 9 bits: [FID(1:0), TOI(6:0)]
  // Bit 0 is the raw MSB (FID bit 1).
  // Bits 1-8 are fed into the LFSR (FID bit 0 and TOI bits 6-0).
  // NOLINTNEXTLINE(hicpp-signed-bitwise)
  const auto bit0 = static_cast<uint8_t>((static_cast<uint32_t>(fid) >> 1U) & 1U);
  // NOLINTNEXTLINE(hicpp-signed-bitwise)
  auto state = static_cast<uint8_t>(((static_cast<uint32_t>(fid) & 1U) << 7U) | (static_cast<uint32_t>(toi) & 0x7FU));

  // Tap mask: bits 0, 3, 4, 5, 6, 7 (indices match Fig 7 registers).
  // In our uint8_t 'state', bit 0 is Stage 1, bit 7 is Stage 8.
  // Polynomial: 1 + X^3 + X^4 + X^5 + X^6 + X^7 + X^8
  // Stages:      1   2   3   4   5   6   7   8
  // Indices:     0   1   2   3   4   5   6   7
  // Taps:        1   0   0   1   1   1   1   1
  constexpr uint8_t kFeedbackMask = 0b11111001U;
  constexpr uint32_t kLfsrLength = 51;

  // Prepend bit 0 (raw MSB).
  out[0] = bit0;

  for (uint32_t k = 0; k < kLfsrLength; ++k) {
    // Stage 8 output (bit 7).
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    const auto output = static_cast<uint8_t>((static_cast<uint32_t>(state) >> 7U) & 1U);

    // XOR of tapped stages (popcount % 2 is an efficient parity XOR).
    const auto fb = static_cast<uint8_t>(static_cast<uint32_t>(std::popcount(static_cast<uint32_t>(state & kFeedbackMask))) % 2U);

    // Shift registers (bits 0-6 move to 1-7; Stage 1 catches feedback).
    state = static_cast<uint8_t>((static_cast<uint32_t>(state) << 1U) | fb);

    // XOR output with bit 0 and store into codeword symbols.
    out[k + 1U] = static_cast<uint8_t>(output ^ bit0);
  }

  return BchStatus::kOk;
}

} // namespace lunalink::signal
