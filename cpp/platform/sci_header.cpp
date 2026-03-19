#include "lunalink/platform/sci_header.hpp"
#include "sci_header_generated.hpp"

#include <cstddef>
#include <cstdint>

namespace lunalink::signal {

// NOLINTBEGIN(hicpp-avoid-c-arrays)
LUNALINK_LUT_SECTION
const SciHeader kSciHeader = {
    LUNALINK_GIT_SHA,
    LUNALINK_LSIS_VERSION,
    LUNALINK_VERSION,
    LUNALINK_BUILD_TIMESTAMP,
    LUNALINK_SCI_CRC32,
};
// NOLINTEND(hicpp-avoid-c-arrays)

namespace {
// CRC-32 (IEEE 802.3, reflected polynomial 0xEDB88320).
// Duplicated locally to avoid a dependency on ldpc_tables.cpp's internal helper.
uint32_t sci_crc32_byte(uint32_t crc, uint8_t byte) noexcept {
    crc ^= byte;
    for (uint32_t j = 0U; j < 8U; ++j) {
        crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
    }
    return crc;
}
} // namespace

bool sci_header_verify() noexcept {
    // Compute CRC-32 over the first four fields of kSciHeader.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* raw = reinterpret_cast<const uint8_t*>(&kSciHeader);
    constexpr size_t payload_len =
        sizeof(kSciHeader.git_sha) +
        sizeof(kSciHeader.lsis_version) +
        sizeof(kSciHeader.version) +
        sizeof(kSciHeader.build_timestamp);

    uint32_t crc = 0xFFFFFFFFU;
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (size_t i = 0U; i < payload_len; ++i) {
        crc = sci_crc32_byte(crc, raw[i]);
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    crc = ~crc;
    return crc == kSciHeader.header_crc32;  // FIZ-3
}

}  // namespace lunalink::signal
