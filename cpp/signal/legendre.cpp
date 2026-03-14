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

/**
 * @brief Thread-safe, hardened cache for Legendre sequences.
 * Mandated by Class B FSW: Generate once, verify against golden CRCs.
 */
class LegendreCache {
public:
    static constexpr uint16_t kPrime10223 = 10223;
    static constexpr uint16_t kPrime1499  = 1499;

    LegendreCache() noexcept {
        (void)generate_legendre_sequence(kPrime10223, std::span(data10223_));
        (void)generate_legendre_sequence(kPrime1499,  std::span(data1499_));
        initialized_ = true;
    }

    [[nodiscard]] std::span<const uint8_t> get_10223() const noexcept {
        return {data10223_.data(), kPrime10223};
    }

    [[nodiscard]] std::span<const uint8_t> get_1499() const noexcept {
        return {data1499_.data(), kPrime1499};
    }

    // Verify against build-time golden CRCs — not self-signed.
    [[nodiscard]] bool verify_integrity() const noexcept {
        return (calculate_crc(std::span(data10223_)) == kLegendre10223Checksum) &&
               (calculate_crc(std::span(data1499_)) == kLegendre1499Checksum);
    }

    [[nodiscard]] uint64_t crc_10223() const noexcept {
        return calculate_crc(std::span(data10223_));
    }

    [[nodiscard]] uint64_t crc_1499() const noexcept {
        return calculate_crc(std::span(data1499_));
    }

private:
    // Pinned to high-integrity sections if supported by linker
    alignas(64) std::array<uint8_t, kPrime10223> data10223_{};
    alignas(64) std::array<uint8_t, kPrime1499>  data1499_{};
    bool initialized_{false};
};

// Global static instance (constructed on first use)
LegendreCache& get_cache() noexcept {
    static LegendreCache cache;
    return cache;
}
} // namespace

LegendreStatus generate_legendre_sequence(
    uint16_t           prime,
    std::span<uint8_t> out) noexcept {
    
    LegendreStatus status = LegendreStatus::kOk;
    uint32_t cfi_count = 0xA5A5A5A5U;

    if (out.size() < prime) {
        status = LegendreStatus::kOutputTooSmall;
    } else if (prime == 0) {
        status = LegendreStatus::kInvalidPrime;
    }

    if (status == LegendreStatus::kOk) {
        std::fill(out.begin(), out.end(), 0U);
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
    auto& cache = get_cache();
    if (!cache.verify_integrity()) [[unlikely]] {
        // In Class A FSW, we might trigger a hardware reset here.
        // For now, return empty span to trigger downstream failure.
        return {};
    }

    if (prime == 10223) {
        return cache.get_10223();
    }
    if (prime == 1499) {
        return cache.get_1499();
    }
    return {};
}

uint64_t legendre_10223_checksum() noexcept {
    return get_cache().crc_10223();
}

uint64_t legendre_1499_checksum() noexcept {
    return get_cache().crc_1499();
}

} // namespace lunalink::signal
