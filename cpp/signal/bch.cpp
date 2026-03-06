#include "lunalink/signal/bch.hpp"

#include <array>

namespace lunalink::signal {

BchStatus bch_encode(
    uint8_t     fid,
    uint8_t     toi,
    uint8_t*    out,
    std::size_t out_len
) noexcept {
  if (out == nullptr) {
    return BchStatus::kNullOutput;
  }
  if (out_len < kBchCodewordLength) {
    return BchStatus::kOutputTooSmall;
  }
  if (fid > 3U) {
    return BchStatus::kInvalidFid;
  }
  if (toi > 99U) {
    return BchStatus::kInvalidToi;
  }

  // SB1 = 9 bits: FID (2 MSBs) | TOI (7 LSBs), bit 0 is MSB.
  const auto fid32 = static_cast<uint32_t>(fid);
  const auto toi32 = static_cast<uint32_t>(toi);
  // Ensure we use unsigned logic throughout to satisfy HICPP 4.2.1.
  const auto sb1 = (fid32 << 7U) | toi32;
  const auto bit0 = static_cast<uint8_t>((sb1 >> 8U) & 1U);

  // Extract 8 data bits (bits 1-8, MSB first).
  std::array<uint8_t, kBchInfoBits> data{};
  for (uint32_t i = 0; i < kBchInfoBits; ++i) {
    // Bounds check handled by loop condition; NOLINT used for strict Core Guidelines compliance.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    data[i] = static_cast<uint8_t>((sb1 >> (7U - i)) & 1U);
  }

  // Load LFSR: bit1 -> stage 8, bit8 -> stage 1.
  // s[0] = stage 1 = data[7], s[7] = stage 8 = data[0].
  std::array<uint8_t, kBchInfoBits> s{};
  for (uint32_t i = 0; i < kBchInfoBits; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    s[i] = data[7U - i];
  }

  // Fibonacci LFSR with polynomial 1+X^3+X^4+X^5+X^6+X^7+X^8.
  // Taps (g0..g7): 1,0,0,1,1,1,1,1
  // Feedback = XOR of stages where tap=1: s[0], s[3], s[4], s[5], s[6], s[7].
  constexpr uint32_t kLfsrLength = 51;

  // Prepend bit 0 (raw).
  // Pointer arithmetic is necessary here as we interface with a raw buffer.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  out[0] = bit0;

  for (uint32_t k = 0; k < kLfsrLength; ++k) {
    const uint8_t output = s[7];

    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    const auto fb = static_cast<uint8_t>(s[0] ^ s[3] ^ s[4] ^ s[5] ^ s[6] ^ s[7]);

    // Shift stages right: s[7]<-s[6], ..., s[1]<-s[0].
    for (uint32_t i = 7; i > 0; --i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      s[i] = s[i - 1U];
    }
    s[0] = fb;

    // XOR output with bit 0 and store.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out[k + 1U] = static_cast<uint8_t>(output ^ bit0);
  }

  return BchStatus::kOk;
}

} // namespace lunalink::signal
