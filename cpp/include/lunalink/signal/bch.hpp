#pragma once
#include <cstdint>
#include <span>
#include <type_traits>
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/**
 * @brief Fault-tolerant status codes for BCH operations.
 * Non-contiguous bit patterns with high Hamming distance resist SEU bit-flips.
 */
enum class BchStatus : uint8_t {
  kOk             = 0x5AU,  // 01011010
  kOutputTooSmall = 0xA5U,  // 10100101
  kInvalidFid     = 0x33U,  // 00110011
  kInvalidToi     = 0xCCU,  // 11001100
  kNullOutput     = 0x99U,  // 10011001
  kInvalidInput   = 0x66U,  // 01100110
  kAmbiguousMatch = 0xF0U,  // 11110000
  kFaultDetected  = 0x0FU,  // 00001111
};

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;
inline constexpr uint8_t kBchFidMax = 3;
inline constexpr uint8_t kBchToiMax = 99;

/**
 * @brief Triple Modular Redundant (TMR) type for Frequency ID [0, 3].
 */
struct Fid {
  TmrValue<CheckedRange<uint8_t, 0, 3>> storage{CheckedRange<uint8_t, 0, 3>{0U}};

  constexpr Fid() noexcept = default;
  explicit constexpr Fid(uint8_t v) noexcept : storage(CheckedRange<uint8_t, 0, 3>{v}) {}

  [[nodiscard]] constexpr uint8_t value() const noexcept {
    return static_cast<uint8_t>(storage.peek());
  }

  /**
   * @brief Perform active repair of the TMR triplets.
   * @return The repaired majority value.
   */
  uint8_t repair() const noexcept {
    return static_cast<uint8_t>(storage.vote());
  }

  explicit constexpr operator uint8_t() const noexcept { return value(); }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const Fid& other) const noexcept {
    return value() == other.value();
  }

  // LSIS Node Constants
  static constexpr Fid kNode1() { return Fid{0}; }
  static constexpr Fid kNode2() { return Fid{1}; }
  static constexpr Fid kNode3() { return Fid{2}; }
  static constexpr Fid kNode4() { return Fid{3}; }
};

/**
 * @brief Triple Modular Redundant (TMR) type for Time of Interval [0, 99].
 */
struct Toi {
  TmrValue<CheckedRange<uint8_t, 0, 99>> storage{CheckedRange<uint8_t, 0, 99>{0U}};

  constexpr Toi() noexcept = default;
  explicit constexpr Toi(uint8_t v) noexcept : storage(CheckedRange<uint8_t, 0, 99>{v}) {}

  [[nodiscard]] constexpr uint8_t value() const noexcept {
    return static_cast<uint8_t>(storage.peek());
  }

  /**
   * @brief Perform active repair of the TMR triplets.
   * @return The repaired majority value.
   */
  uint8_t repair() const noexcept {
    return static_cast<uint8_t>(storage.vote());
  }

  explicit constexpr operator uint8_t() const noexcept { return value(); }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const Toi& other) const noexcept {
    return value() == other.value();
  }
};

/**
 * @brief Result of a BCH decoding operation.
 * Aligned to 8 bytes to ensure efficient register-loading and no hidden padding leaks.
 */
struct alignas(8) BchResult {
  BchStatus status;
  Fid       fid;
  Toi       toi;
  uint8_t   hamming_distance;

  constexpr BchResult(BchStatus s, Fid f, Toi t, uint8_t d) noexcept
      : status(s), fid(f), toi(t), hamming_distance(d) {}
};

/**
 * @brief Encode a navigation sub-block header (SB1). [LSIS-AFS-501]
 *
 * Header contains 2-bit FID and 7-bit TOI, protected by BCH(51,8) code.
 * Final output is 52 bits (1 raw MSB + 51 LFSR bits).
 *
 * @param fid Frequency Identifier (0-3).
 * @param toi Time of Interval (0-99).
 * @param out Buffer for 52 binary symbols (uint8_t {0, 1}).
 *
 * @return BchStatus::kOk or error code.
 */
[[nodiscard]] BchStatus bch_encode(
    Fid                      fid,
    Toi                      toi,
    std::span<uint8_t, 52ULL> out) noexcept;

/**
 * @brief Ergonomic overload for fixed-size arrays.
 */
template <typename T>
[[nodiscard]] inline BchStatus bch_encode(
    Fid fid,
    Toi toi,
    T&& out) noexcept {
  if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::span<uint8_t, 52ULL>>) {
    return bch_encode(fid, toi, out);
  } else {
    return bch_encode(fid, toi, std::span<uint8_t, 52ULL>(out));
  }
}

/**
 * @brief Maximum Likelihood (ML) BCH decoder. [LSIS-AFS-501]
 * Corrects up to 2 errors using pre-computed exhaustive codebook.
 * Aligned with Class A Flight Software safety standards.
 *
 * @param in Buffer containing 52 binary symbols.
 * @return BchResult including decoded parameters and confidence status.
 */
[[nodiscard]] BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept;

/**
 * @brief Calculate JIT codebook checksum for CBIT integrity. [NASA-FSW-CBIT]
 * @return 64-bit CRC-like checksum of the internal codebook memory.
 */
[[nodiscard]] uint64_t bch_codebook_checksum() noexcept;

} // namespace lunalink::signal
