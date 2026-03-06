#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/tiered_code.hpp"
#include <cstdint>
#include <limits>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace lunalink::signal;

namespace {

/// Unpack a packed PRN into a numpy uint8 array of {0, 1} chip values.
py::array_t<uint8_t> unpack_prn(const uint8_t *packed, uint16_t chip_length) {
  auto out = py::array_t<uint8_t>(chip_length);
  auto *dst = out.mutable_data();
  for (uint16_t i = 0; i < chip_length; ++i)
    dst[i] = unpack_chip(packed, i);
  return out;
}

} // namespace

PYBIND11_MODULE(_afs, m) {
  m.doc() = "LunaLink AFS C++ extension module.";

  m.def(
      "prn_code",
      [](int prn_id) -> py::array_t<uint8_t> {
        if (prn_id < 1 || prn_id > kPrnCount)
          throw py::value_error("prn_id must be in [1, 210]");
        return unpack_prn(gold_prn_packed(static_cast<uint8_t>(prn_id)),
                          kGoldChipLength);
      },
      py::arg("prn_id"),
      "Return the Gold-2046 chip sequence for PRN prn_id (1-indexed).");

  m.def(
      "weil10230_code",
      [](int prn_id) -> py::array_t<uint8_t> {
        if (prn_id < 1 || prn_id > kPrnCount)
          throw py::value_error("prn_id must be in [1, 210]");
        return unpack_prn(weil10230_prn_packed(static_cast<uint8_t>(prn_id)),
                          kWeil10230ChipLength);
      },
      py::arg("prn_id"),
      "Return the Weil-10230 chip sequence for PRN prn_id (1-indexed).");

  m.def(
      "weil1500_code",
      [](int prn_id) -> py::array_t<uint8_t> {
        if (prn_id < 1 || prn_id > kPrnCount)
          throw py::value_error("prn_id must be in [1, 210]");
        return unpack_prn(weil1500_prn_packed(static_cast<uint8_t>(prn_id)),
                          kWeil1500ChipLength);
      },
      py::arg("prn_id"),
      "Return the Weil-1500 chip sequence for PRN prn_id (1-indexed).");

  m.def(
      "modulate_i",
      [](py::array_t<uint8_t, py::array::c_style> prn,
         int data_symbol) -> py::array_t<int8_t> {
        if (data_symbol != 1 && data_symbol != -1)
          throw py::value_error("data_symbol must be +1 or -1");
        auto r = prn.request();
        if (r.ndim != 1)
          throw py::value_error("prn must be a 1-D array");
        if (r.shape[0] > std::numeric_limits<uint16_t>::max())
          throw py::value_error("prn array exceeds maximum chip count (65535)");
        auto out = py::array_t<int8_t>(r.shape[0]);
        modulate_bpsk_i(static_cast<const uint8_t *>(r.ptr),
                        static_cast<uint16_t>(r.shape[0]),
                        static_cast<int8_t>(data_symbol), out.mutable_data());
        return out;
      },
      py::arg("prn"), py::arg("data_symbol"),
      "Modulate a chip sequence with a BPSK data symbol (AFS-I channel).");

  m.def(
      "tiered_code_epoch",
      [](int prn_id, int epoch_idx) -> py::array_t<uint8_t> {
        if (prn_id < 1 || prn_id > kPrnCount)
          throw py::value_error("prn_id must be in [1, 210]");
        if (epoch_idx < 0 || epoch_idx >= kEpochsPerFrame)
          throw py::value_error("epoch_idx must be in [0, 5999]");
        auto out = py::array_t<uint8_t>(kWeil10230ChipLength);
        const bool ok = tiered_code_epoch_checked(
            default_tiered_assignment(static_cast<uint8_t>(prn_id)),
            static_cast<uint16_t>(epoch_idx), out.mutable_data());
        if (!ok)
          throw py::value_error("invalid tiered code assignment");
        return out;
      },
      py::arg("prn_id"), py::arg("epoch_idx"),
      "Return one primary epoch (10230 chips) of the tiered AFS-Q code.");

  m.def(
      "tiered_code_epoch_assigned",
      [](int primary_prn,
         int secondary_code_idx,
         int tertiary_prn,
         int tertiary_phase_offset,
         int epoch_idx) -> py::array_t<uint8_t> {
        if (epoch_idx < 0 || epoch_idx >= kEpochsPerFrame)
          throw py::value_error("epoch_idx must be in [0, 5999]");
        if (primary_prn < 1 || primary_prn > kPrnCount || tertiary_prn < 1 ||
            tertiary_prn > kPrnCount || secondary_code_idx < 0 ||
            secondary_code_idx >= kSecondaryCodeCount || tertiary_phase_offset < 0 ||
            tertiary_phase_offset >= kWeil1500ChipLength) {
          throw py::value_error(
              "assignment invalid: primary/tertiary PRN in [1,210], secondary in "
              "[0,3], tertiary_phase_offset in [0,1499]");
        }
        TieredCodeAssignment a{};
        a.primary_prn = static_cast<uint8_t>(primary_prn);
        a.secondary_code_idx = static_cast<uint8_t>(secondary_code_idx);
        a.tertiary_prn = static_cast<uint8_t>(tertiary_prn);
        a.tertiary_phase_offset = static_cast<uint16_t>(tertiary_phase_offset);
        auto out = py::array_t<uint8_t>(kWeil10230ChipLength);
        const bool ok = tiered_code_epoch_checked(
            a, static_cast<uint16_t>(epoch_idx), out.mutable_data());
        if (!ok)
          throw py::value_error("invalid tiered code assignment");
        return out;
      },
      py::arg("primary_prn"), py::arg("secondary_code_idx"),
      py::arg("tertiary_prn"), py::arg("tertiary_phase_offset"),
      py::arg("epoch_idx"),
      "Return one primary epoch using explicit AFS-Q code assignments.");

  m.attr("EPOCHS_PER_FRAME") = kEpochsPerFrame;
  m.attr("SECONDARY_CODE_LENGTH") = kSecondaryCodeLength;
  m.attr("SECONDARY_CODE_COUNT") = kSecondaryCodeCount;
  m.attr("TERTIARY_CODE_LENGTH") = kWeil1500ChipLength;
}
