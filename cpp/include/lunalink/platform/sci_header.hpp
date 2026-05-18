#pragma once
#include "lunalink/core/safety.hpp"

#include <cstdint>

namespace lunalink::signal {

/**
 * @brief Software Configuration Identification (SCI) header.
 * Embedded in the binary at link time. POST step 1 verifies the CRC-32.
 * [SW-SAF-04, FM-034]
 */
LUNALINK_PACKED_BEGIN
struct LUNALINK_PACKED_ATTR SciHeader {
  // NOLINTBEGIN(hicpp-avoid-c-arrays) — C-style char arrays required for packed
  // binary layout and direct CRC computation over raw bytes; std::array<char,N>
  // cannot guarantee the same ABI without additional indirection.
  char git_sha[41];      // null-terminated hex SHA-1 (40 chars + NUL)
  char lsis_version[16]; // e.g. "LSIS-V1.0-2025\0\0"
  char version[24];      // git describe, e.g. "v0.1.8-47-gabcdef0\0..."
  // NOLINTEND(hicpp-avoid-c-arrays)
  uint32_t build_timestamp; // seconds since J2000 (2000-01-01T12:00:00 UTC)
  uint32_t header_crc32;    // CRC-32 (IEEE 802.3) of the preceding four fields
};
LUNALINK_PACKED_END

static_assert(sizeof(SciHeader) == 41U + 16U + 24U + 4U + 4U,
              "SciHeader size must be exactly 89 bytes");

/// The SCI header is embedded in the binary via a dedicated linker section.
LUNALINK_LUT_SECTION
extern const SciHeader kSciHeader;

/**
 * @brief POST Step 1: verify kSciHeader.header_crc32 matches the preceding fields.
 * Returns false (FIZ-3) if the CRC does not match — indicates binary corruption.
 */
[[nodiscard("sci_header_verify returns false when the SCI header CRC-32 mismatches "
              "the binary image — POST step 1 is the binary-integrity gate; ignoring "
              "it would let OPERATIONAL be entered on a corrupt image "
              "(SW-SAF-04, FM-034)")]] bool
sci_header_verify() noexcept;

} // namespace lunalink::signal
