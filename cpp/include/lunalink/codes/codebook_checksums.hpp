// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>

namespace lunalink::signal {

/**
 * @brief Golden checksum constants for BCH and Weil codebook integrity (POST step 2 / CBIT).
 *
 * These values are computed once by running:
 *   task compute-golden-checksums
 * and committed to git alongside the codebook tables.  They are regenerated only
 * when the underlying PRN/BCH tables change.
 *
 * POST step 2 compares the runtime CRC-32 checksum of each codebook against these
 * constants.  A mismatch indicates single-event upset corruption of the table and
 * triggers FaultStatus::kFaultDetected.
 *
 * Algorithm: CRC-32 (IEEE 802.3, poly 0xEDB88320, reflected) over all packed bytes.
 * Return type is uint64_t.  The upper 32 bits are 0xFFFFFFFF because the checksum
 * functions return ~static_cast<uint64_t>(crc32), i.e. the full bitwise complement
 * of a 32-bit CRC value extended to 64 bits.
 */

// Golden values — recompute with `task compute-golden-checksums` if tables change.
inline constexpr uint64_t kBchCodebookChecksum = 0xfffffffff7c352e2ULL;
inline constexpr uint64_t kWeil10230Checksum = 0xffffffff780e10a7ULL;
inline constexpr uint64_t kWeil1500Checksum = 0xffffffff031ee19dULL;
// Legendre sequences are generated at runtime — golden CRCs verify generation correctness.
// Recompute with `task compute-golden-checksums` if generate_legendre_sequence() changes.
inline constexpr uint64_t kLegendre10223Checksum = 0xffffffff29cddcc8ULL;
inline constexpr uint64_t kLegendre1499Checksum = 0xffffffff6f7f944fULL;
// BeiDou B1C secondary codes use Legendre prime p=3607 (BDS-SIS-ICD-B1C-1.0 §5.2.2).
// Recompute with `task compute-golden-checksums` if generate_legendre_sequence() changes.
inline constexpr uint64_t kLegendre3607Checksum = 0xffffffff719927f1ULL;
// BeiDou B1C Weil-10230 uses Legendre prime p=10243 (BDS-SIS-ICD-B1C-1.0).
// Recompute with `task compute-golden-checksums` if generate_legendre_sequence() changes.
inline constexpr uint64_t kLegendre10243Checksum = 0xffffffff8d63e0cbULL;

} // namespace lunalink::signal
