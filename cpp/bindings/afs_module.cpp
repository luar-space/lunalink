// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/legendre.hpp"
#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn_codes.hpp"
#include "lunalink/core/sise.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/messages/message_parser.hpp"
#include "lunalink/messages/time_of_transmission.hpp"
#include "lunalink/signal/file_format.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/signal_gen.hpp"
#include "lunalink/signal/sync_detector.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/unique_ptr.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;
using namespace lunalink::signal;

// Untyped 1-D contiguous CPU array — dtype checked at runtime.
using ndarray_1d = nb::ndarray<nb::numpy, nb::ndim<1>, nb::c_contig, nb::device::cpu>;

namespace {

// Helpers: allocate a numpy array and return {ndarray, raw pointer}.
// The capsule owns the heap allocation — the ndarray borrows from it.
// NOTE: Heap allocation is intentional here.  The binding layer is host-only
// code (not flight); §DC-1 (zero-dynamic-allocation) applies to lunalink_core.
template <typename T>
std::pair<nb::ndarray<nb::numpy, T, nb::ndim<1>>, T*> make_numpy_1d(size_t n) {
  auto* buf = new T[n];
  nb::capsule owner(buf, [](void* p) noexcept { delete[] static_cast<T*>(p); });
  return {nb::ndarray<nb::numpy, T, nb::ndim<1>>(buf, {n}, std::move(owner)), buf};
}

template <typename T>
std::pair<nb::ndarray<nb::numpy, T, nb::ndim<2>>, T*> make_numpy_2d(size_t r, size_t c) {
  auto* buf = new T[r * c];
  nb::capsule owner(buf, [](void* p) noexcept { delete[] static_cast<T*>(p); });
  size_t shape[2] = {r, c};
  return {nb::ndarray<nb::numpy, T, nb::ndim<2>>(buf, 2, shape, std::move(owner)), buf};
}

/// Reject any array whose dtype is not exactly T.
/// nanobind's typed ndarray silently converts (e.g. float32 → uint8 via
/// truncation), which is a data-corruption hazard for binary bit arrays.
/// This helper enforces strict dtype matching at the Python boundary.
template <typename T> void require_dtype(const ndarray_1d& arr, const char* name) {
  constexpr auto expected = nb::dtype<T>();
  if (arr.dtype() != expected) {
    throw nb::type_error((std::string(name) + " must have the correct numpy dtype").c_str());
  }
}

/// Convenience: validate dtype and return a typed data pointer.
template <typename T> const T* checked_data(const ndarray_1d& arr, const char* name) {
  require_dtype<T>(arr, name);
  return static_cast<const T*>(arr.data());
}

/// Convenience: validate dtype and return a typed span.
template <typename T> std::span<const T> checked_span(const ndarray_1d& arr, const char* name) {
  return {checked_data<T>(arr, name), arr.shape(0)};
}

nb::ndarray<nb::numpy, uint8_t, nb::ndim<1>> unpack_prn(const PrnCode& code) {
  auto [out, dst] = make_numpy_1d<uint8_t>(code.chip_length);
  for (uint32_t i = 0; i < code.chip_length; ++i) {
    if (unpack_chip(code, static_cast<uint16_t>(i), dst[i]) != PrnStatus::kOk) {
      throw nb::value_error("unpack_chip failed");
    }
  }
  return out;
}

} // namespace

