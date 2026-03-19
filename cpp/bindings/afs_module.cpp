#include "lunalink/encoding/bch.hpp"
#include "lunalink/signal/correlator.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/legendre.hpp"
#include "lunalink/signal/sync_detector.hpp"
#include <cstdint>
#include <span>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace lunalink::signal;

namespace {

py::array_t<uint8_t> unpack_prn(const PrnCode& code) {
  auto out = py::array_t<uint8_t>(code.chip_length);
  auto *dst = out.mutable_data();
  for (uint32_t i = 0; i < code.chip_length; ++i) {
    if (unpack_chip(code, static_cast<uint16_t>(i), dst[i]) != PrnStatus::kOk) {
      throw py::value_error("unpack_chip failed");
    }
  }
  return out;
}

} // namespace

PYBIND11_MODULE(_afs, m) {
  m.doc() = "LunaLink AFS C++ extension module.";

  py::class_<Fid>(m, "Fid")
      .def(py::init<uint8_t>())
      .def_property_readonly("value", &Fid::value)
      .def("__int__", [](const Fid& f) { return static_cast<uint8_t>(f); })
      .def_static("NODE1", &Fid::kNode1)
      .def_static("NODE2", &Fid::kNode2)
      .def_static("NODE3", &Fid::kNode3)
      .def_static("NODE4", &Fid::kNode4);

  py::class_<Toi>(m, "Toi")
      .def(py::init<uint8_t>())
      .def_property("value", &Toi::value, [](Toi& self, uint8_t v) { self = Toi(v); })
      .def("__int__", [](const Toi& t) { return static_cast<uint8_t>(t); });

  py::class_<PrnId>(m, "PrnId")
      .def(py::init<uint8_t>())
      .def_property_readonly("value", [](const PrnId& p) { return p.value(); })
      .def("valid", [](const PrnId& p) { return p.valid(); })
      .def("__int__", [](const PrnId& p) { return p.value(); });

  py::enum_<BchStatus>(m, "BchStatus")
      .value("OK", BchStatus::kOk)
      .value("OUTPUT_TOO_SMALL", BchStatus::kOutputTooSmall)
      .value("INVALID_FID", BchStatus::kInvalidFid)
      .value("INVALID_TOI", BchStatus::kInvalidToi)
      .value("NULL_OUTPUT", BchStatus::kNullOutput)
      .value("INVALID_INPUT", BchStatus::kInvalidInput)
      .value("AMBIGUOUS_MATCH", BchStatus::kAmbiguousMatch)
      .value("FAULT_DETECTED", BchStatus::kFaultDetected)
      .export_values();

  py::enum_<FrameStatus>(m, "FrameStatus")
      .value("OK", FrameStatus::kOk)
      .value("OUTPUT_TOO_SMALL", FrameStatus::kOutputTooSmall)
      .value("INVALID_FID", FrameStatus::kInvalidFid)
      .value("INVALID_TOI", FrameStatus::kInvalidToi)
      .value("BCH_FAILED", FrameStatus::kBchFailed)
      .value("FAULT_DETECTED", FrameStatus::kFaultDetected)
      .value("LDPC_FAILED", FrameStatus::kLdpcFailed)
      .value("CRC_FAILED", FrameStatus::kCrcFailed)
      .export_values();

  py::enum_<MatchedCodeStatus>(m, "MatchedCodeStatus")
      .value("OK", MatchedCodeStatus::kOk)
      .value("INVALID_PRN", MatchedCodeStatus::kInvalidPrn)
      .value("INVALID_EPOCH", MatchedCodeStatus::kInvalidEpoch)
      .value("OUTPUT_TOO_SMALL", MatchedCodeStatus::kOutputTooSmall)
      .value("INVALID_ASSIGNMENT", MatchedCodeStatus::kInvalidAssignment)
      .value("FAULT_DETECTED", MatchedCodeStatus::kFaultDetected)
      .export_values();

  py::enum_<ModulationStatus>(m, "ModulationStatus")
      .value("OK", ModulationStatus::kOk)
      .value("INVALID_SYMBOL", ModulationStatus::kInvalidSymbol)
      .value("INVALID_CHIP_VALUE", ModulationStatus::kInvalidChipValue)
      .value("OUTPUT_TOO_SMALL", ModulationStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", ModulationStatus::kFaultDetected)
      .export_values();

  py::enum_<IqMuxStatus>(m, "IqMuxStatus")
      .value("OK", IqMuxStatus::kOk)
      .value("INPUT_TOO_SMALL", IqMuxStatus::kInputTooSmall)
      .value("OUTPUT_TOO_SMALL", IqMuxStatus::kOutputTooSmall)
      .value("INVALID_I_SAMPLE", IqMuxStatus::kInvalidISample)
      .value("INVALID_Q_SAMPLE", IqMuxStatus::kInvalidQSample)
      .value("NULL_INPUT", IqMuxStatus::kNullInput)
      .value("FAULT_DETECTED", IqMuxStatus::kFaultDetected)
      .export_values();

  py::enum_<LdpcSubframe>(m, "LdpcSubframe")
      .value("SF2", LdpcSubframe::kSF2)
      .value("SF3", LdpcSubframe::kSF3)
      .value("SF4", LdpcSubframe::kSF4)
      .export_values();

  py::enum_<LdpcStatus>(m, "LdpcStatus")
      .value("OK", LdpcStatus::kOk)
      .value("INVALID_INPUT", LdpcStatus::kInvalidInput)
      .value("OUTPUT_TOO_SMALL", LdpcStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", LdpcStatus::kFaultDetected)
      .value("NOT_CONVERGED", LdpcStatus::kNotConverged)
      .export_values();

  py::enum_<CrcStatus>(m, "CrcStatus")
      .value("OK", CrcStatus::kOk)
      .value("NULL_INPUT", CrcStatus::kNullInput)
      .value("OUTPUT_TOO_SMALL", CrcStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", CrcStatus::kFaultDetected)
      .export_values();

  py::enum_<InterleaverStatus>(m, "InterleaverStatus")
      .value("OK", InterleaverStatus::kOk)
      .value("INPUT_TOO_SMALL", InterleaverStatus::kInputTooSmall)
      .value("OUTPUT_TOO_SMALL", InterleaverStatus::kOutputTooSmall)
      .value("FAULT_DETECTED", InterleaverStatus::kFaultDetected)
      .export_values();

  py::class_<LdpcWorkspace>(m, "LdpcWorkspace")
      .def(py::init<>())
      .def_readwrite("alpha_numerator", &LdpcWorkspace::alpha_numerator)
      .def_readwrite("alpha_denominator", &LdpcWorkspace::alpha_denominator);

  py::enum_<PrnStatus>(m, "PrnStatus")
      .value("OK", PrnStatus::kOk)
      .value("INVALID_PRN", PrnStatus::kInvalidPrn)
      .value("NULL_OUTPUT", PrnStatus::kNullOutput)
      .value("INVALID_CHIP_INDEX", PrnStatus::kInvalidChipIndex)
      .value("FAULT_DETECTED", PrnStatus::kFaultDetected)
      .export_values();

  py::class_<BchResult>(m, "BchResult")
      .def_readonly("status", &BchResult::status)
      .def_readonly("fid", &BchResult::fid)
      .def_readonly("toi", &BchResult::toi)
      .def_readonly("hamming_distance", &BchResult::hamming_distance);

  m.def("bch_encode", [](uint8_t fid, uint8_t toi) {
    auto codeword = py::array_t<uint8_t>(52);
    if (bch_encode(static_cast<Fid>(fid), Toi(toi), std::span<uint8_t, 52>(codeword.mutable_data(), 52)) != BchStatus::kOk)
      throw py::value_error("Invalid BCH params");
    return codeword;
  });

  m.def("bch_decode", [](py::array_t<uint8_t> codeword) {
    if (codeword.size() != 52) throw py::value_error("Must be 52 symbols");
    return bch_decode(std::span<const uint8_t, 52>(codeword.data(), 52));
  });

  m.def("bch_decode_soft", [](py::array_t<float> llr) {
    if (llr.size() != 52) throw py::value_error("Must be 52 soft symbols");
    return bch_decode_soft(std::span<const float, 52>(llr.data(), 52));
  });

  m.def("bch_codebook_checksum", &bch_codebook_checksum);

  m.def("crc24_calculate", [](py::array_t<uint8_t> data) {
    auto out = py::array_t<uint8_t>(24);
    std::array<uint8_t, 24> crc{};
    auto status = crc24_calculate(
        std::span<const uint8_t>(data.data(), static_cast<size_t>(data.size())),
        std::span<uint8_t, 24>(crc.data(), 24));
    if (status != CrcStatus::kOk) throw py::value_error("CRC-24 failed");
    auto* dst = out.mutable_data();
    for (uint32_t i = 0; i < 24U; ++i) dst[i] = crc[i];
    return out;
  });

  m.def("crc24_verify", [](py::array_t<uint8_t> data_with_crc) {
    return crc24_verify(std::span<const uint8_t>(
        data_with_crc.data(), static_cast<size_t>(data_with_crc.size())));
  });

  m.def("block_interleave", [](py::array_t<uint8_t> in) {
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kInterleaverSize));
    auto status = block_interleave(
        std::span<const uint8_t>(in.data(), static_cast<size_t>(in.size())),
        std::span<uint8_t>(out.mutable_data(), kInterleaverSize));
    if (status != InterleaverStatus::kOk)
      throw py::value_error("block_interleave failed: input must be exactly 5880 bits");
    return out;
  });

  m.def("block_deinterleave", [](py::array_t<uint8_t> in) {
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kInterleaverSize));
    auto status = block_deinterleave(
        std::span<const uint8_t>(in.data(), static_cast<size_t>(in.size())),
        std::span<uint8_t>(out.mutable_data(), kInterleaverSize));
    if (status != InterleaverStatus::kOk)
      throw py::value_error("block_deinterleave failed: input must be exactly 5880 bits");
    return out;
  });

  m.def("block_deinterleave_soft", [](py::array_t<float, py::array::c_style | py::array::forcecast> in) {
    auto out = py::array_t<float>(static_cast<ssize_t>(kInterleaverSize));
    auto status = block_deinterleave_soft(
        std::span<const float>(in.data(), static_cast<size_t>(in.size())),
        std::span<float>(out.mutable_data(), kInterleaverSize));
    if (status != InterleaverStatus::kOk)
      throw py::value_error("block_deinterleave_soft failed");
    return out;
  });

  m.def("ldpc_encode", [](LdpcSubframe type, py::array_t<uint8_t> msg) {
    const size_t n_out = (type == LdpcSubframe::kSF2) ? 2400U : 1740U;
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(n_out));
    LdpcEncodeWorkspace enc_ws{};
    auto status = ldpc_encode(type, std::span<const uint8_t>(msg.data(), static_cast<size_t>(msg.size())), std::span<uint8_t>(out.mutable_data(), n_out), enc_ws);
    if (status != LdpcStatus::kOk) throw py::value_error("LDPC encode failed");
    return out;
  });

  m.def("ldpc_decode", [](LdpcSubframe type, py::array_t<float> symbols, uint8_t iterations, py::object workspace) {
    const size_t k_nom = (type == LdpcSubframe::kSF2) ? 1200U : 870U;
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(k_nom));
    // Stack-allocate workspace to comply with zero-dynamic-allocation mandate (§DC-1).
    LdpcWorkspace  local_ws_stack{};
    LdpcWorkspace* ws_ptr;
    if (workspace.is_none()) {
        ws_ptr = &local_ws_stack;
    } else {
        ws_ptr = workspace.cast<LdpcWorkspace*>();
    }

    auto status = ldpc_decode(type, std::span<const float>(symbols.data(), static_cast<size_t>(symbols.size())), iterations, std::span<uint8_t>(out.mutable_data(), k_nom), *ws_ptr);
    if (status != LdpcStatus::kOk && status != LdpcStatus::kNotConverged)
        throw py::value_error("LDPC decode failed");
    return py::make_tuple(out, status);
  }, py::arg("type"), py::arg("symbols"), py::arg("iterations"), py::arg("workspace") = py::none());

  py::class_<LdpcEncodeWorkspace>(m, "LdpcEncodeWorkspace")
      .def(py::init<>());

  m.def("frame_build_partial", [](uint8_t fid, uint8_t toi) {
    auto out = py::array_t<uint8_t>(6000);
    FrameBuildWorkspace fbw{};
    if (frame_build_partial(static_cast<Fid>(fid), Toi(toi), std::span<uint8_t, 6000>(out.mutable_data(), 6000), fbw) != FrameStatus::kOk)
      throw py::value_error("Frame build failed");
    return out;
  });

  m.def("frame_build", [](uint8_t fid, uint8_t toi,
                           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb2_data,
                           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb3_data,
                           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb4_data) {
    if (sb2_data.size() != kSb2DataBits) throw py::value_error("SB2 data must be 1176 bits");
    if (sb3_data.size() != kSb3DataBits) throw py::value_error("SB3 data must be 846 bits");
    if (sb4_data.size() != kSb4DataBits) throw py::value_error("SB4 data must be 846 bits");
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kFrameLength));
    LdpcEncodeWorkspace ws{};
    FrameBuildWorkspace fbw{};
    auto status = frame_build(
        static_cast<Fid>(fid), Toi(toi),
        std::span<const uint8_t, kSb2DataBits>(sb2_data.data(), kSb2DataBits),
        std::span<const uint8_t, kSb3DataBits>(sb3_data.data(), kSb3DataBits),
        std::span<const uint8_t, kSb4DataBits>(sb4_data.data(), kSb4DataBits),
        std::span<uint8_t, kFrameLength>(out.mutable_data(), kFrameLength),
        ws, fbw);
    if (status != FrameStatus::kOk) throw py::value_error("Frame build failed");
    return out;
  });

  m.def("commit_frame", [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> frame) {
    return commit_frame(std::span<const uint8_t>(frame.data(), static_cast<size_t>(frame.size())));
  });

  m.def("subframe_build_sb2", [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
    if (data.size() != kSb2DataBits) throw py::value_error("SB2 data must be 1176 bits");
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb2Length));
    LdpcEncodeWorkspace ws{};
    auto status = subframe_build_sb2(
        std::span<const uint8_t, kSb2DataBits>(data.data(), kSb2DataBits),
        std::span<uint8_t, kSb2Length>(out.mutable_data(), kSb2Length), ws);
    if (status != FrameStatus::kOk) throw py::value_error("SB2 build failed");
    return out;
  });

  m.def("subframe_build_sb3", [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
    if (data.size() != kSb3DataBits) throw py::value_error("SB3 data must be 846 bits");
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb3Length));
    LdpcEncodeWorkspace ws{};
    auto status = subframe_build_sb3(
        std::span<const uint8_t, kSb3DataBits>(data.data(), kSb3DataBits),
        std::span<uint8_t, kSb3Length>(out.mutable_data(), kSb3Length), ws);
    if (status != FrameStatus::kOk) throw py::value_error("SB3 build failed");
    return out;
  });

  m.def("subframe_build_sb4", [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
    if (data.size() != kSb4DataBits) throw py::value_error("SB4 data must be 846 bits");
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb4Length));
    LdpcEncodeWorkspace ws{};
    auto status = subframe_build_sb4(
        std::span<const uint8_t, kSb4DataBits>(data.data(), kSb4DataBits),
        std::span<uint8_t, kSb4Length>(out.mutable_data(), kSb4Length), ws);
    if (status != FrameStatus::kOk) throw py::value_error("SB4 build failed");
    return out;
  });

  m.def("prn_code", [](int prn_id) {
    const PrnId id{static_cast<uint8_t>(prn_id)};
    PrnCode p;
    if (gold_prn_packed(id, p) != PrnStatus::kOk) throw py::value_error("Invalid PRN");
    return unpack_prn(p);
  });

  m.def("modulate_i", [](py::array_t<uint8_t> prn, int data) {
    auto r = prn.request();
    auto out = py::array_t<int8_t>(r.size);
    if (modulate_bpsk_i(std::span<const uint8_t>(static_cast<const uint8_t*>(r.ptr), static_cast<size_t>(r.size)), static_cast<int8_t>(data), std::span<int8_t>(out.mutable_data(), static_cast<size_t>(r.size))) != ModulationStatus::kOk)
      throw py::value_error("Modulation failed");
    return out;
  });

  m.def("modulate_q", [](py::array_t<uint8_t> chips) {
    auto r = chips.request();
    auto out = py::array_t<int8_t>(r.size);
    if (modulate_bpsk_q(std::span<const uint8_t>(static_cast<const uint8_t*>(r.ptr), static_cast<size_t>(r.size)), std::span<int8_t>(out.mutable_data(), static_cast<size_t>(r.size))) != ModulationStatus::kOk)
      throw py::value_error("Modulation failed");
    return out;
  });

  m.def("multiplex_iq", [](py::array_t<int8_t> i, py::array_t<int8_t> q) {
    auto out = py::array_t<int16_t>(20460);
    if (multiplex_iq(std::span<const int8_t>(static_cast<const int8_t*>(i.data()), 2046), std::span<const int8_t>(static_cast<const int8_t*>(q.data()), 10230), std::span<int16_t>(out.mutable_data(), 20460)) != IqMuxStatus::kOk)
      throw py::value_error("Mux failed");
    return out.reshape({10230, 2});
  });

  m.def("weil10230_code", [](int id) { 
    PrnCode p; 
    if (weil10230_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk) throw py::value_error("Bad ID");
    return unpack_prn(p);
  });
  
  m.def("weil1500_code", [](int id) { 
    PrnCode p; 
    if (weil1500_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk) throw py::value_error("Bad ID");
    return unpack_prn(p);
  });

  m.def("matched_code_epoch", [](int id, int epoch) {
    auto out = py::array_t<uint8_t>(10230);
    MatchedCodeAssignment a;
    if (default_matched_assignment_checked(PrnId{static_cast<uint8_t>(id)}, a) != MatchedCodeStatus::kOk) throw py::value_error("Bad ID");
    if (matched_code_epoch_checked(a, static_cast<uint16_t>(epoch), std::span<uint8_t>(out.mutable_data(), 10230)) != MatchedCodeStatus::kOk) throw py::value_error("Bad epoch");
    return out;
  });

  m.def("matched_code_epoch_assigned", [](int primary_prn, int secondary_code_idx, int tertiary_prn, int tertiary_phase_offset, int epoch_idx) {
    MatchedCodeAssignment a;
    a.primary_prn = PrnId{static_cast<uint8_t>(primary_prn)};
    a.secondary_code_idx = CheckedRange<uint8_t, 0, 3>{static_cast<uint8_t>(secondary_code_idx)};
    a.tertiary_prn = PrnId{static_cast<uint8_t>(tertiary_prn)};
    a.tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>{static_cast<uint16_t>(tertiary_phase_offset)};
    auto out = py::array_t<uint8_t>(10230);
    if (matched_code_epoch_checked(a, static_cast<uint16_t>(epoch_idx), std::span<uint8_t>(out.mutable_data(), 10230)) != MatchedCodeStatus::kOk) throw py::value_error("Bad assignment");
    return out;
  });

  m.def("default_matched_assignment", [](int prn_id) {
    MatchedCodeAssignment a{};
    auto status = default_matched_assignment_checked(PrnId{static_cast<uint8_t>(prn_id)}, a);
    if (status != MatchedCodeStatus::kOk) throw py::value_error("Invalid PRN for assignment");
    return py::make_tuple(
        static_cast<uint8_t>(a.primary_prn),
        static_cast<uint8_t>(a.secondary_code_idx),
        static_cast<uint8_t>(a.tertiary_prn),
        static_cast<uint16_t>(a.tertiary_phase_offset));
  });

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

  py::enum_<SyncDetectorStatus>(m, "SyncDetectorStatus")
      .value("OK", SyncDetectorStatus::kOk)
      .value("INVALID_INPUT", SyncDetectorStatus::kInvalidInput)
      .value("NOT_DETECTED", SyncDetectorStatus::kNotDetected)
      .value("FAULT_DETECTED", SyncDetectorStatus::kFaultDetected)
      .export_values();

  py::class_<SyncResult>(m, "SyncResult")
      .def_readonly("frame_offset", &SyncResult::frame_offset)
      .def_readonly("peak_metric", &SyncResult::peak_metric)
      .def_readonly("second_peak", &SyncResult::second_peak)
      .def_readonly("detected", &SyncResult::detected);

  m.def("sync_detect", [](py::array_t<float, py::array::c_style | py::array::forcecast> rx_symbols,
                           float threshold) {
    NullTelemetryHal hal;
    SyncDetector det(hal);
    if (threshold > 0.0F) {
        if (!det.set_threshold(threshold)) {
            throw py::value_error("Invalid threshold");
        }
    }
    SyncResult result{};
    auto status = det.detect(
        std::span<const float>(rx_symbols.data(), static_cast<size_t>(rx_symbols.size())),
        result);
    return py::make_tuple(result, status);
  }, py::arg("rx_symbols"), py::arg("threshold") = 0.0F,
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

  py::enum_<FrameDecodeStatus>(m, "FrameDecodeStatus")
      .value("OK", FrameDecodeStatus::kOk)
      .value("SYNC_NOT_FOUND", FrameDecodeStatus::kSyncNotFound)
      .value("BCH_FAILED", FrameDecodeStatus::kBchFailed)
      .value("LDPC_FAILED", FrameDecodeStatus::kLdpcFailed)
      .value("CRC_FAILED", FrameDecodeStatus::kCrcFailed)
      .value("INVALID_INPUT", FrameDecodeStatus::kInvalidInput)
      .value("FAULT_DETECTED", FrameDecodeStatus::kFaultDetected)
      .export_values();

  py::class_<SubframeDecodeResult>(m, "SubframeDecodeResult")
      .def_readonly("ldpc_converged", &SubframeDecodeResult::ldpc_converged)
      .def_readonly("crc_ok", &SubframeDecodeResult::crc_ok);

  py::class_<FrameDecodeResult>(m, "FrameDecodeResult")
      .def_readonly("status", &FrameDecodeResult::status)
      .def_readonly("sync", &FrameDecodeResult::sync)
      .def_readonly("bch", &FrameDecodeResult::bch)
      .def_readonly("sb2", &FrameDecodeResult::sb2)
      .def_readonly("sb3", &FrameDecodeResult::sb3)
      .def_readonly("sb4", &FrameDecodeResult::sb4);

  m.def("frame_decode", [](py::array_t<float, py::array::c_style | py::array::forcecast> rx_symbols,
                            uint8_t ldpc_iterations,
                            float sync_threshold) {
    NullTelemetryHal hal;
    FrameDecodeResult result{};
    auto workspace = std::make_unique<FrameDecodeWorkspace>();

    const auto status = frame_decode(
        std::span<const float>(rx_symbols.data(), static_cast<size_t>(rx_symbols.size())),
        result, *workspace, hal, ldpc_iterations, sync_threshold);

    // Copy decoded data into numpy arrays — only when overall status is kOk
    // AND the individual subframe CRC passed.  On any fault, workspace data
    // buffers have been scrubbed by frame_decode() and must not be copied.
    auto sb2 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb2DataBits));
    auto sb3 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb3DataBits));
    auto sb4 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb4DataBits));

    if (status == FrameDecodeStatus::kOk && result.sb2.crc_ok) {
        auto* dst = sb2.mutable_data();
        for (size_t i = 0; i < kSb2DataBits; ++i) { dst[i] = workspace->sb2_data[i]; }
    }
    if (status == FrameDecodeStatus::kOk && result.sb3.crc_ok) {
        auto* dst = sb3.mutable_data();
        for (size_t i = 0; i < kSb3DataBits; ++i) { dst[i] = workspace->sb3_data[i]; }
    }
    if (status == FrameDecodeStatus::kOk && result.sb4.crc_ok) {
        auto* dst = sb4.mutable_data();
        for (size_t i = 0; i < kSb4DataBits; ++i) { dst[i] = workspace->sb4_data[i]; }
    }

    return py::make_tuple(result, sb2, sb3, sb4, status);
  }, py::arg("rx_symbols"),
     py::arg("ldpc_iterations") = kFrameDecodeLdpcIterations,
     py::arg("sync_threshold") = 0.0F,
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

  // ─── Correlator bindings ───────────────────────────────────────────────────

  py::enum_<CorrelatorStatus>(m, "CorrelatorStatus")
      .value("OK",             CorrelatorStatus::kOk)
      .value("INVALID_INPUT",  CorrelatorStatus::kInvalidInput)
      .value("ACQ_FAILED",     CorrelatorStatus::kAcqFailed)
      .value("FAULT_DETECTED", CorrelatorStatus::kFaultDetected)
      .value("NOT_LOCKED",     CorrelatorStatus::kNotLocked)
      .value("TIMEOUT",        CorrelatorStatus::kTimeout);

  py::enum_<ChannelState>(m, "ChannelState")
      .value("IDLE",      ChannelState::kIdle)
      .value("ACQUIRING", ChannelState::kAcquiring)
      .value("TRACKING",  ChannelState::kTracking)
      .value("LOST",      ChannelState::kLost);

  py::class_<AcquisitionResult>(m, "AcquisitionResult")
      .def_readonly("code_delay",   &AcquisitionResult::code_delay)
      .def_readonly("doppler_hz",   &AcquisitionResult::doppler_hz)
      .def_readonly("peak_metric",  &AcquisitionResult::peak_metric)
      .def_readonly("locked",       &AcquisitionResult::locked);

  py::class_<TrackingState>(m, "TrackingState")
      .def_readonly("code_phase_chips",  &TrackingState::code_phase_chips)
      .def_readonly("carrier_phase_rad", &TrackingState::carrier_phase_rad)
      .def_readonly("doppler_hz",        &TrackingState::doppler_hz)
      .def_readonly("dll_discriminator", &TrackingState::dll_discriminator)
      .def_readonly("pll_discriminator", &TrackingState::pll_discriminator)
      .def_readonly("cn0_estimate",      &TrackingState::cn0_estimate)
      .def_readonly("cn0_smooth",        &TrackingState::cn0_smooth);

  py::class_<UniformPrior>(m, "UniformPrior")
      .def(py::init<>());

  py::class_<Correlator>(m, "Correlator")
      .def(py::init([](UniformPrior& prior) {
          static NullTelemetryHal hal;
          return std::make_unique<Correlator>(prior, hal);
      }), py::arg("prior"), py::keep_alive<1, 2>(),
         "Construct a correlator with a UniformPrior.")
      .def("assign_channel", [](Correlator& self, uint8_t ch_idx, uint8_t prn_id) {
          return self.assign_channel(ch_idx, PrnId{prn_id});
      }, py::arg("ch_idx"), py::arg("prn_id"))
      .def("acquire", [](Correlator& self, uint8_t ch_idx,
                         py::array_t<std::complex<float>, py::array::c_style | py::array::forcecast> rx) {
          AcquisitionResult result{};
          auto status = self.acquire(ch_idx,
              std::span<const std::complex<float>>(rx.data(), static_cast<size_t>(rx.size())), result);
          return py::make_tuple(result, status);
      }, py::arg("ch_idx"), py::arg("rx"))
      .def("track", [](Correlator& self, uint8_t ch_idx,
                        py::array_t<std::complex<float>, py::array::c_style | py::array::forcecast> rx) {
          return self.track(ch_idx,
              std::span<const std::complex<float>>(rx.data(), static_cast<size_t>(rx.size())));
      }, py::arg("ch_idx"), py::arg("rx"))
      .def("pseudorange", [](const Correlator& self, uint8_t ch_idx) {
          double meters = 0.0;
          auto status = self.pseudorange(ch_idx, meters);
          return py::make_tuple(meters, status);
      }, py::arg("ch_idx"))
      .def("channel_state", &Correlator::channel_state, py::arg("ch_idx"))
      .def("tracking_state", &Correlator::tracking_state, py::arg("ch_idx"))
      .def("data_symbol", &Correlator::data_symbol, py::arg("ch_idx"),
           "Return the soft data symbol (prompt.real()) after the last track() call.")
      .def("reset_channel", &Correlator::reset_channel, py::arg("ch_idx"));

  m.attr("AFS_I_CODE_LENGTH") = kAfsICodeLength;
  m.attr("FFT_SIZE") = kFftSize;
  m.attr("SAMPLE_RATE_HZ") = kSampleRateHz;
  m.attr("CODE_PERIOD_S") = kCodePeriodS;
  m.attr("MAX_CHANNELS") = kMaxChannels;

  // PRN code constants
  m.attr("GOLD_CHIP_LENGTH") = kGoldChipLength;
  m.attr("WEIL_10230_CHIP_LENGTH") = kWeil10230ChipLength;
  m.attr("WEIL_1500_CHIP_LENGTH") = kWeil1500ChipLength;
  m.attr("PRN_COUNT") = kPrnCount;
  m.attr("CHIP_RATE_HZ") = kChipRateHz;
  m.attr("DOPPLER_BIN_SPACING_HZ") = kDopplerBinSpacingHz;

  // Legendre cache bindings
  py::enum_<LegendreStatus>(m, "LegendreStatus")
      .value("OK", LegendreStatus::kOk)
      .value("OUTPUT_TOO_SMALL", LegendreStatus::kOutputTooSmall)
      .value("INVALID_PRIME", LegendreStatus::kInvalidPrime)
      .value("FAULT_DETECTED", LegendreStatus::kFaultDetected)
      .export_values();

  m.def("legendre_cache_init", []() {
    return legendre_cache_init();
  });
  m.def("legendre_10223_checksum", &legendre_10223_checksum);
  m.def("legendre_1499_checksum", &legendre_1499_checksum);
}
