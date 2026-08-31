// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/legendre.hpp"

#include "lunalink/codes/codebook_checksums.hpp"
#include "lunalink/core/crc32.hpp"
#include "lunalink/core/safety.hpp"

#include <algorithm>
#include <array>

namespace lunalink::signal {

namespace {

/// CRC-32 over a byte span, returned as uint64_t for golden-checksum compatibility.
/// Upper 32 bits are 0xFFFFFFFF (see codebook_checksums.hpp).
uint64_t crc32_ieee_u64(std::span<const uint8_t> data) noexcept {
  return ~static_cast<uint64_t>(~crc32_ieee(data));
}

// Legendre cache — file-scope, no hidden __cxa_guard initialization.
// Must be explicitly initialized via legendre_cache_init() before use.
constexpr uint16_t kPrime10223 = 10223;
constexpr uint16_t kPrime1499 = 1499;
constexpr uint16_t kPrime3607 = 3607;
constexpr uint16_t kPrime10243 = 10243;

alignas(64) std::array<uint8_t, kPrime10223> g_data10223{};
alignas(64) std::array<uint8_t, kPrime1499> g_data1499{};
alignas(64) std::array<uint8_t, kPrime3607> g_data3607{};
alignas(64) std::array<uint8_t, kPrime10243> g_data10243{};

// P0-7: NOT atomic by design.  legendre_cache_init() must be called once from
// Power-On Self-Test (POST) on the boot core *before* any concurrent reader
// accesses get_cached_legendre().  All production call-sites satisfy this
// precondition; the lazy-init fallback in get_cached_legendre() exists only as
// a defensive backstop for unit-test harnesses that skip POST.
bool g_initialized = false;

} // namespace

LegendreStatus legendre_cache_init() noexcept {
  // Generate both Legendre sequences.
  const auto s1 = generate_legendre_sequence(kPrime10223, std::span(g_data10223));
  if (s1 != LegendreStatus::kOk) [[unlikely]] {
    secure_scrub(g_data10223); // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CRC mismatch on Legendre cache
                               // init — scrub + return failure
    secure_scrub(g_data1499);
    secure_scrub(g_data3607);
    secure_scrub(g_data10243);
    g_initialized = false;
    return s1;
  } // NOCOVER_END

  const auto s2 = generate_legendre_sequence(kPrime1499, std::span(g_data1499));
  if (s2 != LegendreStatus::kOk) [[unlikely]] {
    secure_scrub(g_data10223); // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CRC mismatch on Legendre cache
                               // init — scrub + return failure
    secure_scrub(g_data1499);
    secure_scrub(g_data3607);
    secure_scrub(g_data10243);
    g_initialized = false;
    return s2;
  } // NOCOVER_END

  const auto s2b = generate_legendre_sequence(kPrime3607, std::span(g_data3607));
  if (s2b != LegendreStatus::kOk) [[unlikely]] {
    secure_scrub(g_data10223); // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CRC mismatch on Legendre cache
                               // init — scrub + return failure
    secure_scrub(g_data1499);
    secure_scrub(g_data3607);
    secure_scrub(g_data10243);
    g_initialized = false;
    return s2b;
  } // NOCOVER_END

  const auto s3 = generate_legendre_sequence(kPrime10243, std::span(g_data10243));
  if (s3 != LegendreStatus::kOk) [[unlikely]] {
    secure_scrub(g_data10223); // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CRC mismatch on Legendre cache
                               // init — scrub + return failure
    secure_scrub(g_data1499);
    secure_scrub(g_data3607);
    secure_scrub(g_data10243);
    g_initialized = false;
    return s3;
  } // NOCOVER_END

  // Verify against golden CRCs — reject corrupted generation.
  if (crc32_ieee_u64(std::span(g_data10223)) != kLegendre10223Checksum ||
      crc32_ieee_u64(std::span(g_data1499)) != kLegendre1499Checksum ||
      crc32_ieee_u64(std::span(g_data3607)) != kLegendre3607Checksum ||
      crc32_ieee_u64(std::span(g_data10243)) != kLegendre10243Checksum) {
    secure_scrub(g_data10223); // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CRC mismatch on Legendre cache
                               // init — scrub + return failure
    secure_scrub(g_data1499);
    secure_scrub(g_data3607);
    secure_scrub(g_data10243);
    g_initialized = false;
    return LegendreStatus::kFaultDetected;
  } // NOCOVER_END

  g_initialized = true;
  return LegendreStatus::kOk;
}

LegendreStatus generate_legendre_sequence(uint16_t prime, std::span<uint8_t> out) noexcept {
  LegendreStatus status = LegendreStatus::kOk;
  // cppcheck-suppress variableScope -- CFI counter must be at function scope for terminal check
  volatile uint32_t cfi_count = 0xA5A5A5A5U;

  if (out.size() < prime) {
    status = LegendreStatus::kOutputTooSmall;
  } else if (prime == 0) {
    status = LegendreStatus::kInvalidPrime;
  }

  if (status == LegendreStatus::kOk) {
    std::fill(out.begin(), out.end(), 0U);
    // cppcheck-suppress [variableScope, unreadVariable] -- mirror variable must be at function
    // scope for fault detection
    uint32_t x_mir = 0xFFFFFFFFU;
    const auto limit = static_cast<uint16_t>((prime >> 1U) + 1U);

    for (uint16_t x = 1; x < limit; ++x) {
      x_mir = ~static_cast<uint32_t>(x);
      if (static_cast<uint32_t>(x) != (~x_mir)) [[unlikely]] {
        status = LegendreStatus::kFaultDetected; // FIZ-1 // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CFI
                                                 // counter mismatch in generation loop
        break;
      } // NOCOVER_END

      const auto x32 = static_cast<uint32_t>(x);
      const auto res = static_cast<uint16_t>((x32 * x32) % static_cast<uint32_t>(prime));
      if (res < out.size()) {
        out[res] = 1U;
      }
      LUNALINK_CRITICAL_ENTER;
      cfi_count = cfi_count + 1U;
      LUNALINK_CRITICAL_EXIT;
      wip_tick();
    }

    if (cfi_count != (0xA5A5A5A5U + (limit - 1U))) [[unlikely]] {
      status = LegendreStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI terminal
                                               // check failure
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
      return {}; // NOCOVER(DEFENCE_IN_DEPTH): bounds/validity check on cache lookup
    }
  }

  // Verify against golden CRCs on every access — detects SEU between calls.
  // WCET note: CRC-32 over ~26 KB total (10223 + 1499 + 3607 + 10243 bytes).
  if (crc32_ieee_u64(std::span(g_data10223)) != kLegendre10223Checksum ||
      crc32_ieee_u64(std::span(g_data1499)) != kLegendre1499Checksum ||
      crc32_ieee_u64(std::span(g_data3607)) != kLegendre3607Checksum ||
      crc32_ieee_u64(std::span(g_data10243)) != kLegendre10243Checksum) [[unlikely]] {
    return {}; // NOCOVER(DEFENCE_IN_DEPTH): bounds/validity check on cache lookup
  }

  if (prime == kPrime10223) {
    return {g_data10223.data(), kPrime10223};
  }
  if (prime == kPrime1499) {
    return {g_data1499.data(), kPrime1499};
  }
  if (prime == kPrime3607) {
    return {g_data3607.data(), kPrime3607};
  }
  if (prime == kPrime10243) {
    return {g_data10243.data(), kPrime10243};
  }
  return {};
}

namespace {
void ensure_legendre_initialized() noexcept {
  if (!g_initialized) [[unlikely]] {
    (void)legendre_cache_init();
  }
}
} // namespace

uint64_t legendre_10223_checksum() noexcept {
  ensure_legendre_initialized();
  return crc32_ieee_u64(std::span(g_data10223));
}

uint64_t legendre_1499_checksum() noexcept {
  ensure_legendre_initialized();
  return crc32_ieee_u64(std::span(g_data1499));
}

uint64_t legendre_3607_checksum() noexcept {
  ensure_legendre_initialized();
  return crc32_ieee_u64(std::span(g_data3607));
}

uint64_t legendre_10243_checksum() noexcept {
  ensure_legendre_initialized();
  return crc32_ieee_u64(std::span(g_data10243));
}

} // namespace lunalink::signal
