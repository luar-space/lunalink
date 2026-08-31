#pragma once
#include "lunalink/codes/prn.hpp"
#include "lunalink/core/safety.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

/**
 * @brief Weil code generator (LSIS Section 2.3.5.2).
 */
class WeilGenerator {
public:
  WeilGenerator() noexcept = default;

  /**
   * @brief Generate Primary Weil code (10230 chips). [LSIS-AFS-102]
   */
  [[nodiscard("PrnStatus reports PRN-range and buffer-size failures from the "
              "Weil-10230 primary construction; ignoring it lets a zero/partial code "
              "reach the modulator (LSIS-AFS-102)")]] PrnStatus
  generate_primary(PrnId prn_id, std::span<uint8_t> out) noexcept;

  /**
   * @brief Generate Tertiary Weil code (1500 chips). [LSIS-AFS-102]
   */
  [[nodiscard("PrnStatus reports PRN-range and buffer-size failures from the "
              "Weil-1500 tertiary construction; ignoring it lets a zero/partial code "
              "reach the modulator (LSIS-AFS-102)")]] PrnStatus
  generate_tertiary(PrnId prn_id, std::span<uint8_t> out) noexcept;

private:
  /**
   * @brief Internal core Weil construction logic.
   *
   * @param prime       Legendre sequence prime (10223 for primary, 1499 for tertiary).
   * @param weil_index  Weil shift parameter w from LSIS Table 8/9.
   * @param insertion_index  Bit insertion position for sequence extension (LSIS §2.3.5.2).
   * @param out         Output buffer for the generated code chips.
   */
  [[nodiscard("PrnStatus reports kInvalidPrime, parameter range and buffer-size "
              "failures from Weil construction; ignoring it lets a partial code "
              "propagate to the caller (LSIS-AFS-102)")]] PrnStatus
  construct_weil(uint16_t prime,
                 uint16_t weil_index,
                 uint16_t insertion_index,
                 std::span<uint8_t> out) noexcept;
};

} // namespace lunalink::signal
