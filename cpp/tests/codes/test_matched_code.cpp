#include "lunalink/codes/prn.hpp"
#include "lunalink/codes/matched_code.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <span>
#include <cstring>

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

TEST_CASE("[LSIS-AFS-401] matched_code_epoch produces binary chip values", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  REQUIRE(matched_code_epoch(PrnId{1}, 0, out) == MatchedCodeStatus::kOk);
  for (const auto chip : out) {
    CHECK((chip == 0 || chip == 1));
  }
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch equals primary XOR secondary XOR tertiary", "[matched_code]") {
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

TEST_CASE("[LSIS-AFS-401] matched_code_epoch differs across secondary code boundary", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> e0{};
  std::array<uint8_t, kWeil10230ChipLength> e3{};
  
  REQUIRE(matched_code_epoch(PrnId{1}, 0, e0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{1}, 3, e3) == MatchedCodeStatus::kOk);

  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    CHECK(e0[i] != e3[i]);
  }
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch changes across tertiary chip boundary", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> e0{};
  std::array<uint8_t, kWeil10230ChipLength> e4{};

  PrnCode tertiary;
  REQUIRE(weil1500_prn_packed(PrnId{1}, tertiary) == PrnStatus::kOk);
  uint8_t t0 = 0, t1 = 0;
  REQUIRE(unpack_chip(tertiary, 0, t0) == PrnStatus::kOk);
  REQUIRE(unpack_chip(tertiary, 1, t1) == PrnStatus::kOk);

  if (t0 != t1) {
    REQUIRE(matched_code_epoch(PrnId{1}, 0, e0) == MatchedCodeStatus::kOk);
    REQUIRE(matched_code_epoch(PrnId{1}, 4, e4) == MatchedCodeStatus::kOk);
    CHECK(e0[0] != e4[0]);
  }
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch different PRNs produce different sequences", "[matched_code]") {
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

  REQUIRE(matched_code_epoch_checked(a, kEpochsPerFrame, out) ==
          MatchedCodeStatus::kInvalidEpoch);
  REQUIRE(matched_code_epoch_checked(a, 0, short_out) ==
          MatchedCodeStatus::kOutputTooSmall);
}

TEST_CASE("[LSIS-AFS-401] matched_code_epoch_checked supports tertiary phase offset", "[matched_code]") {
  std::array<uint8_t, kWeil10230ChipLength> out0{};
  std::array<uint8_t, kWeil10230ChipLength> out1{};
  MatchedCodeAssignment a0{}, a1{};
  
  REQUIRE(default_matched_assignment_checked(PrnId{1}, a0) == MatchedCodeStatus::kOk);
  a1 = a0;
  a1.tertiary_phase_offset = CheckedRange<uint16_t, 0, 1499>{1};

  REQUIRE(matched_code_epoch_checked(a0, 0, out0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch_checked(a1, 0, out1) == MatchedCodeStatus::kOk);

  PrnCode t;
  REQUIRE(weil1500_prn_packed(PrnId{1}, t) == PrnStatus::kOk);
  uint8_t c0=0, c1=0;
  REQUIRE(unpack_chip(t, 0, c0) == PrnStatus::kOk);
  REQUIRE(unpack_chip(t, 1, c1) == PrnStatus::kOk);

  if (c0 != c1) {
    CHECK(out0 != out1);
  }
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
    CHECK(static_cast<uint8_t>(matched_code_epoch_checked(make_test_assignment(PrnId(1)), 0, small_out)) == static_cast<uint8_t>(MatchedCodeStatus::kOutputTooSmall));

    // Invalid Epoch
    CHECK(static_cast<uint8_t>(matched_code_epoch_checked(make_test_assignment(PrnId(1)), kEpochsPerFrame, out_chips)) == static_cast<uint8_t>(MatchedCodeStatus::kInvalidEpoch));

    // Invalid PRN
    PrnId bad_prn(1);
    uint8_t over_prn = 250;
    bad_prn.storage.v1 = CheckedRange<uint8_t, 1, 210>(1);
    std::memcpy(&bad_prn.storage.v1, &over_prn, 1);
    std::memcpy(&bad_prn.storage.v2, &over_prn, 1);
    std::memcpy(&bad_prn.storage.v3, &over_prn, 1);

    auto assignment = make_test_assignment(bad_prn);
    CHECK(static_cast<uint8_t>(matched_code_epoch_checked(assignment, 0, out_chips)) == static_cast<uint8_t>(MatchedCodeStatus::kInvalidAssignment));
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
    // Epoch 4: next tertiary chip.
    REQUIRE(matched_code_epoch_checked(assignment, 0, epoch0) == MatchedCodeStatus::kOk);

    // To isolate tertiary, we'd need to XOR out the primary and secondary.
    // But we can check that epoch 0 and 4 use different tertiary indices.
    // Tertiary index = (offset + floor(epoch/4)) % 1500
    // Epoch 0 -> index 0
    // Epoch 4 -> index 1

    PrnCode t_code;
    REQUIRE(weil1500_prn_packed(assignment.tertiary_prn, t_code) == PrnStatus::kOk);
    uint8_t t0=0, t1=0;
    REQUIRE(unpack_chip(t_code, 0, t0) == PrnStatus::kOk);
    REQUIRE(unpack_chip(t_code, 1, t1) == PrnStatus::kOk);

    if (t0 != t1) {
        // If tertiary chips differ, and secondary chip is same (S0[0] == S0[4%4]),
        // then epochs should differ.
        REQUIRE(matched_code_epoch_checked(assignment, 4, epoch4) == MatchedCodeStatus::kOk);
        CHECK(epoch0[0] != epoch4[0]);
    }
}
