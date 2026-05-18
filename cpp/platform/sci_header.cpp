#include "lunalink/platform/sci_header.hpp"

#include "lunalink/core/crc32.hpp"

#include "sci_header_generated.hpp"

#include <cstddef>
#include <cstdint>

namespace lunalink::signal {

// NOLINTBEGIN(hicpp-avoid-c-arrays) -- struct with char[] fields requires C-array layout
LUNALINK_LUT_SECTION
constinit const SciHeader kSciHeader = {
    LUNALINK_GIT_SHA,
    LUNALINK_LSIS_VERSION,
    LUNALINK_VERSION,
    LUNALINK_BUILD_TIMESTAMP,
    LUNALINK_SCI_CRC32,
};
// NOLINTEND(hicpp-avoid-c-arrays)

bool sci_header_verify() noexcept {
  // Compute CRC-32 over the first four fields of kSciHeader.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- byte-wise CRC over POD struct
  const auto* raw = reinterpret_cast<const uint8_t*>(&kSciHeader);
  constexpr size_t payload_len = sizeof(kSciHeader.git_sha) + sizeof(kSciHeader.lsis_version) +
                                 sizeof(kSciHeader.version) + sizeof(kSciHeader.build_timestamp);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- constructing span from POD
  // struct raw bytes
  const auto crc = crc32_ieee({raw, payload_len});
  return crc == kSciHeader.header_crc32; // FIZ-3
}

} // namespace lunalink::signal
