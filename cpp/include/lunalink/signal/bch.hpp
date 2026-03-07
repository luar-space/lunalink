#pragma once
#include <cstdint>
#include <span>
#include <type_traits>

namespace lunalink::signal {

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;
inline constexpr uint8_t kBchInfoBits       = 8;
inline constexpr uint8_t kSb1BitCount       = 9;

/// FID/TOI constants.
inline constexpr uint8_t  kBchFidMax        = 3;
inline constexpr uint8_t  kBchFidCount      = 4;
inline constexpr uint8_t  kBchToiMax        = 99;
inline constexpr uint8_t  kBchToiCount      = 100;
inline constexpr uint32_t kBchCodebookSize  = 400;

/**
 * @brief Fault-tolerant status codes for BCH operations.
 * Patterns selected with high Hamming distance to resist SEU bit-flips.
 */
enum class BchStatus : uint8_t {
  kOk              = 0x5AU,  // 01011010
  kOutputTooSmall  = 0xA5U,  // 10100101
  kInvalidFid      = 0x33U,  // 00110011
  kInvalidToi      = 0xCCU,  // 11001100
  kNullOutput      = 0x0FU,  // 00001111
  kInvalidInput    = 0xF0U,  // 11110000
  kAmbiguousMatch  = 0x66U,  // 01100110
  kFaultDetected   = 0x99U,  // 10011001 (Reciprocal check failed)
};

enum class Fid : uint8_t {
  kNode1 = 0,
  kNode2 = 1,
  kNode3 = 2,
  kNode4 = 3,
};

struct Toi {
  uint8_t value;
  explicit constexpr Toi(uint8_t v) noexcept : value(v) {}
  explicit constexpr operator uint8_t() const noexcept { return value; }
};

/**
 * @brief Result of a BCH decoding operation.
 * Aligned to 8 bytes to ensure efficient register-loading and no hidden padding leaks.
 */
struct alignas(8) BchResult {
  BchStatus status           = BchStatus::kNullOutput;
  Fid       fid              = Fid::kNode1;
  Toi       toi              = Toi(0U);
  uint32_t  hamming_distance = 0xFFFFFFFFU;

  constexpr BchResult() noexcept = default;
  constexpr BchResult(BchStatus s, Fid f, Toi t, uint32_t d) noexcept 
    : status(s), fid(f), toi(t), hamming_distance(d) {}
};

[[nodiscard]] BchStatus bch_encode(
    Fid                      fid,
    Toi                      toi,
    std::span<uint8_t, 52ULL> out) noexcept;

[[nodiscard]] BchResult bch_decode(
    std::span<const uint8_t, 52ULL> in) noexcept;

/**
 * @brief Verify the integrity of the static codebook (Self-Test).
 * @return Additive sum of the codebook for rapid verification.
 */
[[nodiscard]] uint64_t bch_codebook_checksum() noexcept;

template <typename T>
[[nodiscard]] inline BchStatus bch_encode(Fid fid, Toi toi, T&& out) noexcept {
  if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::span<uint8_t, 52ULL>>) {
    return bch_encode(fid, toi, out);
  } else {
    return bch_encode(fid, toi, std::span<uint8_t, 52ULL>(out));
  }
}

} // namespace lunalink::signal
