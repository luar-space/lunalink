#pragma once
#include "lunalink/messages/frame.hpp"

#include <cstdint>
#include <span>

namespace lunalink::signal {

// ─── Constants ───────────────────────────────────────────────────────────────

/// SB3/SB4 type field width in bits.
/// INTEROP RISK: LSIS V1.0 Tables 19/20 mark this as TBC ({LSIS-TBC-2023}/{LSIS-TBC-2024}):
/// "4 or 6" bits. LunaLink uses 6 bits because Table 21 MSG IDs go up to 32
/// (requires 6 bits). If another implementation uses 4 bits, type values > 15
/// will be unrepresentable and all subsequent field offsets will differ by 2 bits.
/// See spec finding SF-005 and docs/gateway6.rst for details.
inline constexpr uint8_t kSubframeTypeBits = 6;

/// WN field width in bits (LSIS-720, Table 22).
inline constexpr uint8_t kWnBits = 13;
/// ITOW field width in bits (LSIS-720, Table 22).
inline constexpr uint8_t kItowBits = 9;
/// Total ToT header bits in SB2 (WN + ITOW).
inline constexpr uint8_t kTotHeaderBits = kWnBits + kItowBits;

// ─── SB2 Field Layout (sb2_bit_layout.csv, §2.4.3.1.6) ─────────────────────
// Total: 1176 data bits.

/// Health and Safety status field: 8 bits at offset 22.
inline constexpr uint8_t kSb2HealthOffset = kTotHeaderBits; // 22
inline constexpr uint8_t kSb2HealthBits = 8;

/// Clock and Ephemeris Data (CED): 896 bits at offset 30.
inline constexpr uint8_t kSb2CedOffset = kSb2HealthOffset + kSb2HealthBits; // 30
inline constexpr uint16_t kSb2CedBits = 896;

/// Time Conversions: 224 bits at offset 926.
inline constexpr uint16_t kSb2TimeConvOffset = kSb2CedOffset + kSb2CedBits; // 926
inline constexpr uint16_t kSb2TimeConvBits = 224;

/// Spare bits: 26 bits at offset 1150 (alternating 0/1 per LSIS-300).
inline constexpr uint16_t kSb2SpareOffset = kSb2TimeConvOffset + kSb2TimeConvBits; // 1150
inline constexpr uint8_t kSb2SpareBits = 26;

// ─── Enums ───────────────────────────────────────────────────────────────────

/** @brief LSIS message types from Table 21 (LNIS V1.0).
 * Values match the spec-defined MSG IDs; not SEU-hardened (spec-constrained). */
enum class MessageType : uint8_t {
  kG1 = 0x01U,      ///< LunaNet Network Access Information
  kG2 = 0x02U,      ///< Health and Safety
  kG3 = 0x03U,      ///< Antenna Properties (Multiple)
  kG4 = 0x04U,      ///< Clock and Ephemeris Data (CED)
  kG5 = 0x05U,      ///< Orbit Almanac (Multiple)
  kG8 = 0x08U,      ///< Time of Transmission (ToT)
  kG10 = 0x0AU,     ///< Maneuver
  kG11 = 0x0BU,     ///< Attitude State/Ephemeris (Single)
  kG14 = 0x0EU,     ///< Conjunction (not LunaNet 1.0)
  kG15 = 0x0FU,     ///< Maplet (not LunaNet 1.0)
  kG17 = 0x11U,     ///< Ancillary Info
  kS19 = 0x13U,     ///< LunaSAR Automatic Acknowledge (not LunaNet 1.0)
  kS20 = 0x14U,     ///< LunaSAR Return Message
  kG22 = 0x16U,     ///< Acknowledge of non-SAR MSG (not LunaNet 1.0)
  kG23 = 0x17U,     ///< GNSS Augmentation (not LunaNet 1.0)
  kG24 = 0x18U,     ///< Detection Alert (not LunaNet 1.0)
  kG25 = 0x19U,     ///< Science (not LunaNet 1.0)
  kG27 = 0x1BU,     ///< UIS Response (not LunaNet 1.0)
  kG28 = 0x1CU,     ///< User Schedule Notice
  kG29 = 0x1DU,     ///< FF Commands (not LunaNet 1.0)
  kG30 = 0x1EU,     ///< Time Conversions
  kG31 = 0x1FU,     ///< Augmentation Differential Corrections
  kG32 = 0x20U,     ///< Coordinate Frame Conversions
  kUnknown = 0xFFU, ///< Unknown or unrecognised type field
};

/** @brief Status codes for message parsing operations.
 * SEU-hardened with high Hamming distance bit patterns. */
enum class MessageParseStatus : uint8_t {
  kOk = 0x5AU,            ///< Parsing completed successfully.
  kInvalidInput = 0xA5U,  ///< Input span has wrong size or is null.
  kFaultDetected = 0x99U, ///< CFI or integrity check failed.
};

// ─── Result Structs ──────────────────────────────────────────────────────────

/// Number of SB2 payload bits after WN + ITOW (1176 - 22 = 1154).
/// Contains CED (MSG-G4), Health (MSG-G2), Time Conversions (MSG-G30),
/// and spare bits — all TBW in LSIS V1.0 (§2.4.3.1.6, LSIS-FID0-520).
inline constexpr uint16_t kSb2PayloadBits = kSb2DataBits - kTotHeaderBits;

/// Number of SB3 payload bits after the type field (846 - 6 = 840).
/// Contains variable message data routed by type (§2.4.3.1.7, LSIS-FID0-570).
inline constexpr uint16_t kSb3PayloadBits = kSb3DataBits - kSubframeTypeBits;

/// Number of SB4 payload bits after the type field (846 - 6 = 840).
/// Contains variable message data routed by type (§2.4.3.1.8, LSIS-FID0-610).
inline constexpr uint16_t kSb4PayloadBits = kSb4DataBits - kSubframeTypeBits;

/// Parsed Subframe 2 result. [LSIS-AFS-801]
///
/// WN and ITOW are extracted per Table 22 (LSIS-720).
/// The remaining 1154 payload bits contain (per §2.4.3.1.6, LSIS-FID0-520):
///   - Clock and Ephemeris Data (MSG-G4, {LSIS-TBW-2006})
///   - Health and Safety (MSG-G2, {LSIS-TBW-2005})
///   - Time Conversions (MSG-G30, {LSIS-TBW-2012})
///   - LNSP-specific data (LSIS-FID0-540 [FLEX])
///   - Mandatory spare bits (LSIS-FID0-550, {LSIS-TBC-2022})
///   - CRC-24 guard covers all above including spares (LSIS-FID0-465)
///
/// Field boundaries within the payload are TBW in LSIS V1.0.
/// Callers access the raw payload via sb2_data[data_offset .. data_offset + payload_size).
struct Sb2ParseResult {
  MessageParseStatus status = MessageParseStatus::kFaultDetected;
  uint16_t wn = 0;                         ///< Week Number (13 bits, 0..8191)
  uint16_t itow = 0;                       ///< Interval Time of Week (9 bits, 0..503)
  uint16_t data_offset = kTotHeaderBits;   ///< Bit offset to payload start
  uint16_t payload_size = kSb2PayloadBits; ///< Payload size in bits (1154)
};

/// Parsed Subframe 3 result. [LSIS-AFS-802]
///
/// The type field routes to the message payload (§2.4.3.1.7, LSIS-FID0-570).
/// Fundamental SB3 content (LSIS-FID0-580): Orbit Almanac (MSG-G5).
/// Optional SB3 content (LSIS-FID0-590): Antenna Properties (MSG-G3),
///   GNSS Augmentation (MSG-G23), Maneuver (MSG-G10), Attitude/Ephemeris
///   (MSG-G11), Conjunction (MSG-G14), Aug. Diff. Corrections (MSG-G31),
///   LunaSAR (MSG-S19/S20), Alerts (MSG-G24), Coord. Frame Conv. (MSG-G32),
///   and custom LNSP messages.
///
/// Callers access the raw payload via sb3_data[data_offset .. data_offset + payload_size).
/// Internal field layouts are TBW / SISICD-defined per message type.
struct Sb3ParseResult {
  MessageParseStatus status = MessageParseStatus::kFaultDetected;
  uint8_t type_raw = 0;                     ///< Raw type field value (6 bits)
  MessageType type = MessageType::kUnknown; ///< Mapped message type
  uint16_t data_offset = kSubframeTypeBits; ///< Bit offset to payload start
  uint16_t payload_size = kSb3PayloadBits;  ///< Payload size in bits (840)
};

/// Parsed Subframe 4 result. [LSIS-AFS-803]
///
/// The type field routes to the message payload (§2.4.3.1.8, LSIS-FID0-610).
/// Fundamental SB4 content (LSIS-FID0-620): Network Access Info (MSG-G1).
/// Optional SB4 content (LSIS-FID0-630): Antenna Properties (MSG-G3),
///   Orbit Almanac (MSG-G5), Maneuver (MSG-G10), Attitude/Ephemeris (MSG-G11),
///   GNSS Augmentation (MSG-G23), User Schedule (MSG-G28), Ancillary (MSG-G17),
///   Conjunction (MSG-G14), Maplet (MSG-G15), Science (MSG-G25),
///   Non-SAR Ack (MSG-G22), UIS Response (MSG-G27), FF Commands (MSG-G29),
///   Aug. Diff. Corrections (MSG-G31), Coord. Frame Conv. (MSG-G32),
///   and custom LNSP messages.
///
/// Callers access the raw payload via sb4_data[data_offset .. data_offset + payload_size).
/// Internal field layouts are TBW / SISICD-defined per message type.
struct Sb4ParseResult {
  MessageParseStatus status = MessageParseStatus::kFaultDetected;
  uint8_t type_raw = 0;                     ///< Raw type field value (6 bits)
  MessageType type = MessageType::kUnknown; ///< Mapped message type
  uint16_t data_offset = kSubframeTypeBits; ///< Bit offset to payload start
  uint16_t payload_size = kSb4PayloadBits;  ///< Payload size in bits (840)
};

// ─── Functions ───────────────────────────────────────────────────────────────

/// Extract N bits from a bit array starting at bit_offset, MSB-first. [LSIS-AFS-320]
///
/// Each element of `bits` is a single bit (0 or 1). The extracted value is
/// packed MSB-first into a uint32_t. Maximum 32 bits can be extracted.
///
/// @param bits       Input bit array (one bit per byte, values 0 or 1).
/// @param bit_offset Starting bit position (0-based).
/// @param n_bits     Number of bits to extract (1..32).
/// @param out        Output value. Set to 0 on error.
/// @return kOk on success, kInvalidInput if out of bounds.
[[nodiscard("extract_bits returns kInvalidInput on bit-range overflow; ignoring it leaves "
            "`out` indeterminate and propagates corrupt navigation data")]] MessageParseStatus
extract_bits(std::span<const uint8_t> bits,
             uint16_t bit_offset,
             uint8_t n_bits,
             uint32_t& out) noexcept;

/// Parse Subframe 2: extract WN and ITOW from decoded SB2 data. [LSIS-AFS-801]
///
/// Per LSIS-720 (Table 22), WN is 13 bits and ITOW is 9 bits.
/// Bit positions within SB2 are assumed MSB-first at offset 0 (see SF-005).
///
/// @param sb2_data  Decoded SB2 navigation data (1176 bits, one bit per byte).
/// @param result    Output parse result.
/// @return MessageParseStatus indicating outcome.
[[nodiscard("parse_sb2 status reports LSIS-720/Table-22 field-range violations; "
            "ignoring it lets malformed WN/ITOW reach the navigation solution")]] MessageParseStatus
parse_sb2(std::span<const uint8_t> sb2_data, Sb2ParseResult& result) noexcept;

/// Parse Subframe 3: extract type field and compute data offset. [LSIS-AFS-802]
///
/// The type field identifies which message is carried (Table 19, §2.4.3.1.7).
///
/// @param sb3_data  Decoded SB3 navigation data (846 bits, one bit per byte).
/// @param result    Output parse result.
/// @return MessageParseStatus indicating outcome.
[[nodiscard(
    "parse_sb3 status reports unrecognised type fields or LSIS-Table-19 "
    "violations; ignoring it leads to mis-routing of subframe payloads")]] MessageParseStatus
parse_sb3(std::span<const uint8_t> sb3_data, Sb3ParseResult& result) noexcept;

/// Parse Subframe 4: extract type field and compute data offset. [LSIS-AFS-803]
///
/// The type field identifies which message is carried (Table 20, §2.4.3.1.8).
///
/// @param sb4_data  Decoded SB4 navigation data (846 bits, one bit per byte).
/// @param result    Output parse result.
/// @return MessageParseStatus indicating outcome.
[[nodiscard(
    "parse_sb4 status reports unrecognised type fields or LSIS-Table-20 "
    "violations; ignoring it leads to mis-routing of subframe payloads")]] MessageParseStatus
parse_sb4(std::span<const uint8_t> sb4_data, Sb4ParseResult& result) noexcept;

/// Validate spare bits follow the alternating 0/1 pattern (LSIS-300).
///
/// Per LSIS-300: spare bits shall be a sequence of alternating zeros and ones
/// starting with zero on the MSB.
///
/// @param bits    Span of bits to validate (one bit per byte).
/// @return true if the pattern is correct, false otherwise.
[[nodiscard("validate_spare_bits returns false on LSIS-300 spare-bit pattern violation; "
            "ignoring it masks a spec-compliance failure that may indicate corruption")]] bool
validate_spare_bits(std::span<const uint8_t> bits) noexcept;

/// Map a raw 6-bit type field value to a MessageType enum.
///
/// @param raw_type  Raw type field value (0..63).
/// @return Corresponding MessageType, or kUnknown if not recognised.
[[nodiscard("type_field_to_message_type returns kUnknown for unrecognised raw types; "
            "ignoring it dispatches the wrong parser and mis-routes the message")]] MessageType
type_field_to_message_type(uint8_t raw_type) noexcept;

} // namespace lunalink::signal
