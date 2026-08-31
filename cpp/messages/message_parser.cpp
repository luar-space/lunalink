/// @file message_parser.cpp
/// @brief Navigation message parsers for decoded subframe data. [LSIS-AFS-801/802/803]
///
/// Extracts structured fields from the raw bit arrays produced by frame_decode().
/// Per LSIS V1.0, most message field layouts are TBW — only WN, ITOW (Table 22)
/// and the SB3/SB4 type fields are fully specified.

#include "lunalink/messages/message_parser.hpp"

#include "lunalink/core/safety.hpp"

namespace lunalink::signal {

namespace {
/// CFI magic sentinel for message parser functions.
constexpr uint32_t kCfiMagicParse = 0x6B6B6B6BU;
} // namespace

// ─── extract_bits ────────────────────────────────────────────────────────────

[[nodiscard]] MessageParseStatus extract_bits(std::span<const uint8_t> bits,
                                              const uint16_t bit_offset,
                                              const uint8_t n_bits,
                                              uint32_t& out) noexcept {
  // [LSIS-AFS-320] MSB-first bit extraction.
  MessageParseStatus status = MessageParseStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicParse;
  out = 0;

  // Validate parameters
  bool ok = (n_bits > 0 && n_bits <= 32);
  if (ok) {
    const auto end = static_cast<uint32_t>(bit_offset) + static_cast<uint32_t>(n_bits);
    ok = (end <= bits.size());
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: input validated
    LUNALINK_CRITICAL_EXIT;
  } else {
    status = MessageParseStatus::kInvalidInput;
  }

  // Pack bits MSB-first into result
  uint32_t result = 0;
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    for (uint8_t i = 0; i < n_bits; ++i) {
      const auto bit_val = bits[static_cast<size_t>(bit_offset) + static_cast<size_t>(i)];
      if (bit_val > 1U) {
        ok = false;
        status = MessageParseStatus::kInvalidInput;
        break;
      }
      result = static_cast<uint32_t>(result << 1U) | static_cast<uint32_t>(bit_val);
    }
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: bits extracted
    LUNALINK_CRITICAL_EXIT;
    out = result;
  }

  // CFI terminal check: both milestones must have been reached.
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    if (cfi_count == kCfiMagicParse + 2U) {
      status = MessageParseStatus::kOk;
    } else {
      status = MessageParseStatus::kFaultDetected; // FIZ-1 // NOCOVER_BEGIN(DEFENCE_IN_DEPTH): CFI
                                                   // sentinel failure path — volatile counter
                                                   // always matches under nominal execution
      out = 0;
    } // NOCOVER_END
  }

  return status;
}

// ─── type_field_to_message_type ──────────────────────────────────────────────

[[nodiscard]] MessageType type_field_to_message_type(const uint8_t raw_type) noexcept {
  // Control-plane lookup — single-exit exemption per §Control Flow rule 5.
  // Pure function with no side effects, no state, no LSIS-AFS tag.
  // The type field encoding uses LSIS MSG ID values from Table 21.
  switch (raw_type) {
  case 1U:
    return MessageType::kG1;
  case 2U:
    return MessageType::kG2;
  case 3U:
    return MessageType::kG3;
  case 4U:
    return MessageType::kG4;
  case 5U:
    return MessageType::kG5;
  case 8U:
    return MessageType::kG8;
  case 10U:
    return MessageType::kG10;
  case 11U:
    return MessageType::kG11;
  case 14U:
    return MessageType::kG14;
  case 15U:
    return MessageType::kG15;
  case 17U:
    return MessageType::kG17;
  case 19U:
    return MessageType::kS19;
  case 20U:
    return MessageType::kS20;
  case 22U:
    return MessageType::kG22;
  case 23U:
    return MessageType::kG23;
  case 24U:
    return MessageType::kG24;
  case 25U:
    return MessageType::kG25;
  case 27U:
    return MessageType::kG27;
  case 28U:
    return MessageType::kG28;
  case 29U:
    return MessageType::kG29;
  case 30U:
    return MessageType::kG30;
  case 31U:
    return MessageType::kG31;
  case 32U:
    return MessageType::kG32;
  default:
    return MessageType::kUnknown;
  }
}

// ─── parse_sb2 ───────────────────────────────────────────────────────────────

