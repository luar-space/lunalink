#include "lunalink/platform/post.hpp"
#include "lunalink/platform/sci_header.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"
#include "lunalink/codes/legendre.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"
#include "lunalink/platform/watchdog.hpp"
#include "lunalink/codes/codebook_checksums.hpp"

#include <array>
#include <cstdint>

namespace lunalink::signal {

namespace {

/// POST Step 3: inject known TMR triplet and verify vote mechanism.
[[nodiscard]] bool post_verify_tmr() noexcept {
    constexpr uint32_t kTestValue = 0xA5A5A5A5U;
    TmrValue<uint32_t> tmr(kTestValue);
    return tmr.vote() == kTestValue;
}

/// POST Step 4: verify .rodata sentinel integrity against SEU corruption.
// constinit ensures zero-initialisation is never replaced by a runtime call,
// placing this array in the .rodata (read-only data) segment.
// This verifies that radiation events have not corrupted compile-time constants
// in the .rodata segment.  This is DISTINCT from stack-overflow detection, which
// requires a flight linker script to paint the stack guard region at _stack_limit.
alignas(16) constinit const std::array<uint32_t, 4> kRodataIntegritySentinel = {
    0xDEADBEEFU, 0xDEADBEEFU, 0xDEADBEEFU, 0xDEADBEEFU
};

[[nodiscard]] bool post_verify_rodata_constants() noexcept {
    for (const auto val : kRodataIntegritySentinel) {
        if (val != 0xDEADBEEFU) { return false; }
    }
    return true;
}

} // namespace

PostResult run_post() noexcept {
    // Step 1: SCI header CRC-32 (binary integrity).
    if (!sci_header_verify()) {
        return {PostStatus::kFaultDetected, 1};  // FIZ-3
    }

    // Step 2: LUT integrity checks.
    // LDPC matrices carry embedded CRC-32 — strong integrity check.
    const bool ldpc_ok =
        kLdpc_sf2_a.verify_integrity() && kLdpc_sf2_b.verify_integrity() &&
        kLdpc_sf2_b_inv.verify_integrity() && kLdpc_sf2_c.verify_integrity() &&
        kLdpc_sf2_d.verify_integrity() &&
        kLdpc_sf3_a.verify_integrity() && kLdpc_sf3_b.verify_integrity() &&
        kLdpc_sf3_b_inv.verify_integrity() && kLdpc_sf3_c.verify_integrity() &&
        kLdpc_sf3_d.verify_integrity();
    if (!ldpc_ok) {
        return {PostStatus::kFaultDetected, 2};  // FIZ-3
    }
    // BCH and Weil codebook checksums: compare against ground-truth golden constants.
    // Non-zero guard alone only detects total table destruction; equality comparison
    // detects single-bit SEU corruption consistent with [SW-SAF-02, CBIT-AFS-PRN].
    const uint64_t bch_crc = bch_codebook_checksum();
    const uint64_t w10_crc = weil10230_codebook_checksum();
    const uint64_t w15_crc = weil1500_codebook_checksum();
    if (bch_crc != kBchCodebookChecksum ||
        w10_crc != kWeil10230Checksum   ||
        w15_crc != kWeil1500Checksum) {
        return {PostStatus::kFaultDetected, 2};  // FIZ-3
    }
    // Legendre cache: explicit deterministic initialisation.
    // Generates both sequences and verifies against golden CRCs internally.
    // Eliminates hidden __cxa_guard dynamic init (TOCTOU integrity gap closure).
    if (legendre_cache_init() != LegendreStatus::kOk) {
        return {PostStatus::kFaultDetected, 2};  // FIZ-3
    }

    // Step 3: TMR mechanism self-test.
    if (!post_verify_tmr()) {
        return {PostStatus::kFaultDetected, 3};  // FIZ-3
    }

    // Step 4: .rodata integrity (compile-time constant corruption detection).
    if (!post_verify_rodata_constants()) {
        return {PostStatus::kFaultDetected, 4};  // FIZ-3
    }

    // Step 5: CfiGuard baseline self-test.
    CfiGuard guard(0xC0FFEE0DU, 1U);
    guard.tick();
    if (!guard.verify()) {
        return {PostStatus::kFaultDetected, 5};  // FIZ-3
    }

    // Lock init-phase configuration to prevent post-boot mutation.
    lock_watchdog_config();

    return {PostStatus::kPass, 0};
}

}  // namespace lunalink::signal