NB_MODULE(_afs, m) {
  m.doc() = "LunaLink AFS C++ extension module.";

  nb::class_<Fid>(m, "Fid")
      .def(nb::init<uint8_t>())
      .def_prop_ro("value", &Fid::value)
      .def("__int__", [](const Fid& f) { return static_cast<uint8_t>(f); })
      .def("__eq__",
           [](const Fid& f, nb::object other) {
             if (nb::isinstance<Fid>(other))
               return static_cast<uint8_t>(f) == static_cast<uint8_t>(nb::cast<Fid>(other));
             if (nb::isinstance<nb::int_>(other))
               return static_cast<int>(static_cast<uint8_t>(f)) == nb::cast<int>(other);
             return false;
           })
      .def("__hash__", [](const Fid& f) { return nb::hash(nb::int_(static_cast<uint8_t>(f))); })
      .def("__repr__",
           [](const Fid& f) { return "Fid(" + std::to_string(static_cast<int>(f.value())) + ")"; })
      .def_static("NODE1", &Fid::kNode1)
      .def_static("NODE2", &Fid::kNode2)
      .def_static("NODE3", &Fid::kNode3)
      .def_static("NODE4", &Fid::kNode4);

  nb::class_<Toi>(m, "Toi")
      .def(nb::init<uint8_t>())
      .def_prop_rw("value", &Toi::value, [](Toi& self, uint8_t v) { self = Toi(v); })
      .def("__int__", [](const Toi& t) { return static_cast<uint8_t>(t); })
      .def("__eq__",
           [](const Toi& t, nb::object other) {
             if (nb::isinstance<Toi>(other))
               return static_cast<uint8_t>(t) == static_cast<uint8_t>(nb::cast<Toi>(other));
             if (nb::isinstance<nb::int_>(other))
               return static_cast<int>(static_cast<uint8_t>(t)) == nb::cast<int>(other);
             return false;
           })
      .def("__hash__", [](const Toi& t) { return nb::hash(nb::int_(static_cast<uint8_t>(t))); })
      .def("__repr__",
           [](const Toi& t) { return "Toi(" + std::to_string(static_cast<int>(t.value())) + ")"; });

  nb::class_<PrnId>(m, "PrnId")
      .def(nb::init<uint8_t>())
      .def_prop_ro("value", [](const PrnId& p) { return p.value(); })
      .def("valid", [](const PrnId& p) { return p.valid(); })
      .def("__int__", [](const PrnId& p) { return p.value(); });

  nb::enum_<BchStatus>(m, "BchStatus")
      .value("OK", BchStatus::kOk)
      .value("OUTPUT_TOO_SMALL", BchStatus::kOutputTooSmall)
      .value("INVALID_FID", BchStatus::kInvalidFid)
      .value("INVALID_TOI", BchStatus::kInvalidToi)
      .value("NULL_OUTPUT", BchStatus::kNullOutput)
      .value("INVALID_INPUT", BchStatus::kInvalidInput)
      .value("AMBIGUOUS_MATCH", BchStatus::kAmbiguousMatch)
      .value("FAULT_DETECTED", BchStatus::kFaultDetected)
      .export_values();

  nb::enum_<FrameStatus>(m, "FrameStatus")
      .value("OK", FrameStatus::kOk)
      .value("OUTPUT_TOO_SMALL", FrameStatus::kOutputTooSmall)
      .value("INVALID_FID", FrameStatus::kInvalidFid)
      .value("INVALID_TOI", FrameStatus::kInvalidToi)
      .value("BCH_FAILED", FrameStatus::kBchFailed)
      .value("FAULT_DETECTED", FrameStatus::kFaultDetected)
      .value("LDPC_FAILED", FrameStatus::kLdpcFailed)
      .value("CRC_FAILED", FrameStatus::kCrcFailed)
      .export_values();

  nb::enum_<MatchedCodeStatus>(m, "MatchedCodeStatus")
      .value("OK", MatchedCodeStatus::kOk)
      .value("INVALID_PRN", MatchedCodeStatus::kInvalidPrn)
      .value("INVALID_EPOCH", MatchedCodeStatus::kInvalidEpoch)
      .value("OUTPUT_TOO_SMALL", MatchedCodeStatus::kOutputTooSmall)
      .value("INVALID_ASSIGNMENT", MatchedCodeStatus::kInvalidAssignment)
      .value("FAULT_DETECTED", MatchedCodeStatus::kFaultDetected)
      .export_values();

  nb::enum_<ModulationStatus>(m, "ModulationStatus")
      .value("OK", ModulationStatus::kOk)
      .value("INVALID_SYMBOL", ModulationStatus::kInvalidSymbol)
      .value("INVALID_CHIP_VALUE", ModulationStatus::kInvalidChipValue)
      .value("OUTPUT_TOO_SMALL", ModulationStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", ModulationStatus::kFaultDetected)
      .export_values();

  nb::enum_<IqMuxStatus>(m, "IqMuxStatus")
      .value("OK", IqMuxStatus::kOk)
      .value("INPUT_TOO_SMALL", IqMuxStatus::kInputTooSmall)
      .value("OUTPUT_TOO_SMALL", IqMuxStatus::kOutputTooSmall)
      .value("INVALID_I_SAMPLE", IqMuxStatus::kInvalidISample)
      .value("INVALID_Q_SAMPLE", IqMuxStatus::kInvalidQSample)
      .value("NULL_INPUT", IqMuxStatus::kNullInput)
      .value("FAULT_DETECTED", IqMuxStatus::kFaultDetected)
      .export_values();

  nb::enum_<LdpcSubframe>(m, "LdpcSubframe")
      .value("SF2", LdpcSubframe::kSF2)
      .value("SF3", LdpcSubframe::kSF3)
      .value("SF4", LdpcSubframe::kSF4)
      .export_values();

  nb::enum_<LdpcStatus>(m, "LdpcStatus")
      .value("OK", LdpcStatus::kOk)
      .value("INVALID_INPUT", LdpcStatus::kInvalidInput)
      .value("OUTPUT_TOO_SMALL", LdpcStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", LdpcStatus::kFaultDetected)
      .value("NOT_CONVERGED", LdpcStatus::kNotConverged)
      .export_values();

  nb::enum_<CrcStatus>(m, "CrcStatus")
      .value("OK", CrcStatus::kOk)
      .value("NULL_INPUT", CrcStatus::kNullInput)
      .value("OUTPUT_TOO_SMALL", CrcStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", CrcStatus::kFaultDetected)
      .export_values();

  nb::enum_<InterleaverStatus>(m, "InterleaverStatus")
      .value("OK", InterleaverStatus::kOk)
      .value("INPUT_TOO_SMALL", InterleaverStatus::kInputTooSmall)
      .value("OUTPUT_TOO_SMALL", InterleaverStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", InterleaverStatus::kFaultDetected)
      .export_values();

  nb::class_<LdpcWorkspace>(m, "LdpcWorkspace").def(nb::init<>());

  nb::enum_<PrnStatus>(m, "PrnStatus")
      .value("OK", PrnStatus::kOk)
      .value("INVALID_PRN", PrnStatus::kInvalidPrn)
      .value("NULL_OUTPUT", PrnStatus::kNullOutput)
      .value("INVALID_CHIP_INDEX", PrnStatus::kInvalidChipIndex)
      .value("FAULT_DETECTED", PrnStatus::kFaultDetected)
      .export_values();

  nb::class_<BchResult>(m, "BchResult")
      .def_ro("status", &BchResult::status)
      .def_ro("fid", &BchResult::fid)
      .def_ro("toi", &BchResult::toi)
      .def_ro("hamming_distance", &BchResult::hamming_distance);

  m.def(
      "bch_encode",
      [](uint8_t fid, uint8_t toi) {
        auto [out, dst] = make_numpy_1d<uint8_t>(52);
        if (bch_encode(static_cast<Fid>(fid), Toi(toi), std::span<uint8_t, 52>(dst, 52)) !=
            BchStatus::kOk)
          throw nb::value_error("Invalid BCH params");
        return out;
      },
      "fid"_a,
      "toi"_a,
      R"(Encode a navigation sub-block header (SB1) using BCH(51,8). [LSIS-AFS-501]

The 9-bit input (2-bit FID + 7-bit TOI) is encoded into a 52-symbol
codeword (1 raw MSB + 51 LFSR symbols). TMR-hardened.

Parameters
----------
fid : int
    Frame ID / node identity (0--3).
toi : int
    Time of Interval (0--99).

Returns
-------
ndarray of uint8
    52 binary symbols (values in {0, 1}).

Raises
------
ValueError
    If *fid* > 3 or *toi* > 99.

Examples
--------
>>> from lunalink.afs import bch_encode, bch_decode
>>> codeword = bch_encode(0, 42)
>>> codeword.shape
(52,)
>>> result = bch_decode(codeword)
>>> int(result.toi)
42
)");

  m.def(
      "bch_decode",
      [](ndarray_1d codeword) {
        if (codeword.shape(0) != 52)
          throw nb::value_error("Must be 52 symbols");
        return bch_decode(
            std::span<const uint8_t, 52>(checked_data<uint8_t>(codeword, "codeword"), 52));
      },
      "codeword"_a,
      R"(Maximum-likelihood BCH decoder (hard decision). [LSIS-AFS-501]

Corrects up to 2 bit errors using a pre-computed exhaustive codebook.

Parameters
----------
codeword : ndarray of uint8
    52 binary symbols (values in {0, 1}).

Returns
-------
BchResult
    Decoded result with ``status``, ``fid``, ``toi``, and
    ``hamming_distance`` (number of corrected bits, 0--2).

Examples
--------
>>> from lunalink.afs import bch_encode, bch_decode
>>> cw = bch_encode(1, 50)
>>> cw[0] ^= 1  # inject 1-bit error
>>> r = bch_decode(cw)
>>> r.status == BchStatus.OK
True
>>> r.hamming_distance
1
)");

  m.def(
      "bch_decode_soft",
      [](ndarray_1d llr) {
        if (llr.shape(0) != 52)
          throw nb::value_error("Must be 52 soft symbols");
        return bch_decode_soft(std::span<const float, 52>(checked_data<float>(llr, "llr"), 52));
      },
      "llr"_a,
      R"(Soft-decision maximum-likelihood BCH decoder. [LSIS-AFS-501]

Selects the codebook entry maximising the inner-product metric
over 52 channel LLRs, giving ~2 dB gain over hard-decision ML.

Parameters
----------
llr : ndarray of float32
    52 channel log-likelihood ratios.  Positive favours bit 0,
    negative favours bit 1 (BPSK AWGN: L = 2y/sigma^2).

Returns
-------
BchResult
    Decoded result.  ``hamming_distance`` is always 0xFF (soft sentinel).
    ``status`` is OK for all finite inputs — caller must judge confidence
    from the LDPC/CRC results, not from BCH status alone.

Notes
-----
- Non-finite (NaN/Inf) LLRs yield ``BchStatus.INVALID_INPUT``.
- Values are internally clamped to +/-100; saturation does not raise.
- No ambiguity detection: ties broken by lowest codebook index.
)");

  m.def("bch_codebook_checksum",
        &bch_codebook_checksum,
        R"(Compute the JIT codebook checksum for CBIT integrity. [NASA-FSW-CBIT]

Returns a 64-bit CRC-like checksum of the internal BCH codebook memory.
Used by power-on self-test to verify codebook ROM integrity.

Returns
-------
int
    64-bit checksum value.
)");

  m.def(
      "crc24_calculate",
      [](ndarray_1d data) {
        auto [out, dst] = make_numpy_1d<uint8_t>(24);
        std::array<uint8_t, 24> crc{};
        auto status = crc24_calculate(checked_span<uint8_t>(data, "data"),
                                      std::span<uint8_t, 24>(crc.data(), 24));
        if (status != CrcStatus::kOk)
          throw nb::value_error("CRC-24 failed");
        for (uint32_t i = 0; i < 24U; ++i)
          dst[i] = crc[i];
        return out;
      },
      "data"_a,
      R"(Calculate CRC-24Q over a bit sequence. [LSIS-FID0-467]

Polynomial: G(X) = 0x864CFB, seed 0, MSB-first.

Parameters
----------
data : ndarray of uint8
    Input bits (one bit per byte, values in {0, 1}).

Returns
-------
ndarray of uint8
    24-bit CRC (one bit per byte).

Examples
--------
>>> import numpy as np
>>> from lunalink.afs.signal import crc24_calculate, crc24_verify
>>> data = np.zeros(100, dtype=np.uint8)
>>> crc = crc24_calculate(data)
>>> crc.shape
(24,)
>>> combined = np.concatenate([data, crc])
>>> crc24_verify(combined)
True
)");

  m.def(
      "crc24_verify",
      [](ndarray_1d data_with_crc) {
        return crc24_verify(checked_span<uint8_t>(data_with_crc, "data_with_crc"));
      },
      "data_with_crc"_a,
      R"(Verify CRC-24Q appended to a bit sequence. [LSIS-FID0-467]

Parameters
----------
data_with_crc : ndarray of uint8
    Input bits with 24-bit CRC appended (one bit per byte).

Returns
-------
bool
    True if CRC matches.
)");

  m.def(
      "block_interleave",
      [](ndarray_1d in) {
        auto [out, dst] = make_numpy_1d<uint8_t>(kInterleaverSize);
        auto status = block_interleave(checked_span<uint8_t>(in, "in"),
                                       std::span<uint8_t>(dst, kInterleaverSize));
        if (status != InterleaverStatus::kOk)
          throw nb::value_error("block_interleave failed: input must be exactly 5880 bits");
        return out;
      },
      "data"_a,
      R"(60x98 block interleaver (write row-wise, read column-wise). [LSIS-FID0-470]

Parameters
----------
data : ndarray of uint8
    5880 binary symbols to interleave.

Returns
-------
ndarray of uint8
    5880 interleaved symbols.
)");

  m.def(
      "block_deinterleave",
      [](ndarray_1d in) {
        auto [out, dst] = make_numpy_1d<uint8_t>(kInterleaverSize);
        auto status = block_deinterleave(checked_span<uint8_t>(in, "in"),
                                         std::span<uint8_t>(dst, kInterleaverSize));
        if (status != InterleaverStatus::kOk)
          throw nb::value_error("block_deinterleave failed: input must be exactly 5880 bits");
        return out;
      },
      "data"_a,
      R"(60x98 block de-interleaver (write column-wise, read row-wise). [LSIS-FID0-470]

Parameters
----------
data : ndarray of uint8
    5880 interleaved binary symbols.

Returns
-------
ndarray of uint8
    5880 de-interleaved symbols.
)");

  m.def(
      "block_deinterleave_soft",
      [](nb::ndarray<const float, nb::ndim<1>, nb::c_contig, nb::device::cpu> in) {
        auto [out, dst] = make_numpy_1d<float>(kInterleaverSize);
        auto status = block_deinterleave_soft(std::span<const float>(in.data(), in.shape(0)),
                                              std::span<float>(dst, kInterleaverSize));
        if (status != InterleaverStatus::kOk)
          throw nb::value_error("block_deinterleave_soft failed");
        return out;
      },
      "data"_a,
      R"(Soft-decision 60x98 block de-interleaver. [LSIS-AFS-471]

Same permutation as ``block_deinterleave`` but preserves float
soft-decision values (LLRs) for FEC decoding.

Parameters
----------
data : ndarray of float32
    5880 soft-decision values (interleaved).

Returns
-------
ndarray of float32
    5880 de-interleaved soft values.
)");

  m.def(
      "ldpc_encode",
      [](LdpcSubframe type, ndarray_1d msg) {
        const size_t n_out = (type == LdpcSubframe::kSF2) ? 2400U : 1740U;
        auto [out, dst] = make_numpy_1d<uint8_t>(n_out);
        LdpcEncodeWorkspace enc_ws{};
        auto status = ldpc_encode(
            type, checked_span<uint8_t>(msg, "msg"), std::span<uint8_t>(dst, n_out), enc_ws);
        if (status != LdpcStatus::kOk)
          throw nb::value_error(
              ("LDPC encode failed (LdpcStatus " + std::to_string(static_cast<int>(status)) + ")")
                  .c_str());
        return out;
      },
      "type"_a,
      "msg"_a,
      R"(Encode a subframe using rate-1/2 5G NR LDPC (Base Graph 2). [LSIS-AFS-320]

Parameters
----------
type : LdpcSubframe
    Subframe type: ``SF2`` (1200 -> 2400) or ``SF3``/``SF4`` (870 -> 1740).
msg : ndarray of uint8
    Message bits including 24-bit CRC (1200 or 870 bits).

Returns
-------
ndarray of uint8
    Encoded symbols (2400 or 1740 bits).

Examples
--------
>>> import numpy as np
>>> from lunalink.afs import ldpc_encode, ldpc_decode, LdpcSubframe
>>> msg = np.zeros(1200, dtype=np.uint8)
>>> encoded = ldpc_encode(LdpcSubframe.SF2, msg)
>>> encoded.shape
(2400,)
)");

  m.def(
      "ldpc_decode",
      [](LdpcSubframe type,
         nb::ndarray<const float, nb::ndim<1>, nb::c_contig, nb::device::cpu> symbols,
         uint8_t iterations,
         nb::object workspace) {
        const size_t k_nom = (type == LdpcSubframe::kSF2) ? 1200U : 870U;
        auto [out, dst] = make_numpy_1d<uint8_t>(k_nom);
        // Stack-allocate workspace to comply with zero-dynamic-allocation mandate (§DC-1).
        LdpcWorkspace local_ws_stack{};
        LdpcWorkspace* ws_ptr;
        if (workspace.is_none()) {
          ws_ptr = &local_ws_stack;
        } else {
          ws_ptr = nb::cast<LdpcWorkspace*>(workspace);
        }

        auto status = ldpc_decode(type,
                                  std::span<const float>(symbols.data(), symbols.shape(0)),
                                  iterations,
                                  std::span<uint8_t>(dst, k_nom),
                                  *ws_ptr);
        if (status != LdpcStatus::kOk && status != LdpcStatus::kNotConverged)
          throw nb::value_error("LDPC decode failed");
        return nb::make_tuple(out, status);
      },
      "type"_a,
      "symbols"_a,
      "iterations"_a,
      "workspace"_a.none() = nb::none(),
      R"(Decode a subframe using Layered Sum-Product BP. [LSIS-AFS-321]

Parameters
----------
type : LdpcSubframe
    Subframe type: ``SF2`` (2400 -> 1200) or ``SF3``/``SF4`` (1740 -> 870).
symbols : ndarray of float32
    Channel LLRs.  Positive favours bit 0, negative favours bit 1
    (BPSK AWGN: L = 2y/sigma^2).
iterations : int
    Maximum belief-propagation iterations (typically 50).
workspace : LdpcWorkspace or None, optional
    Reusable scratchpad.  ``None`` allocates a fresh workspace.
    Pass a persistent workspace for decoder state (re-entrancy).

Returns
-------
tuple[ndarray of uint8, LdpcStatus]
    (decoded_bits, status).  Status is ``OK`` on convergence,
    ``NOT_CONVERGED`` if iteration limit reached (bits may still be usable
    — check CRC).

Examples
--------
>>> import numpy as np
>>> from lunalink.afs import ldpc_encode, ldpc_decode, LdpcSubframe
>>> msg = np.zeros(1200, dtype=np.uint8)
>>> enc = ldpc_encode(LdpcSubframe.SF2, msg)
>>> soft = np.where(enc == 0, 5.0, -5.0).astype(np.float32)
>>> decoded, status = ldpc_decode(LdpcSubframe.SF2, soft, 50)
>>> np.array_equal(decoded, msg)
True
)");

  nb::class_<LdpcEncodeWorkspace>(m, "LdpcEncodeWorkspace").def(nb::init<>());

  m.def(
      "frame_build_partial",
      [](uint8_t fid, uint8_t toi) {
        auto [out, dst] = make_numpy_1d<uint8_t>(6000);
        FrameBuildWorkspace fbw{};
        const auto status = frame_build_partial(
            static_cast<Fid>(fid), Toi(toi), std::span<uint8_t, 6000>(dst, 6000), fbw);
        if (status != FrameStatus::kOk)
          throw nb::value_error(
              ("Frame build failed (FrameStatus " + std::to_string(static_cast<int>(status)) + ")")
                  .c_str());
        return out;
      },
      "fid"_a,
      "toi"_a,
      R"(Build a partial AFS frame: sync + BCH header + zero payload. [LSIS-AFS-601]

Useful for testing sync detection and BCH decoding in isolation.

Parameters
----------
fid : int
    Frame ID (0--3).
toi : int
    Time of Interval (0--99).

Returns
-------
ndarray of uint8
    6000 binary symbols (sync + SB1 + zero-padded payload).
)");

  m.def(
      "frame_build",
      [](uint8_t fid, uint8_t toi, ndarray_1d sb2_data, ndarray_1d sb3_data, ndarray_1d sb4_data) {
        if (sb2_data.shape(0) != kSb2DataBits)
          throw nb::value_error("SB2 data must be 1176 bits");
        if (sb3_data.shape(0) != kSb3DataBits)
          throw nb::value_error("SB3 data must be 846 bits");
        if (sb4_data.shape(0) != kSb4DataBits)
          throw nb::value_error("SB4 data must be 846 bits");
        auto [out, dst] = make_numpy_1d<uint8_t>(kFrameLength);
        LdpcEncodeWorkspace ws{};
        FrameBuildWorkspace fbw{};
        auto status = frame_build(static_cast<Fid>(fid),
                                  Toi(toi),
                                  std::span<const uint8_t, kSb2DataBits>(
                                      checked_data<uint8_t>(sb2_data, "sb2_data"), kSb2DataBits),
                                  std::span<const uint8_t, kSb3DataBits>(
                                      checked_data<uint8_t>(sb3_data, "sb3_data"), kSb3DataBits),
                                  std::span<const uint8_t, kSb4DataBits>(
                                      checked_data<uint8_t>(sb4_data, "sb4_data"), kSb4DataBits),
                                  std::span<uint8_t, kFrameLength>(dst, kFrameLength),
                                  ws,
                                  fbw);
        if (status != FrameStatus::kOk)
          throw nb::value_error(
              ("Frame build failed (FrameStatus " + std::to_string(static_cast<int>(status)) + ")")
                  .c_str());
        return out;
      },
      "fid"_a,
      "toi"_a,
      "sb2_data"_a,
      "sb3_data"_a,
      "sb4_data"_a,
      R"(Build a complete AFS navigation frame. [LSIS-AFS-605]

Pipeline: data -> CRC-24 -> LDPC encode -> concatenate -> block interleave.
Output is 68 sync + 52 BCH + 5880 interleaved = 6000 symbols.

Parameters
----------
fid : int
    Frame ID / node identity (0--3).
toi : int
    Time of Interval (0--99).
sb2_data : ndarray of uint8
    Subframe 2 navigation data (1176 bits).
sb3_data : ndarray of uint8
    Subframe 3 navigation data (846 bits).
sb4_data : ndarray of uint8
    Subframe 4 navigation data (846 bits).

Returns
-------
ndarray of uint8
    6000 binary symbols ready for modulation.

Examples
--------
>>> import numpy as np
>>> from lunalink.afs import frame_build
>>> sb2 = np.zeros(1176, dtype=np.uint8)
>>> sb3 = np.zeros(846, dtype=np.uint8)
>>> sb4 = np.zeros(846, dtype=np.uint8)
>>> frame = frame_build(0, 42, sb2, sb3, sb4)
>>> frame.shape
(6000,)
)");

  m.def(
      "commit_frame",
      [](ndarray_1d frame) { return commit_frame(checked_span<uint8_t>(frame, "frame")); },
      "frame"_a,
      R"(Frame commit gate: verify assembled frame integrity via CRC-24Q. [LSIS-AFS-606]

Defence-in-depth check — no frame shall be transmitted unless this passes.

Parameters
----------
frame : ndarray of uint8
    Assembled frame buffer (must be at least 25 symbols).

Returns
-------
FrameStatus
    ``OK`` if CRC matches; ``FAULT_DETECTED`` otherwise.
)");

  m.def(
      "subframe_build_sb2",
      [](ndarray_1d data) {
        if (data.shape(0) != kSb2DataBits)
          throw nb::value_error("SB2 data must be 1176 bits");
        auto [out, dst] = make_numpy_1d<uint8_t>(kSb2Length);
        LdpcEncodeWorkspace ws{};
        auto status = subframe_build_sb2(std::span<const uint8_t, kSb2DataBits>(
                                             checked_data<uint8_t>(data, "data"), kSb2DataBits),
                                         std::span<uint8_t, kSb2Length>(dst, kSb2Length),
                                         ws);
        if (status != FrameStatus::kOk)
          throw nb::value_error("SB2 build failed");
        return out;
      },
      "data"_a,
      R"(Build Subframe 2: CRC-24 + LDPC SF2 encode. [LSIS-AFS-602]

Parameters
----------
data : ndarray of uint8
    SB2 navigation data (1176 bits).

Returns
-------
ndarray of uint8
    2400 encoded symbols.
)");

  m.def(
      "subframe_build_sb3",
      [](ndarray_1d data) {
        if (data.shape(0) != kSb3DataBits)
          throw nb::value_error("SB3 data must be 846 bits");
        auto [out, dst] = make_numpy_1d<uint8_t>(kSb3Length);
        LdpcEncodeWorkspace ws{};
        auto status = subframe_build_sb3(std::span<const uint8_t, kSb3DataBits>(
                                             checked_data<uint8_t>(data, "data"), kSb3DataBits),
                                         std::span<uint8_t, kSb3Length>(dst, kSb3Length),
                                         ws);
        if (status != FrameStatus::kOk)
          throw nb::value_error("SB3 build failed");
        return out;
      },
      "data"_a,
      R"(Build Subframe 3: CRC-24 + LDPC SF3 encode. [LSIS-AFS-603]

Parameters
----------
data : ndarray of uint8
    SB3 navigation data (846 bits).

Returns
-------
ndarray of uint8
    1740 encoded symbols.
)");

  m.def(
      "subframe_build_sb4",
      [](ndarray_1d data) {
        if (data.shape(0) != kSb4DataBits)
          throw nb::value_error("SB4 data must be 846 bits");
        auto [out, dst] = make_numpy_1d<uint8_t>(kSb4Length);
        LdpcEncodeWorkspace ws{};
        auto status = subframe_build_sb4(std::span<const uint8_t, kSb4DataBits>(
                                             checked_data<uint8_t>(data, "data"), kSb4DataBits),
                                         std::span<uint8_t, kSb4Length>(dst, kSb4Length),
                                         ws);
        if (status != FrameStatus::kOk)
          throw nb::value_error("SB4 build failed");
        return out;
      },
      "data"_a,
      R"(Build Subframe 4: CRC-24 + LDPC SF4 encode. [LSIS-AFS-604]

Parameters
----------
data : ndarray of uint8
    SB4 navigation data (846 bits).

Returns
-------
ndarray of uint8
    1740 encoded symbols.
)");

  m.def(
      "prn_code",
      [](int prn_id) {
        const PrnId id{static_cast<uint8_t>(prn_id)};
        PrnCode p;
        if (gold_prn_packed(id, p) != PrnStatus::kOk)
          throw nb::value_error("Invalid PRN");
        return unpack_prn(p);
      },
      "prn_id"_a,
      R"(Get the Gold-2046 spreading code for AFS-I. [LSIS-AFS-101]

Parameters
----------
prn_id : int
    PRN identifier (1--12 for interim assignments).

Returns
-------
ndarray of uint8
    2046 binary chips (values in {0, 1}).

Examples
--------
>>> from lunalink.afs import prn_code
>>> gold = prn_code(1)
>>> gold.shape
(2046,)
)");

  m.def(
      "modulate_i",
      [](ndarray_1d prn, int data) {
        const size_t n = prn.shape(0);
        auto [out, dst] = make_numpy_1d<int8_t>(n);
        if (modulate_bpsk_i(checked_span<uint8_t>(prn, "prn"),
                            static_cast<int8_t>(data),
                            std::span<int8_t>(dst, n)) != ModulationStatus::kOk)
          throw nb::value_error("Modulation failed");
        return out;
      },
      "prn"_a,
      "data"_a,
      R"(BPSK-modulate a chip sequence for the AFS-I data channel. [LSIS-AFS-201]

Mapping: chip XOR data -> {+1, -1} (0 -> +1, 1 -> -1).

Parameters
----------
prn : ndarray of uint8
    Spreading code chips (values in {0, 1}).
data : int
    Navigation data symbol (+1 or -1).

Returns
-------
ndarray of int8
    Modulated bipolar samples.
)");

  m.def(
      "modulate_q",
      [](ndarray_1d chips) {
        const size_t n = chips.shape(0);
        auto [out, dst] = make_numpy_1d<int8_t>(n);
        if (modulate_bpsk_q(checked_span<uint8_t>(chips, "chips"), std::span<int8_t>(dst, n)) !=
            ModulationStatus::kOk)
          throw nb::value_error("Modulation failed");
        return out;
      },
      "chips"_a,
      R"(BPSK-modulate a chip sequence for the AFS-Q pilot channel. [LSIS-AFS-201]

Mapping: 0 -> +1, 1 -> -1 (no data symbol — pilot is dataless).

Parameters
----------
chips : ndarray of uint8
    Spreading code chips (values in {0, 1}).

Returns
-------
ndarray of int8
    Modulated bipolar samples.
)");

  m.def(
      "multiplex_iq",
      [](ndarray_1d i, ndarray_1d q) {
        if (i.shape(0) != 2046)
          throw nb::value_error("I channel must be 2046 samples");
        if (q.shape(0) != 10230)
          throw nb::value_error("Q channel must be 10230 samples");
        auto [out, dst] = make_numpy_2d<int16_t>(10230, 2);
        if (multiplex_iq(std::span<const int8_t>(checked_data<int8_t>(i, "i"), 2046),
                         std::span<const int8_t>(checked_data<int8_t>(q, "q"), 10230),
                         std::span<int16_t>(dst, 20460)) != IqMuxStatus::kOk)
          throw nb::value_error("Mux failed");
        return out;
      },
      "i"_a,
      "q"_a,
      R"(Multiplex I and Q channels into interleaved I/Q pairs. [LSIS-AFS-301]

The I channel (2046 samples) is upsampled 5x to match the Q channel
rate (10230 samples).  Output: [I0, Q0, I1, Q1, ...].

Parameters
----------
i : ndarray of int8
    In-phase modulated chips (2046 samples).
q : ndarray of int8
    Quadrature modulated chips (10230 samples).

Returns
-------
ndarray of int16, shape (10230, 2)
    Interleaved I/Q pairs at Q-channel rate.
)");

  m.def(
      "weil10230_code",
      [](int id) {
        PrnCode p;
        if (weil10230_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk)
          throw nb::value_error("prn_id must be in [1, 210]");
        return unpack_prn(p);
      },
      "prn_id"_a,
      R"(Get the Weil-10230 primary spreading code for AFS-Q. [LSIS-AFS-102]

Parameters
----------
prn_id : int
    PRN identifier (1--12 for interim assignments).

Returns
-------
ndarray of uint8
    10230 binary chips.
)");

  m.def(
      "weil1500_code",
      [](int id) {
        PrnCode p;
        if (weil1500_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk)
          throw nb::value_error("prn_id must be in [1, 210]");
        return unpack_prn(p);
      },
      "prn_id"_a,
      R"(Get the Weil-1500 tertiary spreading code for AFS-Q. [LSIS-AFS-102]

Parameters
----------
prn_id : int
    PRN identifier (1--12 for interim assignments).

Returns
-------
ndarray of uint8
    1500 binary chips.
)");

  m.def(
      "matched_code_epoch",
      [](int id, int epoch) {
        auto [out, dst] = make_numpy_1d<uint8_t>(10230);
        MatchedCodeAssignment a;
        if (default_matched_assignment_checked(PrnId{static_cast<uint8_t>(id)}, a) !=
            MatchedCodeStatus::kOk)
          throw nb::value_error("prn_id must be in [1, 210]");
        if (matched_code_epoch_checked(a,
                                       static_cast<uint16_t>(epoch),
                                       std::span<uint8_t>(dst, 10230)) != MatchedCodeStatus::kOk)
          throw nb::value_error("Bad epoch");
        return out;
      },
      "prn_id"_a,
      "epoch"_a,
      R"(Generate one epoch of the AFS-Q matched code using default assignments. [LSIS-AFS-401]

Combines primary (Weil-10230), secondary (4-chip Hadamard), and tertiary
(Weil-1500) codes per LSIS Table 11 interim assignments.

Parameters
----------
prn_id : int
    PRN identifier (1--12).
epoch : int
    Epoch index within the 12 s frame (0--5999).

Returns
-------
ndarray of uint8
    10230 binary chips for one 2 ms epoch.

Examples
--------
>>> from lunalink.afs import matched_code_epoch
>>> chips = matched_code_epoch(1, 0)
>>> chips.shape
(10230,)
)");

  m.def(
      "matched_code_epoch_assigned",
      [](int primary_prn,
         int secondary_code_idx,
         int tertiary_prn,
         int tertiary_phase_offset,
         int epoch_idx) {
        MatchedCodeAssignment a;
        a.primary_prn = PrnId{static_cast<uint8_t>(primary_prn)};
        a.secondary_code_idx =
            CheckedRange<uint8_t, 0, 3>{static_cast<uint8_t>(secondary_code_idx)};
        a.tertiary_prn = PrnId{static_cast<uint8_t>(tertiary_prn)};
        a.tertiary_phase_offset =
            CheckedRange<uint16_t, 0, 1499>{static_cast<uint16_t>(tertiary_phase_offset)};
        auto [out, dst] = make_numpy_1d<uint8_t>(10230);
        if (matched_code_epoch_checked(a,
                                       static_cast<uint16_t>(epoch_idx),
                                       std::span<uint8_t>(dst, 10230)) != MatchedCodeStatus::kOk)
          throw nb::value_error("Bad assignment");
        return out;
      },
      "primary_prn"_a,
      "secondary_code_idx"_a,
      "tertiary_prn"_a,
      "tertiary_phase_offset"_a,
      "epoch_idx"_a,
      R"(Generate one epoch of AFS-Q matched code with explicit assignments. [LSIS-AFS-401]

Parameters
----------
primary_prn : int
    Primary Weil-10230 PRN (1--12).
secondary_code_idx : int
    Secondary Hadamard code index (0--3).
tertiary_prn : int
    Tertiary Weil-1500 PRN (1--12).
tertiary_phase_offset : int
    Tertiary code phase offset (0--1499 chips).
epoch_idx : int
    Epoch index within the frame (0--5999).

Returns
-------
ndarray of uint8
    10230 binary chips.
)");

  m.def(
      "default_matched_assignment",
      [](int prn_id) {
        MatchedCodeAssignment a{};
        auto status = default_matched_assignment_checked(PrnId{static_cast<uint8_t>(prn_id)}, a);
        if (status != MatchedCodeStatus::kOk)
          throw nb::value_error("Invalid PRN for assignment");
        return nb::make_tuple(static_cast<uint8_t>(a.primary_prn),
                              static_cast<uint8_t>(a.secondary_code_idx),
                              static_cast<uint8_t>(a.tertiary_prn),
                              static_cast<uint16_t>(a.tertiary_phase_offset));
      },
      "prn_id"_a,
      R"(Get the default interim code assignment for a PRN. [LSIS-AFS-401]

Per LSIS Table 11.  Only PRN 1--12 have interim assignments.

Parameters
----------
prn_id : int
    PRN identifier (1--12).

Returns
-------
tuple[int, int, int, int]
    (primary_prn, secondary_code_idx, tertiary_prn, tertiary_phase_offset).
)");

  // ─── Signal generation (TX pipeline) ──────────────────────────────────────

  nb::enum_<SignalGenStatus>(m, "SignalGenStatus")
      .value("OK", SignalGenStatus::kOk)
      .value("INVALID_PRN", SignalGenStatus::kInvalidPrn)
      .value("INVALID_INPUT", SignalGenStatus::kInvalidInput)
      .value("MODULATION_FAILED", SignalGenStatus::kModulationFailed)
      .value("MUX_FAILED", SignalGenStatus::kMuxFailed)
      .value("FAULT_DETECTED", SignalGenStatus::kFaultDetected)
      .export_values();

  m.def(
      "generate_iq_epoch_native",
      [](int prn_id, int epoch_idx, int data_symbol) {
        // Unpack Gold code for this single-epoch call.
        PrnCode gold{};
        if (gold_prn_packed(PrnId{static_cast<uint8_t>(prn_id)}, gold) != PrnStatus::kOk)
          throw nb::value_error("Invalid PRN");
        std::array<uint8_t, kGoldChipLength> gold_chips{};
        for (uint32_t i = 0; i < kGoldChipLength; ++i) {
          if (unpack_chip(gold, static_cast<uint16_t>(i), gold_chips[i]) != PrnStatus::kOk)
            throw nb::value_error("unpack_chip failed");
        }
        auto [out, dst] = make_numpy_2d<int16_t>(kIqSamplesPerEpoch, 2);
        auto status = generate_iq_epoch(std::span<const uint8_t>(gold_chips),
                                        PrnId{static_cast<uint8_t>(prn_id)},
                                        static_cast<uint16_t>(epoch_idx),
                                        static_cast<int8_t>(data_symbol),
                                        std::span<int16_t>(dst, kIqPairsPerEpoch));
        if (status != SignalGenStatus::kOk)
          throw nb::value_error("generate_iq_epoch failed");
        return out;
      },
      "prn_id"_a,
      "epoch_idx"_a,
      "data_symbol"_a = 1,
      "Generate one 2 ms epoch of interleaved I/Q at native rate (10230x2 int16).");

  m.def(
      "generate_iq_epoch_float",
      [](int prn_id, int epoch_idx, int data_symbol, double sample_rate) {
        const auto n_native = static_cast<size_t>(kIqSamplesPerEpoch);
        const bool is_native = (std::fabs(sample_rate - kNativeSampleRateHz) < 1.0) ||
                               (sample_rate <= 0.0);
        const size_t n_out =
            is_native ? n_native
                      : static_cast<size_t>(std::lround(static_cast<double>(n_native) *
                                                        sample_rate / kNativeSampleRateHz));
        auto [out, dst] = make_numpy_2d<float>(n_out, 2);
        size_t written = 0;
        auto status = generate_iq_epoch_float(PrnId{static_cast<uint8_t>(prn_id)},
                                              static_cast<uint16_t>(epoch_idx),
                                              static_cast<int8_t>(data_symbol),
                                              sample_rate,
                                              std::span<float>(dst, n_out * 2),
                                              written);
        if (status != SignalGenStatus::kOk)
          throw nb::value_error("generate_iq_epoch_float failed");
        return out;
      },
      "prn_id"_a,
      "epoch_idx"_a,
      "data_symbol"_a = 1,
      "sample_rate"_a = kNativeSampleRateHz,
      "Generate one 2 ms epoch as float32 with optional resampling.");

  m.def(
      "generate_iq_frame_native",
      [](int prn_id, ndarray_1d frame_symbols) {
        if (frame_symbols.shape(0) != kEpochsPerFrame)
          throw nb::value_error("frame_symbols must be 6000 elements");
        auto [out, dst] =
            make_numpy_2d<int16_t>(static_cast<size_t>(kEpochsPerFrame) * kIqSamplesPerEpoch, 2);
        auto status = generate_iq_frame(PrnId{static_cast<uint8_t>(prn_id)},
                                        checked_span<uint8_t>(frame_symbols, "frame_symbols"),
                                        std::span<int16_t>(dst, kIqPairsPerFrame));
        if (status != SignalGenStatus::kOk)
          throw nb::value_error("generate_iq_frame failed");
        return out;
      },
      "prn_id"_a,
      "frame_symbols"_a,
      "Generate a full 12 s frame of interleaved I/Q at native rate (int16).");

  m.def(
      "generate_iq_frame_float",
      [](int prn_id, ndarray_1d frame_symbols, double sample_rate) {
        if (frame_symbols.shape(0) != kEpochsPerFrame)
          throw nb::value_error("frame_symbols must be 6000 elements");
        const auto n_native = static_cast<size_t>(kIqSamplesPerEpoch);
        const bool is_native = (std::fabs(sample_rate - kNativeSampleRateHz) < 1.0) ||
                               (sample_rate <= 0.0);
        const size_t n_out_per_epoch =
            is_native ? n_native
                      : static_cast<size_t>(std::lround(static_cast<double>(n_native) *
                                                        sample_rate / kNativeSampleRateHz));
        const size_t total_rows = n_out_per_epoch * kEpochsPerFrame;
        const size_t total_floats = total_rows * 2;
        auto [out, dst] = make_numpy_2d<float>(total_rows, 2);
        size_t written = 0;
        auto status = generate_iq_frame_float(PrnId{static_cast<uint8_t>(prn_id)},
                                              checked_span<uint8_t>(frame_symbols, "frame_symbols"),
                                              sample_rate,
                                              std::span<float>(dst, total_floats),
                                              written);
        if (status != SignalGenStatus::kOk)
          throw nb::value_error("generate_iq_frame_float failed");
        return out;
      },
      "prn_id"_a,
      "frame_symbols"_a,
      "sample_rate"_a,
      "Generate a full 12 s frame as float32 with optional resampling.");

  m.attr("IQ_PAIRS_PER_EPOCH") = kIqPairsPerEpoch;
  m.attr("IQ_PAIRS_PER_FRAME") = kIqPairsPerFrame;

  m.attr("EPOCHS_PER_FRAME") = kEpochsPerFrame;
  m.attr("SECONDARY_CODE_LENGTH") = kSecondaryCodeLength;
  m.attr("SECONDARY_CODE_COUNT") = kSecondaryCodeCount;
  m.attr("INTERIM_ASSIGNMENT_MAX_PRN") = kInterimAssignmentMaxPrn;
  m.attr("TERTIARY_CODE_LENGTH") = kWeil1500ChipLength;
  m.attr("IQ_UPSAMPLE_FACTOR") = kIqUpsampleFactor;
  m.attr("IQ_SAMPLES_PER_EPOCH") = kIqSamplesPerEpoch;
  m.attr("FRAME_LENGTH") = kFrameLength;
  m.attr("SYNC_LENGTH") = kSyncLength;
  m.attr("SB1_LENGTH") = kSb1Length;
  m.attr("PAYLOAD_LENGTH") = kPayloadLength;
  m.attr("BCH_CODEWORD_LENGTH") = 52;
  m.attr("SYMBOL_RATE") = kSymbolRate;
  m.attr("FRAME_DURATION_S") = kFrameDurationS;
  m.attr("INTERLEAVER_SIZE") = kInterleaverSize;
  m.attr("INTERLEAVER_ROWS") = kInterleaverRows;
  m.attr("INTERLEAVER_COLS") = kInterleaverCols;
  m.attr("SB2_DATA_BITS") = kSb2DataBits;
  m.attr("SB2_MSG_BITS") = kSb2MsgBits;
  m.attr("SB2_LENGTH") = kSb2Length;
  m.attr("SB3_DATA_BITS") = kSb3DataBits;
  m.attr("SB3_MSG_BITS") = kSb3MsgBits;
  m.attr("SB3_LENGTH") = kSb3Length;
  m.attr("SB4_DATA_BITS") = kSb4DataBits;
  m.attr("SB4_MSG_BITS") = kSb4MsgBits;
  m.attr("SB4_LENGTH") = kSb4Length;
  m.attr("DEFAULT_SYNC_THRESHOLD") = kDefaultSyncThreshold;

  nb::enum_<SyncDetectorStatus>(m, "SyncDetectorStatus")
      .value("OK", SyncDetectorStatus::kOk)
      .value("INVALID_INPUT", SyncDetectorStatus::kInvalidInput)
      .value("NOT_DETECTED", SyncDetectorStatus::kNotDetected)
      .value("FAULT_DETECTED", SyncDetectorStatus::kFaultDetected)
      .export_values();

  nb::class_<SyncResult>(m, "SyncResult")
      .def_ro("frame_offset", &SyncResult::frame_offset)
      .def_ro("peak_metric", &SyncResult::peak_metric)
      .def_ro("second_peak", &SyncResult::second_peak)
      .def_ro("detected", &SyncResult::detected);

  m.def(
      "sync_detect",
      [](nb::ndarray<const float, nb::ndim<1>, nb::c_contig, nb::device::cpu> rx_symbols,
         float threshold) {
        NullTelemetryHal hal;
        SyncDetector det(hal);
        if (threshold > 0.0F) {
          if (!det.set_threshold(threshold)) {
            throw nb::value_error("Invalid threshold");
          }
        }
        SyncResult result{};
        auto status = det.detect(std::span<const float>(rx_symbols.data(), rx_symbols.shape(0)),
                                 result);
        return nb::make_tuple(result, status);
      },
      "rx_symbols"_a,
      "threshold"_a = 0.0F,
      R"(Detect the 68-symbol sync pattern in a stream of soft symbols.

Parameters
----------
rx_symbols : array of float
    Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1).
threshold : float, optional
    Detection threshold (default: 50.0). Set to 0 to use default.

Returns
-------
tuple[SyncResult, SyncDetectorStatus]
    Detection result and status code.
)");

  // ── Frame Decode (RX Pipeline) ──────────────────────────────────────────

  nb::enum_<FrameDecodeStatus>(m, "FrameDecodeStatus")
      .value("OK", FrameDecodeStatus::kOk)
      .value("SYNC_NOT_FOUND", FrameDecodeStatus::kSyncNotFound)
      .value("BCH_FAILED", FrameDecodeStatus::kBchFailed)
      .value("LDPC_FAILED", FrameDecodeStatus::kLdpcFailed)
      .value("CRC_FAILED", FrameDecodeStatus::kCrcFailed)
      .value("INVALID_INPUT", FrameDecodeStatus::kInvalidInput)
      .value("FAULT_DETECTED", FrameDecodeStatus::kFaultDetected)
      .export_values();

  nb::class_<SubframeDecodeResult>(m, "SubframeDecodeResult")
      .def_ro("ldpc_converged", &SubframeDecodeResult::ldpc_converged)
      .def_ro("crc_ok", &SubframeDecodeResult::crc_ok);

  nb::class_<FrameDecodeResult>(m, "FrameDecodeResult")
      .def_ro("status", &FrameDecodeResult::status)
      .def_ro("sync", &FrameDecodeResult::sync)
      .def_ro("bch", &FrameDecodeResult::bch)
      .def_ro("sb2", &FrameDecodeResult::sb2)
      .def_ro("sb3", &FrameDecodeResult::sb3)
      .def_ro("sb4", &FrameDecodeResult::sb4);

  m.def(
      "frame_decode",
      [](nb::ndarray<const float, nb::ndim<1>, nb::c_contig, nb::device::cpu> rx_symbols,
         uint8_t ldpc_iterations,
         float sync_threshold) {
        NullTelemetryHal hal;
        FrameDecodeResult result{};
        auto workspace = std::make_unique<FrameDecodeWorkspace>();

        const auto status =
            frame_decode(std::span<const float>(rx_symbols.data(), rx_symbols.shape(0)),
                         result,
                         *workspace,
                         hal,
                         ldpc_iterations,
                         sync_threshold);

        // Copy decoded data into numpy arrays — only when overall status is kOk
        // AND the individual subframe CRC passed.  On any fault, workspace data
        // buffers have been scrubbed by frame_decode() and must not be copied.
        auto [sb2, sb2_dst] = make_numpy_1d<uint8_t>(kSb2DataBits);
        auto [sb3, sb3_dst] = make_numpy_1d<uint8_t>(kSb3DataBits);
        auto [sb4, sb4_dst] = make_numpy_1d<uint8_t>(kSb4DataBits);

        if (status == FrameDecodeStatus::kOk && result.sb2.crc_ok) {
          std::memcpy(sb2_dst, workspace->sb2_data.data(), kSb2DataBits);
        }
        if (status == FrameDecodeStatus::kOk && result.sb3.crc_ok) {
          std::memcpy(sb3_dst, workspace->sb3_data.data(), kSb3DataBits);
        }
        if (status == FrameDecodeStatus::kOk && result.sb4.crc_ok) {
          std::memcpy(sb4_dst, workspace->sb4_data.data(), kSb4DataBits);
        }

        return nb::make_tuple(result, sb2, sb3, sb4, status);
      },
      "rx_symbols"_a,
      "ldpc_iterations"_a = kFrameDecodeLdpcIterations,
      "sync_threshold"_a = 0.0F,
      R"(Decode a complete AFS navigation frame from soft-decision symbols.

Parameters
----------
rx_symbols : array of float
    Bipolar soft-decision symbols (+1.0 = bit 0, -1.0 = bit 1).
    Must contain at least kFrameLength (6000) symbols after the sync offset.
ldpc_iterations : int, optional
    LDPC BP iteration count (default: 50).
sync_threshold : float, optional
    Sync detection threshold (default: 50.0). Set to 0 to use default.

Returns
-------
tuple[FrameDecodeResult, ndarray, ndarray, ndarray, FrameDecodeStatus]
    (result, sb2_data, sb3_data, sb4_data, status)
)");

  // ─── AFS-I signal constants (derived from prn.hpp spec values) ──────────

  m.attr("AFS_I_CODE_LENGTH") = kGoldChipLength;  // 2046
  m.attr("AFS_I_CHIP_RATE_HZ") = kAfsIChipRateHz; // 1.023e6

  // PRN code constants
  m.attr("GOLD_CHIP_LENGTH") = kGoldChipLength;
  m.attr("WEIL_10230_CHIP_LENGTH") = kWeil10230ChipLength;
  m.attr("WEIL_1500_CHIP_LENGTH") = kWeil1500ChipLength;
  m.attr("PRN_COUNT") = kPrnCount;
  m.attr("CHIP_RATE_HZ") = kAfsIChipRateHz; // 1.023e6 (from prn.hpp)

  // Signal parameters (signal_parameters.csv)
  m.attr("CENTER_FREQUENCY_HZ") = kCenterFrequencyHz;
  m.attr("AFS_I_CHIP_RATE_HZ") = kAfsIChipRateHz;
  m.attr("AFS_Q_CHIP_RATE_HZ") = kAfsQChipRateHz;

  // BCH metadata (encoding_parameters.csv)
  m.attr("BCH_GENERATOR_POLY") = kBchGeneratorPoly;
  m.attr("BCH_INPUT_BITS") = kBchInputBits;

  // LDPC metadata (encoding_parameters.csv)
  m.attr("LDPC_BG2_ROWS") = kLdpcBg2Rows;
  m.attr("LDPC_BG2_COLS") = kLdpcBg2Cols;
  m.attr("LDPC_BG2_KB") = kLdpcBg2Kb;
  m.attr("LDPC_SF2_ZC") = kLdpcSf2Zc;
  m.attr("LDPC_SF2_ILS") = kLdpcSf2Ils;
  m.attr("LDPC_SF34_ZC") = kLdpcSf34Zc;
  m.attr("LDPC_SF34_ILS") = kLdpcSf34Ils;

  // Time system (time_system_parameters.csv)
  m.attr("TOI_COUNT") = kToiCount;

  // SB2 field layout (sb2_bit_layout.csv)
  m.attr("SB2_HEALTH_OFFSET") = kSb2HealthOffset;
  m.attr("SB2_HEALTH_BITS") = kSb2HealthBits;
  m.attr("SB2_CED_OFFSET") = kSb2CedOffset;
  m.attr("SB2_CED_BITS") = kSb2CedBits;
  m.attr("SB2_TIMECONV_OFFSET") = kSb2TimeConvOffset;
  m.attr("SB2_TIMECONV_BITS") = kSb2TimeConvBits;
  m.attr("SB2_SPARE_OFFSET") = kSb2SpareOffset;
  m.attr("SB2_SPARE_BITS") = kSb2SpareBits;

  // File format magic numbers (file_format_headers.csv)
  m.attr("MAGIC_IQSG") = kMagicIqsg;
  m.attr("MAGIC_IQS2") = kMagicIqs2;
  m.attr("MAGIC_LSIS") = kMagicLsis;
  m.attr("MAGIC_FRAM") = kMagicFram;
  m.attr("IQ_HEADER_BYTES") = kIqHeaderBytes;
  m.attr("LSIS_HEADER_BYTES") = kLsisHeaderBytes;
  m.attr("FRAM_HEADER_BYTES") = kFramHeaderBytes;

  // Legendre cache bindings
  nb::enum_<LegendreStatus>(m, "LegendreStatus")
      .value("OK", LegendreStatus::kOk)
      .value("OUTPUT_TOO_SMALL", LegendreStatus::kOutputTooSmall)
      .value("INVALID_PRIME", LegendreStatus::kInvalidPrime)
      .value("FAULT_DETECTED", LegendreStatus::kFaultDetected)
      .export_values();

  m.def(
      "legendre_cache_init",
      []() { return legendre_cache_init(); },
      R"(Initialise the Legendre sequence cache (POST step).

Generates both sequences (p=10223, p=1499) and verifies against
golden CRCs.  Must be called before any Weil code generation.

Returns
-------
LegendreStatus
    ``OK`` on success; ``FAULT_DETECTED`` if CRC mismatch.
)");

  m.def("legendre_10223_checksum",
        &legendre_10223_checksum,
        R"(CRC-32 checksum of the cached Legendre-10223 sequence.

Returns
-------
int
    CRC-32 (IEEE 802.3) checksum.
)");

  m.def("legendre_1499_checksum",
        &legendre_1499_checksum,
        R"(CRC-32 checksum of the cached Legendre-1499 sequence.

Returns
-------
int
    CRC-32 (IEEE 802.3) checksum.
)");

  // ─── Message Parser bindings (Gateway 6) ──────────────────────────────────

  nb::enum_<MessageType>(m, "MessageType")
      .value("G1", MessageType::kG1)
      .value("G2", MessageType::kG2)
      .value("G3", MessageType::kG3)
      .value("G4", MessageType::kG4)
      .value("G5", MessageType::kG5)
      .value("G8", MessageType::kG8)
      .value("G10", MessageType::kG10)
      .value("G11", MessageType::kG11)
      .value("G14", MessageType::kG14)
      .value("G15", MessageType::kG15)
      .value("G17", MessageType::kG17)
      .value("S19", MessageType::kS19)
      .value("S20", MessageType::kS20)
      .value("G22", MessageType::kG22)
      .value("G23", MessageType::kG23)
      .value("G24", MessageType::kG24)
      .value("G25", MessageType::kG25)
      .value("G27", MessageType::kG27)
      .value("G28", MessageType::kG28)
      .value("G29", MessageType::kG29)
      .value("G30", MessageType::kG30)
      .value("G31", MessageType::kG31)
      .value("G32", MessageType::kG32)
      .value("UNKNOWN", MessageType::kUnknown)
      .export_values();

  nb::enum_<MessageParseStatus>(m, "MessageParseStatus")
      .value("OK", MessageParseStatus::kOk)
      .value("INVALID_INPUT", MessageParseStatus::kInvalidInput)
      .value("FAULT_DETECTED", MessageParseStatus::kFaultDetected)
      .export_values();

  nb::enum_<TotStatus>(m, "TotStatus")
      .value("OK", TotStatus::kOk)
      .value("INVALID_TOI", TotStatus::kInvalidToi)
      .value("INVALID_ITOW", TotStatus::kInvalidItow)
      .value("INVALID_WN", TotStatus::kInvalidWn)
      .value("INVALID_DELTA", TotStatus::kInvalidDelta)
      .value("FAULT_DETECTED", TotStatus::kFaultDetected)
      .export_values();

  nb::class_<Sb2ParseResult>(m, "Sb2ParseResult")
      .def_ro("status", &Sb2ParseResult::status)
      .def_ro("wn", &Sb2ParseResult::wn)
      .def_ro("itow", &Sb2ParseResult::itow)
      .def_ro("data_offset", &Sb2ParseResult::data_offset)
      .def_ro("payload_size", &Sb2ParseResult::payload_size);

  nb::class_<Sb3ParseResult>(m, "Sb3ParseResult")
      .def_ro("status", &Sb3ParseResult::status)
      .def_ro("type_raw", &Sb3ParseResult::type_raw)
      .def_ro("type", &Sb3ParseResult::type)
      .def_ro("data_offset", &Sb3ParseResult::data_offset)
      .def_ro("payload_size", &Sb3ParseResult::payload_size);

  nb::class_<Sb4ParseResult>(m, "Sb4ParseResult")
      .def_ro("status", &Sb4ParseResult::status)
      .def_ro("type_raw", &Sb4ParseResult::type_raw)
      .def_ro("type", &Sb4ParseResult::type)
      .def_ro("data_offset", &Sb4ParseResult::data_offset)
      .def_ro("payload_size", &Sb4ParseResult::payload_size);

  nb::class_<TotResult>(m, "TotResult")
      .def_ro("status", &TotResult::status)
      .def_ro("t_f", &TotResult::t_f);

  // parse_sb2/sb3/sb4: accept nb::ndarray<> (untyped) and validate dtype manually
  // to prevent silent float-to-int truncation bypassing the C++ bit-value guard.
  // nanobind's typed ndarray does implicit dtype conversion, so we must check.
  m.def(
      "parse_sb2",
      [](ndarray_1d sb2_data) {
        Sb2ParseResult result{};
        auto status = parse_sb2(checked_span<uint8_t>(sb2_data, "sb2_data"), result);
        if (status == MessageParseStatus::kFaultDetected)
          throw nb::value_error("parse_sb2: fault detected");
        if (status != MessageParseStatus::kOk)
          throw nb::value_error("parse_sb2 failed");
        return result;
      },
      "sb2_data"_a,
      R"(Parse Subframe 2: extract WN and ITOW fields. [LSIS-AFS-801]

Parameters
----------
sb2_data : array of uint8
    Decoded SB2 navigation data (1176 bits, one bit per byte).
    Must be dtype uint8 — non-uint8 arrays are rejected (no silent coercion).

Returns
-------
Sb2ParseResult
    Parsed result with wn, itow, and data_offset fields.
)");

  m.def(
      "parse_sb3",
      [](ndarray_1d sb3_data) {
        Sb3ParseResult result{};
        auto status = parse_sb3(checked_span<uint8_t>(sb3_data, "sb3_data"), result);
        if (status == MessageParseStatus::kFaultDetected)
          throw nb::value_error("parse_sb3: fault detected");
        if (status != MessageParseStatus::kOk)
          throw nb::value_error("parse_sb3 failed");
        return result;
      },
      "sb3_data"_a,
      R"(Parse Subframe 3: extract type field. [LSIS-AFS-802]

Parameters
----------
sb3_data : array of uint8
    Decoded SB3 navigation data (846 bits, one bit per byte).
    Must be dtype uint8 — non-uint8 arrays are rejected (no silent coercion).

Returns
-------
Sb3ParseResult
    Parsed result with type_raw, type, and data_offset fields.
)");

  m.def(
      "parse_sb4",
      [](ndarray_1d sb4_data) {
        Sb4ParseResult result{};
        auto status = parse_sb4(checked_span<uint8_t>(sb4_data, "sb4_data"), result);
        if (status == MessageParseStatus::kFaultDetected)
          throw nb::value_error("parse_sb4: fault detected");
        if (status != MessageParseStatus::kOk)
          throw nb::value_error("parse_sb4 failed");
        return result;
      },
      "sb4_data"_a,
      R"(Parse Subframe 4: extract type field. [LSIS-AFS-803]

Parameters
----------
sb4_data : array of uint8
    Decoded SB4 navigation data (846 bits, one bit per byte).
    Must be dtype uint8 — non-uint8 arrays are rejected (no silent coercion).

Returns
-------
Sb4ParseResult
    Parsed result with type_raw, type, and data_offset fields.
)");

  m.def(
      "compute_tot",
      [](uint16_t wn, uint16_t itow, uint8_t toi, double delta_t_lrt) {
        return compute_tot(wn, itow, toi, delta_t_lrt);
      },
      "wn"_a,
      "itow"_a,
      "toi"_a,
      "delta_t_lrt"_a = 0.0,
      R"(Compute Time of Transmission per LSIS-720. [LSIS-AFS-810]

Formula: t_F = WN * 604800 + ITOW * 1200 + TOI * 12 + delta_t_lrt

Parameters
----------
wn : int
    Week Number (0..8191, 13 bits).
itow : int
    Interval Time of Week (0..503, 9 bits).
toi : int
    Time of Interval (0..99, 7 bits).
delta_t_lrt : float, optional
    Node Time offset w.r.t. LRT in seconds (default: 0.0).

Returns
-------
TotResult
    Result with status and t_f (time since LRT epoch in seconds).
)");

  // Export Gateway 6 constants
  m.attr("SUBFRAME_TYPE_BITS") = kSubframeTypeBits;
  m.attr("WN_BITS") = kWnBits;
  m.attr("ITOW_BITS") = kItowBits;
  m.attr("TOT_HEADER_BITS") = kTotHeaderBits;
  m.attr("SB2_PAYLOAD_BITS") = kSb2PayloadBits;
  m.attr("SB3_PAYLOAD_BITS") = kSb3PayloadBits;
  m.attr("SB4_PAYLOAD_BITS") = kSb4PayloadBits;
  m.attr("SEC_WEEK") = kSecWeek;
  m.attr("BLOCK_INTERVAL_DURATION") = kBlockIntervalDuration;
  m.attr("FRAME_DURATION_SEC") = kFrameDurationSec;
  m.attr("WN_MAX") = kWnMax;
  m.attr("ITOW_MAX") = kItowMax;
  m.attr("TOT_TOI_MAX") = kTotToiMax;

  // ─── SISE (Signal-In-Space Error) bindings (Gateway 7) ──────────────────

  using lunalink::core::SisePercentileResult;
  using lunalink::core::SiseResult;
  using lunalink::core::SiseStatus;
  using lunalink::core::SiseVec;

  nb::enum_<SiseStatus>(m, "SiseStatus")
      .value("OK", SiseStatus::kOk)
      .value("INVALID_INPUT", SiseStatus::kInvalidInput)
      .value("EMPTY_DATA", SiseStatus::kEmptyData)
      .value("FAULT_DETECTED", SiseStatus::kFaultDetected)
      .export_values();

  nb::class_<SiseVec>(m, "SiseVec")
      .def(nb::init<double, double, double, double>(),
           "x"_a = 0.0,
           "y"_a = 0.0,
           "z"_a = 0.0,
           "ct"_a = 0.0)
      .def_rw("x", &SiseVec::x)
      .def_rw("y", &SiseVec::y)
      .def_rw("z", &SiseVec::z)
      .def_rw("ct", &SiseVec::ct);

  nb::class_<SiseResult>(m, "SiseResult")
      .def_ro("status", &SiseResult::status)
      .def_ro("position_error_m", &SiseResult::position_error_m)
      .def_ro("velocity_error_mps", &SiseResult::velocity_error_mps);

  nb::class_<SisePercentileResult>(m, "SisePercentileResult")
      .def_ro("status", &SisePercentileResult::status)
      .def_ro("position_percentile_m", &SisePercentileResult::position_percentile_m)
      .def_ro("velocity_percentile_mps", &SisePercentileResult::velocity_percentile_mps)
      .def_ro("position_pass", &SisePercentileResult::position_pass)
      .def_ro("velocity_pass", &SisePercentileResult::velocity_pass);

  m.def(
      "compute_sise",
      [](SiseVec pos_true, SiseVec pos_broadcast, SiseVec vel_true, SiseVec vel_broadcast) {
        return lunalink::core::compute_sise(pos_true, pos_broadcast, vel_true, vel_broadcast);
      },
      "pos_true"_a,
      "pos_broadcast"_a,
      "vel_true"_a,
      "vel_broadcast"_a,
      R"(Compute single-epoch SISE per LSIS V1.0 §2.2.2. [LSIS-SISE]

4-D RSS of (true - broadcast) position+clock and velocity+clock-drift.

Parameters
----------
pos_true : SiseVec
    True position (x,y,z in metres) + clock bias (ct in metres).
pos_broadcast : SiseVec
    Broadcast position + clock bias.
vel_true : SiseVec
    True velocity (x,y,z in m/s) + clock drift (ct in m/s).
vel_broadcast : SiseVec
    Broadcast velocity + clock drift.

Returns
-------
SiseResult
    Result with status, position_error_m, and velocity_error_mps.
)");

  m.def(
      "compute_sise_percentile",
      [](nb::ndarray<const double, nb::ndim<1>, nb::c_contig, nb::device::cpu> pos_errors,
         nb::ndarray<const double, nb::ndim<1>, nb::c_contig, nb::device::cpu> vel_errors,
         double percentile) {
        const auto n = pos_errors.shape(0);
        if (vel_errors.shape(0) != n) {
          throw nb::value_error("pos_errors and vel_errors must have same length");
        }
        // Create mutable scratch copies — compute_sise_percentile reorders in-place.
        std::vector<double> pos_copy(pos_errors.data(), pos_errors.data() + n);
        std::vector<double> vel_copy(vel_errors.data(), vel_errors.data() + n);
        return lunalink::core::compute_sise_percentile(
            std::span<double>(pos_copy), std::span<double>(vel_copy), percentile);
      },
      "pos_errors_m"_a,
      "vel_errors_mps"_a,
      "percentile"_a = 0.95,
      R"(Compute percentile SISE over N epochs. [LSIS-SISE]

Parameters
----------
pos_errors_m : array of float64
    Position errors in metres for each epoch.
vel_errors_mps : array of float64
    Velocity errors in m/s for each epoch.
percentile : float, optional
    Percentile to compute (0.0..1.0, default 0.95).

Returns
-------
SisePercentileResult
    Result with status, percentile values, and pass/fail against spec limits
    (40 m position, 0.01 m/s velocity).
)");

  m.attr("SISE_POSITION_LIMIT_M") = lunalink::core::kSisePositionLimitM;
  m.attr("SISE_VELOCITY_LIMIT_MPS") = lunalink::core::kSiseVelocityLimitMps;
  m.attr("SPEED_OF_LIGHT_MPS") = lunalink::core::kSpeedOfLightMps;
}
