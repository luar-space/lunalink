// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/prn_codes.hpp"
#include "lunalink/signal/modulator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i chip mapping with +1 symbol", "[modulator]") {
  std::array<uint8_t, 2> chips = {0, 1};
  std::array<int8_t, 2> out{};
  // We use modulate_bpsk_any for tiny buffers
  REQUIRE(modulate_bpsk_any(chips, 1, out) == ModulationStatus::kOk);
  REQUIRE(out[0] == 1);  // 0 -> +1
  REQUIRE(out[1] == -1); // 1 -> -1
}

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i chip mapping with -1 symbol", "[modulator]") {
  std::array<uint8_t, 2> chips = {0, 1};
  std::array<int8_t, 2> out{};
  REQUIRE(modulate_bpsk_any(chips, -1, out) == ModulationStatus::kOk);
  REQUIRE(out[0] == -1); // (0 -> +1) * -1 = -1
  REQUIRE(out[1] == 1);  // (1 -> -1) * -1 = 1
}

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i full PRN 1 all values in {-1, +1}", "[modulator]") {
  // Unpack Gold PRN 1 chips from packed format
  PrnCode packed;
  REQUIRE(gold_prn_packed(PrnId{1}, packed) == PrnStatus::kOk);
  std::array<uint8_t, kGoldChipLength> chips{};
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(packed, i, chip) == PrnStatus::kOk);
    chips.at(i) = chip;
  }

  std::array<int8_t, kGoldChipLength> out{};
  REQUIRE(modulate_bpsk_i(chips, 1, out) == ModulationStatus::kOk);
  for (auto v : out)
    REQUIRE((v == -1 || v == 1));
}

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i returns explicit error status", "[modulator]") {
  std::array<uint8_t, 2> chips = {0, 1};
  std::array<int8_t, 1> out{};
  REQUIRE(modulate_bpsk_any(chips, 1, out) == ModulationStatus::kOutputTooSmall);
}

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i rejects invalid data symbol", "[modulator]") {
  std::array<uint8_t, kGoldChipLength> chips{};
  std::array<int8_t, kGoldChipLength> out{};
  REQUIRE(modulate_bpsk_i(chips, 0, out) == ModulationStatus::kInvalidSymbol);
}

TEST_CASE("[LSIS-AFS-201] modulate_bpsk_i rejects non-binary chips", "[modulator]") {
  std::array<uint8_t, kGoldChipLength> chips{};
  chips.fill(0);
  chips[100] = 2; // Invalid
  std::array<int8_t, kGoldChipLength> out{};
  REQUIRE(modulate_bpsk_i(chips, 1, out) == ModulationStatus::kInvalidChipValue);
}

TEST_CASE("modulate_bpsk_any: invalid chip value rejected", "[modulator]") {
  std::array<uint8_t, 10> chips = {0};
  std::array<int8_t, 10> out = {0};
  chips[0] = 2;
  CHECK(static_cast<uint8_t>(modulate_bpsk_any(chips, 1, out)) ==
        static_cast<uint8_t>(ModulationStatus::kInvalidChipValue));
}
