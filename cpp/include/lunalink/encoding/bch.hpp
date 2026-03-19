#pragma once
#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>
#include "lunalink/core/safety.hpp"

namespace lunalink::signal {

/** @brief Status codes for BCH(51,8) encode and maximum-likelihood decode. [LSIS-AFS-501] SEU-hardened with high Hamming distance bit patterns. */
enum class BchStatus : uint8_t {
    kOk             = 0x5AU,  ///< Operation completed successfully.
    kOutputTooSmall = 0xA5U,  ///< Output buffer too small for result.
    kInvalidFid     = 0x33U,  ///< FID value exceeds kBchFidMax.
    kInvalidToi     = 0xCCU,  ///< TOI value exceeds kBchToiMax.
    kNullOutput     = 0x99U,  ///< Required buffer is null or empty.
    kInvalidInput   = 0x66U,  ///< Input parameters out of range or invalid.
    kAmbiguousMatch = 0xF0U,  ///< Multiple codewords at same Hamming distance.
    kFaultDetected  = 0x0FU,  ///< CFI, TMR, or integrity check failed (FIZ-1/2/3).
};

/// BCH(51,8) encoder output length: 1 raw MSB + 51 LFSR symbols = 52 symbols.
inline constexpr uint8_t kBchCodewordLength = 52;
/// Maximum FID value (inclusive). LSIS §2.4.3.
inline constexpr uint8_t kBchFidMax = 3;
/// Maximum TOI value (inclusive). LSIS §2.4.3.
inline constexpr uint8_t kBchToiMax = 99;

/// Tag type for nominal type safety (prevents Fid/Toi confusion at compile time).
struct FidTag {};
/// Tag type for nominal type safety (prevents Fid/Toi confusion at compile time).
struct ToiTag {};

/**
 * @brief TMR-protected Frequency ID [0, 3] with LSIS node constants.
 */
struct Fid : TmrCheckedId<0, kBchFidMax, FidTag> {
  using TmrCheckedId::TmrCheckedId;

  // LSIS Node Constants
  static constexpr Fid kNode1() { return Fid{0}; }
  static constexpr Fid kNode2() { return Fid{1}; }
  static constexpr Fid kNode3() { return Fid{2}; }
  static constexpr Fid kNode4() { return Fid{3}; }
};

/**
 * @brief TMR-protected Time of Interval [0, 99].
 */
using Toi = TmrCheckedId<0, kBchToiMax, ToiTag>;

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
    requires (!std::same_as<std::remove_cvref_t<T>, std::span<uint8_t, 52ULL>>)
[[nodiscard]] inline BchStatus bch_encode(
    Fid fid,
    Toi toi,
    T&& out) noexcept {
  return bch_encode(fid, toi, std::span<uint8_t, 52ULL>(out));
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
 * @brief Soft-decision Maximum Likelihood BCH decoder. [LSIS-AFS-501]
 *
 * Operates on 52 channel LLRs (L_ch = 2y/σ² for BPSK AWGN).
 * For BPSK/AWGN: positive value favours bit=0, negative favours bit=1.
 * Selects the codebook entry that maximises the inner-product metric
 * Σ_j llr_j × (1 − 2·c_j), giving ~2 dB gain over hard-decision ML.
 *
 * **Behavioural differences from bch_decode (hard):**
 * - Sets kInvalidInput (does not early-return) if any LLR is non-finite (NaN or Inf).
 * - Always returns kOk for valid finite inputs — there is no confidence threshold
 *   (no equivalent of the hard decoder's kNullOutput / min_dist > 2 guard).
 *   The caller must not infer reliability from the kOk status alone.
 * - No ambiguity detection: soft-metric ties are broken by the lowest codebook
 *   index. kAmbiguousMatch is never returned.
 * - BchResult::hamming_distance is always 0xFF (255) — a soft-decode sentinel,
 *   not a Hamming distance. Do not compare this field against numeric thresholds.
 * - LLR values are internally clamped to ±100.0 before accumulation; inputs
 *   outside this range are treated as saturated but do not raise kInvalidInput.
 *
 * @param llr  52 finite channel LLR values (float, one per bit position).
 * @return BchResult with decoded FID/TOI.
 *         status == kInvalidInput if any LLR is non-finite.
 *         status == kOk otherwise (including low-confidence / very noisy cases).
 *         status == kFaultDetected if CFI or variable-mirror integrity check fails.
 */
[[nodiscard]] BchResult bch_decode_soft(std::span<const float, 52ULL> llr) noexcept;

/**
 * @brief Calculate JIT codebook checksum for CBIT integrity. [NASA-FSW-CBIT]
 * @return 64-bit CRC-like checksum of the internal codebook memory.
 */
[[nodiscard]] uint64_t bch_codebook_checksum() noexcept;

} // namespace lunalink::signal
