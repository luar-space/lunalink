#pragma once
#include <cstdint>
#include <span>

namespace lunalink::signal {

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;

/// Number of information bits fed into the LFSR (bits 1-8 of SB1).
inline constexpr uint8_t kBchInfoBits = 8;

/// Total SB1 field width before encoding.
inline constexpr uint8_t kSb1BitCount = 9;

/// Maximum FID (2 bits): 3.
inline constexpr uint8_t kBchFidMax = 3;
/// Maximum TOI (7 bits): 99 (LSIS V1.0 §2.3.4.1).
inline constexpr uint8_t kBchToiMax = 99;

enum class BchStatus : uint8_t {
  kOk = 0,
  kOutputTooSmall,
  kInvalidFid,
  kInvalidToi,
  kNullOutput,
};

/// Encode a 9-bit SB1 field (FID + TOI) using BCH(51,8).
///
/// Per LSIS V1.0 §2.4.2.1:
///   - The 8 LSBs are loaded into an 8-stage Fibonacci LFSR.
///   - The MSB (bit 0) is XORed with each produced symbol.
///
/// @pre fid <= 3
/// @pre toi <= 99
/// @pre out.size() == kBchCodewordLength (52)
/// @post out is populated with {0, 1} symbols if kOk
/// @complexity O(kBchCodewordLength)
/// Pack FID and TOI into a 9-bit information field (SB1).
/// Per LSIS V1.0 §2.4.2: SB1 = (FID << 7) | TOI.
[[nodiscard]] constexpr uint16_t pack_bch_info(uint8_t fid, uint8_t toi) noexcept {
  return static_cast<uint16_t>((static_cast<uint32_t>(fid & 0x03U) << 7U) |
                               (static_cast<uint32_t>(toi & 0x7FU)));
}

[[nodiscard]] BchStatus bch_encode(
    uint8_t            fid,
    uint8_t            toi,
    std::span<uint8_t> out) noexcept;

/**
 * @brief Ergonomic overload for bch_encode that deduces from arrays/fixed-spans.
 */
template <typename T>
[[nodiscard]] inline BchStatus bch_encode(
    uint8_t fid,
    uint8_t toi,
    T&&     out) noexcept {
  return bch_encode(fid, toi, std::span<uint8_t>(out));
}

} // namespace lunalink::signal
