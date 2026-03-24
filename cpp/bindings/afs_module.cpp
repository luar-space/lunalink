#include "lunalink/codes/legendre.hpp"
#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"
#include "lunalink/encoding/bch.hpp"
#include "lunalink/encoding/crc.hpp"
#include "lunalink/encoding/interleaver.hpp"
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/messages/frame.hpp"
#include "lunalink/messages/frame_decode.hpp"
#include "lunalink/messages/message_parser.hpp"
#include "lunalink/messages/time_of_transmission.hpp"
#include "lunalink/signal/correlator.hpp"
#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/file_format.hpp"
#include "lunalink/signal/sync_detector.hpp"
#include "lunalink/core/sise.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <cstdint>
#include <span>

namespace py = pybind11;
using namespace lunalink::signal;

namespace {

py::array_t<uint8_t> unpack_prn(const PrnCode& code) {
  auto out = py::array_t<uint8_t>(code.chip_length);
  auto* dst = out.mutable_data();
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
      .def("__eq__",
           [](const Fid& f, py::object other) {
             if (py::isinstance<Fid>(other))
               return static_cast<uint8_t>(f) ==
                      static_cast<uint8_t>(other.cast<Fid>());
             if (py::isinstance<py::int_>(other))
               return static_cast<int>(static_cast<uint8_t>(f)) ==
                      other.cast<int>();
             return false;
           })
      .def("__hash__",
           [](const Fid& f) { return py::hash(py::int_(static_cast<uint8_t>(f))); })
      .def("__repr__",
           [](const Fid& f) {
             return "Fid(" + std::to_string(static_cast<uint8_t>(f)) + ")";
           })
      .def_static("NODE1", &Fid::kNode1)
      .def_static("NODE2", &Fid::kNode2)
      .def_static("NODE3", &Fid::kNode3)
      .def_static("NODE4", &Fid::kNode4);

  py::class_<Toi>(m, "Toi")
      .def(py::init<uint8_t>())
      .def_property("value", &Toi::value,
                     [](Toi& self, uint8_t v) { self = Toi(v); })
      .def("__int__", [](const Toi& t) { return static_cast<uint8_t>(t); })
      .def("__eq__",
           [](const Toi& t, py::object other) {
             if (py::isinstance<Toi>(other))
               return static_cast<uint8_t>(t) ==
                      static_cast<uint8_t>(other.cast<Toi>());
             if (py::isinstance<py::int_>(other))
               return static_cast<int>(static_cast<uint8_t>(t)) ==
                      other.cast<int>();
             return false;
           })
      .def("__hash__",
           [](const Toi& t) { return py::hash(py::int_(static_cast<uint8_t>(t))); })
      .def("__repr__", [](const Toi& t) {
        return "Toi(" + std::to_string(static_cast<uint8_t>(t)) + ")";
      });

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
    if (bch_encode(static_cast<Fid>(fid),
                   Toi(toi),
                   std::span<uint8_t, 52>(codeword.mutable_data(), 52)) != BchStatus::kOk)
      throw py::value_error("Invalid BCH params");
    return codeword;
  });

  m.def("bch_decode", [](py::array_t<uint8_t> codeword) {
    if (codeword.size() != 52)
      throw py::value_error("Must be 52 symbols");
    return bch_decode(std::span<const uint8_t, 52>(codeword.data(), 52));
  });

  m.def("bch_decode_soft", [](py::array_t<float> llr) {
    if (llr.size() != 52)
      throw py::value_error("Must be 52 soft symbols");
    return bch_decode_soft(std::span<const float, 52>(llr.data(), 52));
  });

  m.def("bch_codebook_checksum", &bch_codebook_checksum);

  m.def("crc24_calculate", [](py::array_t<uint8_t> data) {
    auto out = py::array_t<uint8_t>(24);
    std::array<uint8_t, 24> crc{};
    auto status =
        crc24_calculate(std::span<const uint8_t>(data.data(), static_cast<size_t>(data.size())),
                        std::span<uint8_t, 24>(crc.data(), 24));
    if (status != CrcStatus::kOk)
      throw py::value_error("CRC-24 failed");
    auto* dst = out.mutable_data();
    for (uint32_t i = 0; i < 24U; ++i)
      dst[i] = crc[i];
    return out;
  });

  m.def("crc24_verify", [](py::array_t<uint8_t> data_with_crc) {
    return crc24_verify(
        std::span<const uint8_t>(data_with_crc.data(), static_cast<size_t>(data_with_crc.size())));
  });

  m.def("block_interleave", [](py::array_t<uint8_t> in) {
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kInterleaverSize));
    auto status =
        block_interleave(std::span<const uint8_t>(in.data(), static_cast<size_t>(in.size())),
                         std::span<uint8_t>(out.mutable_data(), kInterleaverSize));
    if (status != InterleaverStatus::kOk)
      throw py::value_error("block_interleave failed: input must be exactly 5880 bits");
    return out;
  });

  m.def("block_deinterleave", [](py::array_t<uint8_t> in) {
    auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kInterleaverSize));
    auto status =
        block_deinterleave(std::span<const uint8_t>(in.data(), static_cast<size_t>(in.size())),
                           std::span<uint8_t>(out.mutable_data(), kInterleaverSize));
    if (status != InterleaverStatus::kOk)
      throw py::value_error("block_deinterleave failed: input must be exactly 5880 bits");
    return out;
  });

  m.def("block_deinterleave_soft",
        [](py::array_t<float, py::array::c_style | py::array::forcecast> in) {
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
    auto status = ldpc_encode(type,
                              std::span<const uint8_t>(msg.data(), static_cast<size_t>(msg.size())),
                              std::span<uint8_t>(out.mutable_data(), n_out),
                              enc_ws);
    if (status != LdpcStatus::kOk)
      throw py::value_error("LDPC encode failed");
    return out;
  });

  m.def(
      "ldpc_decode",
      [](LdpcSubframe type, py::array_t<float> symbols, uint8_t iterations, py::object workspace) {
        const size_t k_nom = (type == LdpcSubframe::kSF2) ? 1200U : 870U;
        auto out = py::array_t<uint8_t>(static_cast<ssize_t>(k_nom));
        // Stack-allocate workspace to comply with zero-dynamic-allocation mandate (§DC-1).
        LdpcWorkspace local_ws_stack{};
        LdpcWorkspace* ws_ptr;
        if (workspace.is_none()) {
          ws_ptr = &local_ws_stack;
        } else {
          ws_ptr = workspace.cast<LdpcWorkspace*>();
        }

        auto status =
            ldpc_decode(type,
                        std::span<const float>(symbols.data(), static_cast<size_t>(symbols.size())),
                        iterations,
                        std::span<uint8_t>(out.mutable_data(), k_nom),
                        *ws_ptr);
        if (status != LdpcStatus::kOk && status != LdpcStatus::kNotConverged)
          throw py::value_error("LDPC decode failed");
        return py::make_tuple(out, status);
      },
      py::arg("type"),
      py::arg("symbols"),
      py::arg("iterations"),
      py::arg("workspace") = py::none());

  py::class_<LdpcEncodeWorkspace>(m, "LdpcEncodeWorkspace").def(py::init<>());

  m.def("frame_build_partial", [](uint8_t fid, uint8_t toi) {
    auto out = py::array_t<uint8_t>(6000);
    FrameBuildWorkspace fbw{};
    if (frame_build_partial(static_cast<Fid>(fid),
                            Toi(toi),
                            std::span<uint8_t, 6000>(out.mutable_data(), 6000),
                            fbw) != FrameStatus::kOk)
      throw py::value_error("Frame build failed");
    return out;
  });

  m.def("frame_build",
        [](uint8_t fid,
           uint8_t toi,
           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb2_data,
           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb3_data,
           py::array_t<uint8_t, py::array::c_style | py::array::forcecast> sb4_data) {
          if (sb2_data.size() != kSb2DataBits)
            throw py::value_error("SB2 data must be 1176 bits");
          if (sb3_data.size() != kSb3DataBits)
            throw py::value_error("SB3 data must be 846 bits");
          if (sb4_data.size() != kSb4DataBits)
            throw py::value_error("SB4 data must be 846 bits");
          auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kFrameLength));
          LdpcEncodeWorkspace ws{};
          FrameBuildWorkspace fbw{};
          auto status =
              frame_build(static_cast<Fid>(fid),
                          Toi(toi),
                          std::span<const uint8_t, kSb2DataBits>(sb2_data.data(), kSb2DataBits),
                          std::span<const uint8_t, kSb3DataBits>(sb3_data.data(), kSb3DataBits),
                          std::span<const uint8_t, kSb4DataBits>(sb4_data.data(), kSb4DataBits),
                          std::span<uint8_t, kFrameLength>(out.mutable_data(), kFrameLength),
                          ws,
                          fbw);
          if (status != FrameStatus::kOk)
            throw py::value_error("Frame build failed");
          return out;
        });

  m.def("commit_frame", [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> frame) {
    return commit_frame(std::span<const uint8_t>(frame.data(), static_cast<size_t>(frame.size())));
  });

  m.def("subframe_build_sb2",
        [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
          if (data.size() != kSb2DataBits)
            throw py::value_error("SB2 data must be 1176 bits");
          auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb2Length));
          LdpcEncodeWorkspace ws{};
          auto status =
              subframe_build_sb2(std::span<const uint8_t, kSb2DataBits>(data.data(), kSb2DataBits),
                                 std::span<uint8_t, kSb2Length>(out.mutable_data(), kSb2Length),
                                 ws);
          if (status != FrameStatus::kOk)
            throw py::value_error("SB2 build failed");
          return out;
        });

  m.def("subframe_build_sb3",
        [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
          if (data.size() != kSb3DataBits)
            throw py::value_error("SB3 data must be 846 bits");
          auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb3Length));
          LdpcEncodeWorkspace ws{};
          auto status =
              subframe_build_sb3(std::span<const uint8_t, kSb3DataBits>(data.data(), kSb3DataBits),
                                 std::span<uint8_t, kSb3Length>(out.mutable_data(), kSb3Length),
                                 ws);
          if (status != FrameStatus::kOk)
            throw py::value_error("SB3 build failed");
          return out;
        });

  m.def("subframe_build_sb4",
        [](py::array_t<uint8_t, py::array::c_style | py::array::forcecast> data) {
          if (data.size() != kSb4DataBits)
            throw py::value_error("SB4 data must be 846 bits");
          auto out = py::array_t<uint8_t>(static_cast<ssize_t>(kSb4Length));
          LdpcEncodeWorkspace ws{};
          auto status =
              subframe_build_sb4(std::span<const uint8_t, kSb4DataBits>(data.data(), kSb4DataBits),
                                 std::span<uint8_t, kSb4Length>(out.mutable_data(), kSb4Length),
                                 ws);
          if (status != FrameStatus::kOk)
            throw py::value_error("SB4 build failed");
          return out;
        });

  m.def("prn_code", [](int prn_id) {
    const PrnId id{static_cast<uint8_t>(prn_id)};
    PrnCode p;
    if (gold_prn_packed(id, p) != PrnStatus::kOk)
      throw py::value_error("Invalid PRN");
    return unpack_prn(p);
  });

  m.def("modulate_i", [](py::array_t<uint8_t> prn, int data) {
    auto r = prn.request();
    auto out = py::array_t<int8_t>(r.size);
    if (modulate_bpsk_i(std::span<const uint8_t>(static_cast<const uint8_t*>(r.ptr),
                                                 static_cast<size_t>(r.size)),
                        static_cast<int8_t>(data),
                        std::span<int8_t>(out.mutable_data(), static_cast<size_t>(r.size))) !=
        ModulationStatus::kOk)
      throw py::value_error("Modulation failed");
    return out;
  });

  m.def("modulate_q", [](py::array_t<uint8_t> chips) {
    auto r = chips.request();
    auto out = py::array_t<int8_t>(r.size);
    if (modulate_bpsk_q(std::span<const uint8_t>(static_cast<const uint8_t*>(r.ptr),
                                                 static_cast<size_t>(r.size)),
                        std::span<int8_t>(out.mutable_data(), static_cast<size_t>(r.size))) !=
        ModulationStatus::kOk)
      throw py::value_error("Modulation failed");
    return out;
  });

  m.def("multiplex_iq", [](py::array_t<int8_t> i, py::array_t<int8_t> q) {
    auto out = py::array_t<int16_t>(20460);
    if (multiplex_iq(std::span<const int8_t>(static_cast<const int8_t*>(i.data()), 2046),
                     std::span<const int8_t>(static_cast<const int8_t*>(q.data()), 10230),
                     std::span<int16_t>(out.mutable_data(), 20460)) != IqMuxStatus::kOk)
      throw py::value_error("Mux failed");
    return out.reshape({10230, 2});
  });

  m.def("weil10230_code", [](int id) {
    PrnCode p;
    if (weil10230_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk)
      throw py::value_error("Bad ID");
    return unpack_prn(p);
  });

  m.def("weil1500_code", [](int id) {
    PrnCode p;
    if (weil1500_prn_packed(PrnId{static_cast<uint8_t>(id)}, p) != PrnStatus::kOk)
      throw py::value_error("Bad ID");
    return unpack_prn(p);
  });

  m.def("matched_code_epoch", [](int id, int epoch) {
    auto out = py::array_t<uint8_t>(10230);
    MatchedCodeAssignment a;
    if (default_matched_assignment_checked(PrnId{static_cast<uint8_t>(id)}, a) !=
        MatchedCodeStatus::kOk)
      throw py::value_error("Bad ID");
    if (matched_code_epoch_checked(
            a, static_cast<uint16_t>(epoch), std::span<uint8_t>(out.mutable_data(), 10230)) !=
        MatchedCodeStatus::kOk)
      throw py::value_error("Bad epoch");
    return out;
  });

  m.def("matched_code_epoch_assigned",
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
          auto out = py::array_t<uint8_t>(10230);
          if (matched_code_epoch_checked(a,
                                         static_cast<uint16_t>(epoch_idx),
                                         std::span<uint8_t>(out.mutable_data(), 10230)) !=
              MatchedCodeStatus::kOk)
            throw py::value_error("Bad assignment");
          return out;
        });

  m.def("default_matched_assignment", [](int prn_id) {
    MatchedCodeAssignment a{};
    auto status = default_matched_assignment_checked(PrnId{static_cast<uint8_t>(prn_id)}, a);
    if (status != MatchedCodeStatus::kOk)
      throw py::value_error("Invalid PRN for assignment");
    return py::make_tuple(static_cast<uint8_t>(a.primary_prn),
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

  m.def(
      "sync_detect",
      [](py::array_t<float, py::array::c_style | py::array::forcecast> rx_symbols,
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
      },
      py::arg("rx_symbols"),
      py::arg("threshold") = 0.0F,
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

  m.def(
      "frame_decode",
      [](py::array_t<float, py::array::c_style | py::array::forcecast> rx_symbols,
         uint8_t ldpc_iterations,
         float sync_threshold) {
        NullTelemetryHal hal;
        FrameDecodeResult result{};
        auto workspace = std::make_unique<FrameDecodeWorkspace>();

        const auto status = frame_decode(
            std::span<const float>(rx_symbols.data(), static_cast<size_t>(rx_symbols.size())),
            result,
            *workspace,
            hal,
            ldpc_iterations,
            sync_threshold);

        // Copy decoded data into numpy arrays — only when overall status is kOk
        // AND the individual subframe CRC passed.  On any fault, workspace data
        // buffers have been scrubbed by frame_decode() and must not be copied.
        auto sb2 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb2DataBits));
        auto sb3 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb3DataBits));
        auto sb4 = py::array_t<uint8_t>(static_cast<ssize_t>(kSb4DataBits));

        if (status == FrameDecodeStatus::kOk && result.sb2.crc_ok) {
          auto* dst = sb2.mutable_data();
          for (size_t i = 0; i < kSb2DataBits; ++i) {
            dst[i] = workspace->sb2_data[i];
          }
        }
        if (status == FrameDecodeStatus::kOk && result.sb3.crc_ok) {
          auto* dst = sb3.mutable_data();
          for (size_t i = 0; i < kSb3DataBits; ++i) {
            dst[i] = workspace->sb3_data[i];
          }
        }
        if (status == FrameDecodeStatus::kOk && result.sb4.crc_ok) {
          auto* dst = sb4.mutable_data();
          for (size_t i = 0; i < kSb4DataBits; ++i) {
            dst[i] = workspace->sb4_data[i];
          }
        }

        return py::make_tuple(result, sb2, sb3, sb4, status);
      },
      py::arg("rx_symbols"),
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
      .value("OK", CorrelatorStatus::kOk)
      .value("INVALID_INPUT", CorrelatorStatus::kInvalidInput)
      .value("ACQ_FAILED", CorrelatorStatus::kAcqFailed)
      .value("FAULT_DETECTED", CorrelatorStatus::kFaultDetected)
      .value("NOT_LOCKED", CorrelatorStatus::kNotLocked)
      .value("TIMEOUT", CorrelatorStatus::kTimeout);

  py::enum_<ChannelState>(m, "ChannelState")
      .value("IDLE", ChannelState::kIdle)
      .value("ACQUIRING", ChannelState::kAcquiring)
      .value("TRACKING", ChannelState::kTracking)
      .value("LOST", ChannelState::kLost);

  py::class_<AcquisitionResult>(m, "AcquisitionResult")
      .def_readonly("code_delay", &AcquisitionResult::code_delay)
      .def_readonly("doppler_hz", &AcquisitionResult::doppler_hz)
      .def_readonly("peak_metric", &AcquisitionResult::peak_metric)
      .def_readonly("locked", &AcquisitionResult::locked);

  py::class_<TrackingState>(m, "TrackingState")
      .def_readonly("code_phase_chips", &TrackingState::code_phase_chips)
      .def_readonly("carrier_phase_rad", &TrackingState::carrier_phase_rad)
      .def_readonly("doppler_hz", &TrackingState::doppler_hz)
      .def_readonly("dll_discriminator", &TrackingState::dll_discriminator)
      .def_readonly("pll_discriminator", &TrackingState::pll_discriminator)
      .def_readonly("cn0_estimate", &TrackingState::cn0_estimate)
      .def_readonly("cn0_smooth", &TrackingState::cn0_smooth);

  py::class_<UniformPrior>(m, "UniformPrior").def(py::init<>());

  py::class_<Correlator>(m, "Correlator")
      .def(py::init([](UniformPrior& prior) {
             static NullTelemetryHal hal;
             return std::make_unique<Correlator>(prior, hal);
           }),
           py::arg("prior"),
           py::keep_alive<1, 2>(),
           "Construct a correlator with a UniformPrior.")
      .def(
          "assign_channel",
          [](Correlator& self, uint8_t ch_idx, uint8_t prn_id) {
            return self.assign_channel(ch_idx, PrnId{prn_id});
          },
          py::arg("ch_idx"),
          py::arg("prn_id"))
      .def(
          "acquire",
          [](Correlator& self,
             uint8_t ch_idx,
             py::array_t<std::complex<float>, py::array::c_style | py::array::forcecast> rx) {
            AcquisitionResult result{};
            auto status = self.acquire(
                ch_idx,
                std::span<const std::complex<float>>(rx.data(), static_cast<size_t>(rx.size())),
                result);
            return py::make_tuple(result, status);
          },
          py::arg("ch_idx"),
          py::arg("rx"))
      .def(
          "track",
          [](Correlator& self,
             uint8_t ch_idx,
             py::array_t<std::complex<float>, py::array::c_style | py::array::forcecast> rx) {
            return self.track(
                ch_idx,
                std::span<const std::complex<float>>(rx.data(), static_cast<size_t>(rx.size())));
          },
          py::arg("ch_idx"),
          py::arg("rx"))
      .def(
          "pseudorange",
          [](const Correlator& self, uint8_t ch_idx) {
            double meters = 0.0;
            auto status = self.pseudorange(ch_idx, meters);
            return py::make_tuple(meters, status);
          },
          py::arg("ch_idx"))
      .def("channel_state", &Correlator::channel_state, py::arg("ch_idx"))
      .def("tracking_state", &Correlator::tracking_state, py::arg("ch_idx"))
      .def("data_symbol",
           &Correlator::data_symbol,
           py::arg("ch_idx"),
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
  py::enum_<LegendreStatus>(m, "LegendreStatus")
      .value("OK", LegendreStatus::kOk)
      .value("OUTPUT_TOO_SMALL", LegendreStatus::kOutputTooSmall)
      .value("INVALID_PRIME", LegendreStatus::kInvalidPrime)
      .value("FAULT_DETECTED", LegendreStatus::kFaultDetected)
      .export_values();

  m.def("legendre_cache_init", []() { return legendre_cache_init(); });
  m.def("legendre_10223_checksum", &legendre_10223_checksum);
  m.def("legendre_1499_checksum", &legendre_1499_checksum);

  // ─── Message Parser bindings (Gateway 6) ──────────────────────────────────

  py::enum_<MessageType>(m, "MessageType")
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

  py::enum_<MessageParseStatus>(m, "MessageParseStatus")
      .value("OK", MessageParseStatus::kOk)
      .value("INVALID_INPUT", MessageParseStatus::kInvalidInput)
      .value("FAULT_DETECTED", MessageParseStatus::kFaultDetected)
      .export_values();

  py::enum_<TotStatus>(m, "TotStatus")
      .value("OK", TotStatus::kOk)
      .value("INVALID_TOI", TotStatus::kInvalidToi)
      .value("INVALID_ITOW", TotStatus::kInvalidItow)
      .value("INVALID_WN", TotStatus::kInvalidWn)
      .value("INVALID_DELTA", TotStatus::kInvalidDelta)
      .value("FAULT_DETECTED", TotStatus::kFaultDetected)
      .export_values();

  py::class_<Sb2ParseResult>(m, "Sb2ParseResult")
      .def_readonly("status", &Sb2ParseResult::status)
      .def_readonly("wn", &Sb2ParseResult::wn)
      .def_readonly("itow", &Sb2ParseResult::itow)
      .def_readonly("data_offset", &Sb2ParseResult::data_offset)
      .def_readonly("payload_size", &Sb2ParseResult::payload_size);

  py::class_<Sb3ParseResult>(m, "Sb3ParseResult")
      .def_readonly("status", &Sb3ParseResult::status)
      .def_readonly("type_raw", &Sb3ParseResult::type_raw)
      .def_readonly("type", &Sb3ParseResult::type)
      .def_readonly("data_offset", &Sb3ParseResult::data_offset)
      .def_readonly("payload_size", &Sb3ParseResult::payload_size);

  py::class_<Sb4ParseResult>(m, "Sb4ParseResult")
      .def_readonly("status", &Sb4ParseResult::status)
      .def_readonly("type_raw", &Sb4ParseResult::type_raw)
      .def_readonly("type", &Sb4ParseResult::type)
      .def_readonly("data_offset", &Sb4ParseResult::data_offset)
      .def_readonly("payload_size", &Sb4ParseResult::payload_size);

  py::class_<TotResult>(m, "TotResult")
      .def_readonly("status", &TotResult::status)
      .def_readonly("t_f", &TotResult::t_f);

  // parse_sb2/sb3/sb4: accept py::object and validate dtype manually to prevent
  // silent float-to-int truncation from bypassing the C++ bit-value guard.
  // pybind11's forcecast and implicit conversion both coerce before the lambda
  // runs, so we must check the original array's dtype ourselves.
  m.def("parse_sb2",
        [](py::object obj) {
          auto arr = py::array::ensure(obj);
          if (!arr || !arr.dtype().is(py::dtype::of<uint8_t>())) {
            throw py::type_error("sb2_data must be a numpy array with dtype uint8");
          }
          auto sb2_data = py::array_t<uint8_t, py::array::c_style>::ensure(obj);
          if (!sb2_data) {
            throw py::type_error("sb2_data must be a C-contiguous uint8 array");
          }
          Sb2ParseResult result{};
          (void)parse_sb2(
              std::span<const uint8_t>(sb2_data.data(), static_cast<size_t>(sb2_data.size())), result);
          return result;
        },
        py::arg("sb2_data"),
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

  m.def("parse_sb3",
        [](py::object obj) {
          auto arr = py::array::ensure(obj);
          if (!arr || !arr.dtype().is(py::dtype::of<uint8_t>())) {
            throw py::type_error("sb3_data must be a numpy array with dtype uint8");
          }
          auto sb3_data = py::array_t<uint8_t, py::array::c_style>::ensure(obj);
          if (!sb3_data) {
            throw py::type_error("sb3_data must be a C-contiguous uint8 array");
          }
          Sb3ParseResult result{};
          (void)parse_sb3(
              std::span<const uint8_t>(sb3_data.data(), static_cast<size_t>(sb3_data.size())), result);
          return result;
        },
        py::arg("sb3_data"),
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

  m.def("parse_sb4",
        [](py::object obj) {
          auto arr = py::array::ensure(obj);
          if (!arr || !arr.dtype().is(py::dtype::of<uint8_t>())) {
            throw py::type_error("sb4_data must be a numpy array with dtype uint8");
          }
          auto sb4_data = py::array_t<uint8_t, py::array::c_style>::ensure(obj);
          if (!sb4_data) {
            throw py::type_error("sb4_data must be a C-contiguous uint8 array");
          }
          Sb4ParseResult result{};
          (void)parse_sb4(
              std::span<const uint8_t>(sb4_data.data(), static_cast<size_t>(sb4_data.size())), result);
          return result;
        },
        py::arg("sb4_data"),
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

  m.def("compute_tot",
        [](uint16_t wn, uint16_t itow, uint8_t toi, double delta_t_lrt) {
          return compute_tot(wn, itow, toi, delta_t_lrt);
        },
        py::arg("wn"),
        py::arg("itow"),
        py::arg("toi"),
        py::arg("delta_t_lrt") = 0.0,
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

  using lunalink::core::SiseStatus;
  using lunalink::core::SiseVec;
  using lunalink::core::SiseResult;
  using lunalink::core::SisePercentileResult;

  py::enum_<SiseStatus>(m, "SiseStatus")
      .value("OK", SiseStatus::kOk)
      .value("INVALID_INPUT", SiseStatus::kInvalidInput)
      .value("EMPTY_DATA", SiseStatus::kEmptyData)
      .value("FAULT_DETECTED", SiseStatus::kFaultDetected)
      .export_values();

  py::class_<SiseVec>(m, "SiseVec")
      .def(py::init<double, double, double, double>(),
           py::arg("x") = 0.0, py::arg("y") = 0.0, py::arg("z") = 0.0,
           py::arg("ct") = 0.0)
      .def_readwrite("x", &SiseVec::x)
      .def_readwrite("y", &SiseVec::y)
      .def_readwrite("z", &SiseVec::z)
      .def_readwrite("ct", &SiseVec::ct);

  py::class_<SiseResult>(m, "SiseResult")
      .def_readonly("status", &SiseResult::status)
      .def_readonly("position_error_m", &SiseResult::position_error_m)
      .def_readonly("velocity_error_mps", &SiseResult::velocity_error_mps);

  py::class_<SisePercentileResult>(m, "SisePercentileResult")
      .def_readonly("status", &SisePercentileResult::status)
      .def_readonly("position_percentile_m", &SisePercentileResult::position_percentile_m)
      .def_readonly("velocity_percentile_mps", &SisePercentileResult::velocity_percentile_mps)
      .def_readonly("position_pass", &SisePercentileResult::position_pass)
      .def_readonly("velocity_pass", &SisePercentileResult::velocity_pass);

  m.def("compute_sise",
        [](SiseVec pos_true, SiseVec pos_broadcast, SiseVec vel_true, SiseVec vel_broadcast) {
          return lunalink::core::compute_sise(pos_true, pos_broadcast,
                                              vel_true, vel_broadcast);
        },
        py::arg("pos_true"),
        py::arg("pos_broadcast"),
        py::arg("vel_true"),
        py::arg("vel_broadcast"),
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

  m.def("compute_sise_percentile",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> pos_errors,
           py::array_t<double, py::array::c_style | py::array::forcecast> vel_errors,
           double percentile) {
          const auto n = static_cast<size_t>(pos_errors.size());
          if (static_cast<size_t>(vel_errors.size()) != n) {
            throw py::value_error("pos_errors and vel_errors must have same length");
          }
          return lunalink::core::compute_sise_percentile(
              pos_errors.data(), vel_errors.data(), n, percentile);
        },
        py::arg("pos_errors_m"),
        py::arg("vel_errors_mps"),
        py::arg("percentile") = 0.95,
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
