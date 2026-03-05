#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include <algorithm>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using namespace lunalink::signal;

PYBIND11_MODULE(_afs, m) {
  m.doc() = "LunaLink AFS C++ extension module.";

  m.def(
      "prn_code",
      [](int prn_id) -> py::array_t<uint8_t> {
        if (prn_id < 1 || prn_id > kGoldPrnCount)
          throw py::value_error("prn_id must be in [1, 210]");
        // Copy into a numpy-owned buffer — gold_prn() returns a pointer into
        // static storage; pybind11 must own the returned array.
        auto out = py::array_t<uint8_t>(kGoldChipLength);
        std::copy_n(gold_prn(static_cast<uint8_t>(prn_id)).data(),
                    kGoldChipLength, out.mutable_data());
        return out;
      },
      py::arg("prn_id"),
      "Return the Gold-2046 chip sequence for PRN prn_id (1-indexed).");

  m.def(
      "modulate_i",
      [](py::array_t<uint8_t, py::array::c_style> prn,
         int data_symbol) -> py::array_t<int8_t> {
        if (data_symbol != 1 && data_symbol != -1)
          throw py::value_error("data_symbol must be +1 or -1");
        auto r = prn.request();
        if (r.ndim != 1)
          throw py::value_error("prn must be a 1-D array");
        auto out = py::array_t<int8_t>(r.shape[0]);
        modulate_bpsk_i(static_cast<const uint8_t *>(r.ptr),
                        static_cast<uint16_t>(r.shape[0]),
                        static_cast<int8_t>(data_symbol), out.mutable_data());
        return out;
      },
      py::arg("prn"), py::arg("data_symbol"),
      "Modulate a chip sequence with a BPSK data symbol (AFS-I channel).");
}
