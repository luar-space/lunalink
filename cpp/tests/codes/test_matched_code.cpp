// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/codes/matched_code.hpp"
#include "lunalink/codes/prn.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <span>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-401] secondary_code_index cycles through S0-S3", "[matched_code]") {
  uint8_t idx = 0;
  CHECK(secondary_code_index_checked(PrnId{1}, idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 0);
  CHECK(secondary_code_index_checked(PrnId{2}, idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 1);
  CHECK(secondary_code_index_checked(PrnId{3}, idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 2);
  CHECK(secondary_code_index_checked(PrnId{4}, idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 3);
  CHECK(secondary_code_index_checked(PrnId{5}, idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 0);
}

TEST_CASE("[LSIS-AFS-401] secondary codes match spec Table 10", "[matched_code]") {
  CHECK(kSecondaryCodes[0][0] == 1);
  CHECK(kSecondaryCodes[0][1] == 1);
  CHECK(kSecondaryCodes[0][2] == 1);
  CHECK(kSecondaryCodes[0][3] == 0);
  CHECK(kSecondaryCodes[1][0] == 0);
  CHECK(kSecondaryCodes[1][1] == 1);
  CHECK(kSecondaryCodes[1][2] == 1);
  CHECK(kSecondaryCodes[1][3] == 1);
}

// G-13: "matched_code_epoch produces binary chip values" deleted —
// `matched_code_epoch` output is the XOR of three already-binary
// inputs, so the per-chip `0 || 1` CHECK was tautological. Correctness
// is anchored by the `primary XOR secondary XOR tertiary` test below
// and the distinctness test elsewhere in this file.

TEST_CASE("[LSIS-AFS-401] matched_code_epoch equals primary XOR secondary XOR tertiary",
          "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  REQUIRE(matched_code_epoch(PrnId{1}, 0, out) == MatchedCodeStatus::kOk);

  PrnCode primary;
  REQUIRE(weil10230_prn_packed(PrnId{1}, primary) == PrnStatus::kOk);
  PrnCode tertiary;
  REQUIRE(weil1500_prn_packed(PrnId{1}, tertiary) == PrnStatus::kOk);

  const uint8_t sec_chip = 1; // S0[0]

  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t p_chip = 0;
    REQUIRE(unpack_chip(primary, static_cast<uint16_t>(i), p_chip) == PrnStatus::kOk);
    uint8_t t_chip = 0;
    REQUIRE(unpack_chip(tertiary, 0, t_chip) == PrnStatus::kOk);

    const uint8_t expected = p_chip ^ sec_chip ^ t_chip;
    if (out[i] != expected) {
      FAIL("Mismatch at chip " << i);
    }
  }
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch differs across secondary code boundary",
          "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> e0{};
  std::array<uint8_t, kWeil10230ChipLength> e3{};

  REQUIRE(matched_code_epoch(PrnId{1}, 0, e0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{1}, 3, e3) == MatchedCodeStatus::kOk);

  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    CHECK(e0[i] != e3[i]);
  }
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch changes across tertiary chip boundary",
          "[matched_code]") {
  // G-11: the previous version did
  //     if (t0 != t1) { CHECK(e0[0] != e4[0]); }
  // using Weil-1500 PRN 1, whose first two chips happen to be equal
  // (see test_prn.cpp: `weil1500 PRN 1 first 8 chips = {1,1,1,0,0,1,0,0}`),
  // so the CHECK never ran and the test silently passed. Fix: scan the
  // tertiary code of PRN 1 for the first chip index `i` where
  // tertiary[i] != tertiary[i+1], then test the matched code at epochs
  // `4*i` and `4*(i+1)` — the first tertiary boundary whose chip actually
  // flips. The assertion now runs unconditionally.
  std::array<uint8_t, kWeil10230ChipLength> e_before{};
  std::array<uint8_t, kWeil10230ChipLength> e_after{};

  PrnCode tertiary;
  REQUIRE(weil1500_prn_packed(PrnId{1}, tertiary) == PrnStatus::kOk);

  uint16_t flip_idx = 0;
  bool found = false;
  for (uint16_t i = 0; i < kWeil1500ChipLength - 1U; ++i) {
    uint8_t a = 0;
    uint8_t b = 0;
    REQUIRE(unpack_chip(tertiary, i, a) == PrnStatus::kOk);
    REQUIRE(unpack_chip(tertiary, static_cast<uint16_t>(i + 1U), b) == PrnStatus::kOk);
    if (a != b) {
      flip_idx = i;
      found = true;
      break;
    }
  }
  REQUIRE(found);

  const uint16_t epoch_before = static_cast<uint16_t>(4U * flip_idx);
  const uint16_t epoch_after = static_cast<uint16_t>(4U * (flip_idx + 1U));
  REQUIRE(matched_code_epoch(PrnId{1}, epoch_before, e_before) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{1}, epoch_after, e_after) == MatchedCodeStatus::kOk);
  CHECK(e_before[0] != e_after[0]);
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch different PRNs produce different sequences",
          "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out1{};
  std::array<uint8_t, kWeil10230ChipLength> out2{};
  std::array<uint8_t, kWeil10230ChipLength> out{};

  REQUIRE(matched_code_epoch(PrnId{1}, 0, out1) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{2}, 0, out2) == MatchedCodeStatus::kOk);
  CHECK(out1 != out2);

  // PRN 13 is invalid for matched code (interim assignment limited to 1-12)
  REQUIRE(matched_code_epoch(PrnId{13}, 0, out) == MatchedCodeStatus::kInvalidPrn);
}

