#include "lunalink/signal/legendre.hpp"
#include "lunalink/signal/safety.hpp"
#include "lunalink/signal/codebook_checksums.hpp"
#include <algorithm>
#include <array>

namespace lunalink::signal {

namespace {

// CRC-32 (IEEE 802.3, reflected) over a byte span.
// Same algorithm as bch_codebook_checksum / weil*_codebook_checksum.
uint64_t calculate_crc(std::span<const uint8_t> data) noexcept {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint8_t val : data) {
        crc ^= val;
        for (uint32_t i = 0; i < 8U; i++) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
        }
    }
    return ~static_cast<uint64_t>(crc);
}

// Legendre cache — file-scope, no hidden __cxa_guard initialization.
// Must be explicitly initialized via legendre_cache_init() before use.
constexpr uint16_t kPrime10223 = 10223;
constexpr uint16_t kPrime1499  = 1499;

alignas(64) std::array<uint8_t, kPrime10223> g_data10223{};
alignas(64) std::array<uint8_t, kPrime1499>  g_data1499{};
bool g_initialized = false;

} // namespace

LegendreStatus legendre_cache_init() noexcept {
    // Generate both Legendre sequences.
    const auto s1 = generate_legendre_sequence(kPrime10223, std::span(g_data10223));
    if (s1 != LegendreStatus::kOk) [[unlikely]] {
        secure_scrub(g_data10223);
        secure_scrub(g_data1499);
        g_initialized = false;
        return s1;
    }

    const auto s2 = generate_legendre_sequence(kPrime1499, std::span(g_data1499));
    if (s2 != LegendreStatus::kOk) [[unlikely]] {
        secure_scrub(g_data10223);
        secure_scrub(g_data1499);
        g_initialized = false;
        return s2;
    }

    // Verify against golden CRCs — reject corrupted generation.
    if (calculate_crc(std::span(g_data10223)) != kLegendre10223Checksum ||
        calculate_crc(std::span(g_data1499))  != kLegendre1499Checksum) {
        secure_scrub(g_data10223);
        secure_scrub(g_data1499);
        g_initialized = false;
        return LegendreStatus::kFaultDetected;
    }

    g_initialized = true;
    return LegendreStatus::kOk;
}

LegendreStatus generate_legendre_sequence(
    uint16_t           prime,
    std::span<uint8_t> out) noexcept {

    LegendreStatus status = LegendreStatus::kOk;
    // cppcheck-suppress variableScope
    uint32_t cfi_count = 0xA5A5A5A5U;

    if (out.size() < prime) {
        status = LegendreStatus::kOutputTooSmall;
    } else if (prime == 0) {
        status = LegendreStatus::kInvalidPrime;
    }

    if (status == LegendreStatus::kOk) {
        std::fill(out.begin(), out.end(), 0U);
        // cppcheck-suppress [variableScope, unreadVariable]
        uint32_t x_mir = 0xFFFFFFFFU;
        const auto limit = static_cast<uint16_t>((prime >> 1U) + 1U);

        for (uint16_t x = 1; x < limit; ++x) {
            x_mir = ~static_cast<uint32_t>(x);
            if (static_cast<uint32_t>(x) != (~x_mir)) [[unlikely]] {
                status = LegendreStatus::kFaultDetected;  // FIZ-1
                break;
            }

            const auto x32 = static_cast<uint32_t>(x);
            const auto res = static_cast<uint16_t>((x32 * x32) % static_cast<uint32_t>(prime));
            if (res < out.size()) {
                out[res] = 1U;
            }
            cfi_count++;
            wip_tick();
        }

        if (cfi_count != (0xA5A5A5A5U + (limit - 1U))) [[unlikely]] {
            status = LegendreStatus::kFaultDetected;  // FIZ-1
        }
    }

    if (status != LegendreStatus::kOk) {
        secure_scrub(out);
    }
    return status;
}

[[nodiscard]] std::span<const uint8_t> get_cached_legendre(uint16_t prime) noexcept {
    // Lazy init if POST has not yet run — plain bool, no __cxa_guard.
    if (!g_initialized) [[unlikely]] {
        if (legendre_cache_init() != LegendreStatus::kOk) {
            return {};
        }
    }

    // Verify against golden CRCs on every access.
    if (calculate_crc(std::span(g_data10223)) != kLegendre10223Checksum ||
        calculate_crc(std::span(g_data1499))  != kLegendre1499Checksum) [[unlikely]] {
        return {};
    }

    if (prime == kPrime10223) {
        return {g_data10223.data(), kPrime10223};
    }
    if (prime == kPrime1499) {
        return {g_data1499.data(), kPrime1499};
    }
    return {};
}

uint64_t legendre_10223_checksum() noexcept {
    if (!g_initialized) [[unlikely]] {
        (void)legendre_cache_init();
    }
    return calculate_crc(std::span(g_data10223));
}

uint64_t legendre_1499_checksum() noexcept {
    if (!g_initialized) [[unlikely]] {
        (void)legendre_cache_init();
    }
    return calculate_crc(std::span(g_data1499));
}

} // namespace lunalink::signal
