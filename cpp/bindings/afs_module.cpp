#include "lunalink/signal/bch.hpp"
#include "lunalink/signal/frame.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/matched_code.hpp"
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
      .def_readwrite("value", &Toi::value);

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

  m.def("bch_codebook_checksum", &bch_codebook_checksum);

  m.def("frame_build_partial", [](uint8_t fid, uint8_t toi) {
    auto out = py::array_t<uint8_t>(6000);
    if (frame_build_partial(static_cast<Fid>(fid), Toi(toi), std::span<uint8_t, 6000>(out.mutable_data(), 6000)) != FrameStatus::kOk)
      throw py::value_error("Frame build failed");
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
}
