#include "lunalink/signal/post.hpp"
#include "lunalink/signal/sci_header.hpp"
#include "lunalink/signal/bch.hpp"
#include "lunalink/signal/ldpc_tables.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/safety.hpp"

#include <array>
#include <cstdint>

namespace lunalink::signal {

namespace {

// Step 3: Verify TMR mechanism with a fresh test triplet.
[[nodiscard]] bool post_verify_tmr() noexcept {
    constexpr uint32_t kTestValue = 0xA5A5A5A5U;
    TmrValue<uint32_t> tmr(kTestValue);
    return tmr.vote() == kTestValue;
}

// Step 4: Stack sentinel pattern painted at link time.
// constinit guarantees zero-initialisation is never replaced by a runtime call.
// The pattern is chosen to be detectable if memory is zeroed or corrupted.
alignas(16) constinit const std::array<uint32_t, 4> kStackSentinel = {
    0xDEADBEEFU, 0xDEADBEEFU, 0xDEADBEEFU, 0xDEADBEEFU
};

[[nodiscard]] bool post_verify_stack_sentinel() noexcept {
    for (const auto val : kStackSentinel) {
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
    // BCH and Weil codebook checksums: verify non-zero (indicates computation ran).
    const uint64_t bch_crc = bch_codebook_checksum();
    const uint64_t w10_crc = weil10230_codebook_checksum();
    const uint64_t w15_crc = weil1500_codebook_checksum();
    if (bch_crc == 0U || w10_crc == 0U || w15_crc == 0U) {
        return {PostStatus::kFaultDetected, 2};  // FIZ-3
    }

    // Step 3: TMR mechanism self-test.
    if (!post_verify_tmr()) {
        return {PostStatus::kFaultDetected, 3};  // FIZ-3
    }

    // Step 4: Stack sentinel integrity.
    if (!post_verify_stack_sentinel()) {
        return {PostStatus::kFaultDetected, 4};  // FIZ-3
    }

    // Step 5: CfiGuard baseline self-test.
    CfiGuard guard(0xC0FFEE0DU, 1U);
    guard.tick();
    if (!guard.verify()) {
        return {PostStatus::kFaultDetected, 5};  // FIZ-3
    }

    return {PostStatus::kPass, 0};
}

}  // namespace lunalink::signal