[[nodiscard]] MessageParseStatus parse_sb2(std::span<const uint8_t> sb2_data,
                                           Sb2ParseResult& result) noexcept {
  // [LSIS-AFS-801] Parse Subframe 2: WN + ITOW extraction.
  result = Sb2ParseResult{};
  MessageParseStatus status = MessageParseStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicParse;

  bool ok = (sb2_data.size() == kSb2DataBits);
  if (!ok) {
    status = MessageParseStatus::kInvalidInput;
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: input validated
    LUNALINK_CRITICAL_EXIT;
  }

  // Extract WN (13 bits at offset 0) [LSIS-720, Table 22]
  uint32_t wn_raw = 0;
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    const auto s = extract_bits(sb2_data, 0, kWnBits, wn_raw);
    if (s != MessageParseStatus::kOk) {
      ok = false;
      status = s;
    }
  }

  // Extract ITOW (9 bits at offset 13) [LSIS-720, Table 22]
  uint32_t itow_raw = 0;
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    const auto s = extract_bits(sb2_data, kWnBits, kItowBits, itow_raw);
    if (s != MessageParseStatus::kOk) {
      ok = false;
      status = s;
    }
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: fields extracted
    LUNALINK_CRITICAL_EXIT;
    result.wn = static_cast<uint16_t>(wn_raw);
    result.itow = static_cast<uint16_t>(itow_raw);
    result.data_offset = kTotHeaderBits;
  }

  // CFI terminal check
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    if (cfi_count == kCfiMagicParse + 2U) {
      status = MessageParseStatus::kOk;
    } else {
      status = MessageParseStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI
                                                   // sentinel failure path — volatile counter
                                                   // always matches under nominal execution
    }
  }

  // Scrub result on failure
  if (status != MessageParseStatus::kOk) {
    secure_scrub_scalar(result.wn);
    secure_scrub_scalar(result.itow);
  }

  result.status = status;
  return status;
}

// ─── parse_sb3 ───────────────────────────────────────────────────────────────

[[nodiscard]] MessageParseStatus parse_sb3(std::span<const uint8_t> sb3_data,
                                           Sb3ParseResult& result) noexcept {
  // [LSIS-AFS-802] Parse Subframe 3: type field extraction.
  result = Sb3ParseResult{};
  MessageParseStatus status = MessageParseStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicParse;

  bool ok = (sb3_data.size() == kSb3DataBits);
  if (!ok) {
    status = MessageParseStatus::kInvalidInput;
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: input validated
    LUNALINK_CRITICAL_EXIT;
  }

  // Extract type field (6 bits at offset 0)
  uint32_t type_raw = 0;
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    const auto s = extract_bits(sb3_data, 0, kSubframeTypeBits, type_raw);
    if (s != MessageParseStatus::kOk) {
      ok = false;
      status = s;
    }
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: type extracted
    LUNALINK_CRITICAL_EXIT;
    result.type_raw = static_cast<uint8_t>(type_raw);
    result.type = type_field_to_message_type(result.type_raw);
    result.data_offset = kSubframeTypeBits;
  }

  // CFI terminal check
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    if (cfi_count == kCfiMagicParse + 2U) {
      status = MessageParseStatus::kOk;
    } else {
      status = MessageParseStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI
                                                   // sentinel failure path — volatile counter
                                                   // always matches under nominal execution
    }
  }

  // Scrub result on failure
  if (status != MessageParseStatus::kOk) {
    secure_scrub_scalar(result.type_raw);
  }

  result.status = status;
  return status;
}

// ─── parse_sb4 ───────────────────────────────────────────────────────────────

[[nodiscard]] MessageParseStatus parse_sb4(std::span<const uint8_t> sb4_data,
                                           Sb4ParseResult& result) noexcept {
  // [LSIS-AFS-803] Parse Subframe 4: type field extraction.
  result = Sb4ParseResult{};
  MessageParseStatus status = MessageParseStatus::kFaultDetected; // Pessimistic default
  volatile uint32_t cfi_count = kCfiMagicParse;

  bool ok = (sb4_data.size() == kSb4DataBits);
  if (!ok) {
    status = MessageParseStatus::kInvalidInput;
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 1: input validated
    LUNALINK_CRITICAL_EXIT;
  }

  // Extract type field (6 bits at offset 0)
  uint32_t type_raw = 0;
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    const auto s = extract_bits(sb4_data, 0, kSubframeTypeBits, type_raw);
    if (s != MessageParseStatus::kOk) {
      ok = false;
      status = s;
    }
  }

  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    LUNALINK_CRITICAL_ENTER;
    cfi_count = cfi_count + 1U; // Milestone 2: type extracted
    LUNALINK_CRITICAL_EXIT;
    result.type_raw = static_cast<uint8_t>(type_raw);
    result.type = type_field_to_message_type(result.type_raw);
    result.data_offset = kSubframeTypeBits;
  }

  // CFI terminal check
  // cppcheck-suppress duplicateCondition  ; single-exit accumulator pattern
  if (ok) {
    if (cfi_count == kCfiMagicParse + 2U) {
      status = MessageParseStatus::kOk;
    } else {
      status = MessageParseStatus::kFaultDetected; // FIZ-1 // NOCOVER(DEFENCE_IN_DEPTH): CFI
                                                   // sentinel failure path — volatile counter
                                                   // always matches under nominal execution
    }
  }

  // Scrub result on failure
  if (status != MessageParseStatus::kOk) {
    secure_scrub_scalar(result.type_raw);
  }

  result.status = status;
  return status;
}

// ─── validate_spare_bits ─────────────────────────────────────────────────────

[[nodiscard]] bool validate_spare_bits(std::span<const uint8_t> bits) noexcept {
  // [LSIS-AFS-300] Spare bits: alternating 0/1, starting with 0 on MSB.
  if (bits.empty()) {
    return false; // Empty spare region is a caller error, not vacuously valid
  }
  for (size_t i = 0; i < bits.size(); ++i) {
    const auto expected = static_cast<uint8_t>(i % 2U);
    if (bits[i] != expected) {
      return false;
    }
  }
  return true;
}

} // namespace lunalink::signal
