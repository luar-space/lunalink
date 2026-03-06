#pragma once
#include <cstddef>
#include <cstdint>

namespace lunalink::signal {

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;

/// Number of information bits fed into the LFSR (bits 1-8 of SB1).
inline constexpr uint8_t kBchInfoBits = 8;

/// Total SB1 field width before encoding.
inline constexpr uint8_t kSb1BitCount = 9;

enum class BchStatus : uint8_t {
  kOk = 0,
  kNullOutput,
  kOutputTooSmall,
  kInvalidFid,
  kInvalidToi,
};

/// Encode a 9-bit SB1 field (FID + TOI) using BCH(51,8).
///
/// Per LSIS V1.0 §2.4.2.1:
///   - The 8 LSBs are loaded into an 8-stage Fibonacci LFSR with generator
///     polynomial 1+X^3+X^4+X^5+X^6+X^7+X^8 and shifted 51 times.
///   - The MSB (bit 0) is XORed with each of the 51 LFSR output symbols
///     and prepended as the MSB of the 52-symbol codeword.
///
/// @param fid  Frame Identifier (0-3, 2 bits)
/// @param toi  Time of Interval (0-99, 7 bits)
/// @param out  Caller-allocated buffer, length >= 52, values in {0, 1}
/// @param out_len  Number of bytes available at @p out
[[nodiscard]] BchStatus bch_encode(
    uint8_t       fid,
    uint8_t       toi,
    uint8_t*      out,
    std::size_t   out_len
) noexcept;

} // namespace lunalink::signal
