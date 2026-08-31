#pragma once
/// @file file_format.hpp
/// @brief LSIS-AFS binary file format definitions (file_format_headers.csv).
///
/// Four spec-defined interchange formats for interoperability test vectors:
///   - IQSG: float32 I/Q signal
///   - IQS2: int16 I/Q signal (scaled to ±32767)
///   - LSIS: uint8 spreading code
///   - FRAM: uint8 frame symbols

#include <cstdint>

namespace lunalink::signal {

// ─── File Format Magic Numbers ──────────────────────────────────────────────

/// IQSG: float32 I/Q signal file.
inline constexpr uint32_t kMagicIqsg = 0x49515347U; // "IQSG"
/// IQS2: int16 I/Q signal file (scaled to ±32767).
inline constexpr uint32_t kMagicIqs2 = 0x49515332U; // "IQS2"
/// LSIS: uint8 spreading code file.
inline constexpr uint32_t kMagicLsis = 0x4C534953U; // "LSIS"
/// FRAM: uint8 frame symbols file.
inline constexpr uint32_t kMagicFram = 0x4652414DU; // "FRAM"

// ─── Header Sizes ───────────────────────────────────────────────────────────

/// IQSG/IQS2 header: magic(4) + version(4) + sample_rate(4) + prn(4) + reserved(16) = 32 bytes.
inline constexpr uint8_t kIqHeaderBytes = 32;
/// LSIS spreading code header: magic(4) + prn(2) + code_type(2) + length(4) + reserved(4) = 16
/// bytes.
inline constexpr uint8_t kLsisHeaderBytes = 16;
/// FRAM frame header: magic(4) + version(4) + frame_length(4) + prn(4) = 16 bytes.
inline constexpr uint8_t kFramHeaderBytes = 16;

} // namespace lunalink::signal
