#pragma once
#include <cstdint>
#include <span>
#include <type_traits>
#include "lunalink/signal/safety.hpp"

namespace lunalink::signal {

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;

/// Maximum FID value (LSIS §2.4.2).
inline constexpr uint8_t kBchFidMax = 3;

/// Maximum TOI value (LSIS §2.4.2).
inline constexpr uint8_t kBchToiMax = 99;

/// Status codes for BCH operations.
enum class BchStatus : uint8_t {
  kOk              = 0x5AU,  // 01011010
  kOutputTooSmall  = 0xA5U,  // 10100101
  kInvalidFid      = 0x33U,  // 00110011
  kInvalidToi      = 0xCCU,  // 11001100
  kNullOutput      = 0x0FU,  // 00001111
  kInvalidInput    = 0xF0U,  // 11110000
  kAmbiguousMatch  = 0x66U,  // 01100110
  kFaultDetected   = 0x99U,  // 10011001
};

/**
 * @brief Triple Modular Redundant (TMR) type for Frame Identifiers [0, 3].
 */
struct Fid {
  TmrValue<CheckedRange<uint8_t, 0, 3>> storage{CheckedRange<uint8_t, 0, 3>{0U}};

  constexpr Fid() noexcept = default;
  explicit constexpr Fid(uint8_t v) noexcept : storage(CheckedRange<uint8_t, 0, 3>{v}) {}

  [[nodiscard]] constexpr uint8_t value() const noexcept {
    return static_cast<uint8_t>(storage.vote());
  }

  explicit constexpr operator uint8_t() const noexcept { return value(); }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const Fid& other) const noexcept {
    return value() == other.value();
  }

  // Pre-defined nodes for convenience.
  static constexpr Fid kNode1() { return Fid{0}; }
  static constexpr Fid kNode2() { return Fid{1}; }
  static constexpr Fid kNode3() { return Fid{2}; }
  static constexpr Fid kNode4() { return Fid{3}; }
};

struct Toi {
  uint8_t value;
  explicit constexpr Toi(uint8_t v) noexcept : value(v) {}
};

/**
 * @brief Result of a BCH decoding operation.
 * Aligned to 8 bytes to ensure efficient register-loading and no hidden padding leaks.
 */
struct alignas(8) BchResult {
  BchStatus status           = BchStatus::kNullOutput;
  Fid       fid              = Fid::kNode1();
  Toi       toi              = Toi(0U);
  uint32_t  hamming_distance = 0xFFFFFFFFU;

  constexpr BchResult() noexcept = default;
  constexpr BchResult(BchStatus s, Fid f, Toi t, uint32_t d) noexcept 
      : status(s), fid(f), toi(t), hamming_distance(d) {}
};

/**
 * @brief Encode FID and TOI into a 52-symbol BCH(51,8) codeword. [LSIS-AFS-501]
 *
 * @param fid  Frame ID (0-3).
 * @param toi  Time of Interval (0-99).
 * @param out  Buffer to populate (exactly 52 symbols).
 *
 * @pre out.size() == 52
 * @post out is populated with 0s and 1s.
 * @return BchStatus::kOk or an error code.
 */
[[nodiscard]] BchStatus bch_encode(
    Fid                      fid,
    Toi                      toi,
    std::span<uint8_t, 52ULL> out) noexcept;

/**
 * @brief Decode a 52-symbol codeword using maximum likelihood (ML) search. [LSIS-AFS-501]
 *
 * This implementation exhaustively compares the input against the 400 possible 
 * valid codewords to find the best match (minimum Hamming distance).
 *
 * @param in  Codeword symbols (exactly 52).
 *
 * @pre in.size() == 52
 * @return BchResult containing status, decoded message, and confidence metric.
 */
[[nodiscard]] BchResult bch_decode(std::span<const uint8_t, 52ULL> in) noexcept;

/**
 * @brief Verify the integrity of the BCH codebook LUT (Self-Test/CBIT).
 */
[[nodiscard]] uint64_t bch_codebook_checksum() noexcept;

/**
 * @brief Ergonomic template overload for bch_encode that deduces from arrays/fixed-spans.
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

} // namespace lunalink::signal
