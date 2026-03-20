#pragma once
#include <cstdint>
#include <span>

namespace lunalink::signal {

/// IEEE CRC-32 (reflected polynomial 0xEDB88320) — single byte update.
/// Use when feeding bytes individually (e.g. CRC over non-contiguous fields).
/// Caller must initialise crc to 0xFFFFFFFFU and apply final ~crc.
[[nodiscard]] constexpr uint32_t crc32_ieee_byte(uint32_t crc, uint8_t byte) noexcept {
  crc ^= byte;
  for (uint32_t j = 0; j < 8U; ++j) {
    crc = (crc >> 1U) ^ (0xEDB88320U & (-(crc & 1U)));
  }
  return crc;
}

/// IEEE CRC-32 (reflected polynomial 0xEDB88320) over a contiguous byte span.
/// Returns the finalised CRC value.
[[nodiscard]] constexpr uint32_t crc32_ieee(std::span<const uint8_t> data) noexcept {
  uint32_t crc = 0xFFFFFFFFU;
  for (const auto byte : data) {
    crc = crc32_ieee_byte(crc, byte);
  }
  return ~crc;
}

} // namespace lunalink::signal