TEST_CASE("kEpochsPerFrame is 6000", "[matched_code]") {
  CHECK(kEpochsPerFrame == 6000);
}

TEST_CASE("[LSIS-AFS-401] default interim mapping only applies to PRN 1-12", "[matched_code]") {
  MatchedCodeAssignment a{};
  CHECK(default_matched_assignment_checked(PrnId{1}, a) == MatchedCodeStatus::kOk);
  CHECK(default_matched_assignment_checked(PrnId{12}, a) == MatchedCodeStatus::kOk);
  // 13 is invalid for interim assignment
  CHECK(default_matched_assignment_checked(PrnId{13}, a) == MatchedCodeStatus::kInvalidPrn);
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch_checked rejects invalid inputs", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  std::array<uint8_t, 100> short_out{};
  MatchedCodeAssignment a{};
  REQUIRE(default_matched_assignment_checked(PrnId{1}, a) == MatchedCodeStatus::kOk);

  REQUIRE(matched_code_epoch_checked(a, kEpochsPerFrame, out) == MatchedCodeStatus::kInvalidEpoch);
  REQUIRE(matched_code_epoch_checked(a, 0, short_out) == MatchedCodeStatus::kOutputTooSmall);
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch_checked supports tertiary phase offset",
          "[matched_code]") {
  // G-11: the previous version guarded the `out0 != out1` CHECK on
  // `tertiary[0] != tertiary[1]` — which is false for PRN 1 (both are 1,
  // see `weil1500 PRN 1` first chips). The CHECK silently skipped. Fix:
  // scan for the first chip offset `k` where tertiary chips 0 and `k`
  // differ, then use phase offset `k`. The assertion now runs
  // unconditionally.
  PrnCode t;
  REQUIRE(weil1500_prn_packed(PrnId{1}, t) == PrnStatus::kOk);
  uint8_t c0 = 0;
  REQUIRE(unpack_chip(t, 0, c0) == PrnStatus::kOk);

  uint16_t phase = 0;
  bool found = false;
  for (uint16_t k = 1; k < kWeil1500ChipLength; ++k) {
    uint8_t ck = 0;
    REQUIRE(unpack_chip(t, k, ck) == PrnStatus::kOk);
    if (ck != c0) {
      phase = k;
      found = true;
      break;
    }
  }
  REQUIRE(found);

  std::array<uint8_t, kWeil10230ChipLength> out0{};
  std::array<uint8_t, kWeil10230ChipLength> out1{};
  MatchedCodeAssignment a0{};
  MatchedCodeAssignment a1{};

  REQUIRE(default_matched_assignment_checked(PrnId{1}, a0) == MatchedCodeStatus::kOk);
  a1 = a0;
  a1.tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>{phase};

  REQUIRE(matched_code_epoch_checked(a0, 0, out0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch_checked(a1, 0, out1) == MatchedCodeStatus::kOk);

  CHECK(out0 != out1);
}

TEST_CASE("checked matched helper APIs: saturating logic", "[matched_code]") {
  MatchedCodeAssignment a{};
  // Storage flip bypasses high-level logic, but value() saturates on read.
  a.primary_prn.storage.refresh(CheckedRange<uint8_t, 1, kPrnCount>{210});
  a.primary_prn.storage.v1 = CheckedRange<uint8_t, 1, kPrnCount>{255};
  // v1 becomes 210 due to saturating CheckedRange constructor
  CHECK(a.primary_prn.value() == 210);
}

namespace {
MatchedCodeAssignment make_test_assignment(PrnId p) {
  MatchedCodeAssignment a;
  a.primary_prn = p;
  a.tertiary_prn = PrnId(1);
  a.secondary_code_idx = CheckedRange<uint8_t, 0, 3>(0);
  a.tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>(0);
  return a;
}
} // namespace

TEST_CASE("matched_code_epoch_checked: error paths", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out_chips{};

  // Output too small
  std::span<uint8_t> small_out(out_chips.data(), 100);
  CHECK(static_cast<uint8_t>(
            matched_code_epoch_checked(make_test_assignment(PrnId(1)), 0, small_out)) ==
        static_cast<uint8_t>(MatchedCodeStatus::kOutputTooSmall));

  // Invalid Epoch
  CHECK(static_cast<uint8_t>(matched_code_epoch_checked(
            make_test_assignment(PrnId(1)), kEpochsPerFrame, out_chips)) ==
        static_cast<uint8_t>(MatchedCodeStatus::kInvalidEpoch));

  // Invalid PRN
  PrnId bad_prn(1);
  uint8_t over_prn = 250;
  bad_prn.storage.v1 = CheckedRange<uint8_t, 1, 210>(1);
  std::memcpy(&bad_prn.storage.v1, &over_prn, 1);
  std::memcpy(&bad_prn.storage.v2, &over_prn, 1);
  std::memcpy(&bad_prn.storage.v3, &over_prn, 1);

  auto assignment = make_test_assignment(bad_prn);
  CHECK(static_cast<uint8_t>(matched_code_epoch_checked(assignment, 0, out_chips)) ==
        static_cast<uint8_t>(MatchedCodeStatus::kInvalidAssignment));
}

TEST_CASE("matched_code_epoch_checked: invalid tertiary PRN", "[matched_code]") {
  MatchedCodeAssignment assignment;
  assignment.primary_prn = PrnId(1);
  assignment.secondary_code_idx = CheckedRange<uint8_t, 0, 3>(0);
  assignment.tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>(0);

  uint8_t bad_val = 250;
  assignment.tertiary_prn = PrnId(1);
  std::memcpy(&assignment.tertiary_prn.storage.v1, &bad_val, 1);
  std::memcpy(&assignment.tertiary_prn.storage.v2, &bad_val, 1);
  std::memcpy(&assignment.tertiary_prn.storage.v3, &bad_val, 1);

  std::array<uint8_t, kWeil10230ChipLength> out{};
  CHECK(matched_code_epoch_checked(assignment, 0, out) == MatchedCodeStatus::kInvalidAssignment);
}

TEST_CASE("[LSIS-AFS-401] Matched Code Coherency (LSIS-172)", "[matched_code]") {
  PrnId prn{1};
  MatchedCodeAssignment assignment;
  REQUIRE(default_matched_assignment_checked(prn, assignment) == MatchedCodeStatus::kOk);

  std::array<uint8_t, kWeil10230ChipLength> epoch0{};
  std::array<uint8_t, kWeil10230ChipLength> epoch1{};
  std::array<uint8_t, kWeil10230ChipLength> epoch4{};

  // 1. Verify Secondary Code alignment
  // S0 = [1, 1, 1, 0]
  // Epoch 0: secondary chip should be 1
  // Epoch 3: secondary chip should be 0
  REQUIRE(matched_code_epoch_checked(assignment, 0, epoch0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch_checked(assignment, 3, epoch1) == MatchedCodeStatus::kOk);

  // They should differ because secondary bit flipped (XOR modifier)
  CHECK(epoch0[0] != epoch1[0]);

  // 2. Verify Tertiary Code alignment (Ns = 4)
  // Tertiary chip only changes every 4 primary epochs.
  // Epoch 0-3: same tertiary chip.
  // Epoch 4k: next tertiary chip at tertiary index k.
  //
  // G-11: the previous version guarded the CHECK on
  // `tertiary[0] != tertiary[1]`, which for `assignment.tertiary_prn`
  // derived from PrnId{1} is `{1,1}` — false, so the CHECK silently
  // skipped. Fix: scan for the first tertiary chip index `k` where
  // tertiary[0] != tertiary[k], then test epoch 0 vs epoch 4k. Because
  // S0 is length-4, epoch 0 and epoch 4k always share the same
  // secondary chip (S0[0] == S0[(4k) mod 4]), so any difference isolates
  // the tertiary flip.
  REQUIRE(matched_code_epoch_checked(assignment, 0, epoch0) == MatchedCodeStatus::kOk);

  PrnCode t_code;
  REQUIRE(weil1500_prn_packed(assignment.tertiary_prn, t_code) == PrnStatus::kOk);
  uint8_t t0 = 0;
  REQUIRE(unpack_chip(t_code, 0, t0) == PrnStatus::kOk);

  uint16_t flip_k = 0;
  bool found = false;
  for (uint16_t k = 1; k < kWeil1500ChipLength; ++k) {
    uint8_t tk = 0;
    REQUIRE(unpack_chip(t_code, k, tk) == PrnStatus::kOk);
    if (tk != t0) {
      flip_k = k;
      found = true;
      break;
    }
  }
  REQUIRE(found);

  const uint16_t epoch_flip = static_cast<uint16_t>(4U * flip_k);
  REQUIRE(matched_code_epoch_checked(assignment, epoch_flip, epoch4) == MatchedCodeStatus::kOk);
  CHECK(epoch0[0] != epoch4[0]);
}
