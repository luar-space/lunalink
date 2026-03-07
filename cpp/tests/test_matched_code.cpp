#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/matched_code.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <span>

using namespace lunalink::signal;

TEST_CASE("secondary_code_index cycles through S0-S3") {
  uint8_t idx = 0;
  CHECK(secondary_code_index_checked(PrnId{1}, &idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 0);
  CHECK(secondary_code_index_checked(PrnId{2}, &idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 1);
  CHECK(secondary_code_index_checked(PrnId{3}, &idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 2);
  CHECK(secondary_code_index_checked(PrnId{4}, &idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 3);
  CHECK(secondary_code_index_checked(PrnId{5}, &idx) == MatchedCodeStatus::kOk);
  CHECK(idx == 0);
}

TEST_CASE("secondary codes match spec Table 10") {
  CHECK(kSecondaryCodes[0][0] == 1);
  CHECK(kSecondaryCodes[0][1] == 1);
  CHECK(kSecondaryCodes[0][2] == 1);
  CHECK(kSecondaryCodes[0][3] == 0);
  CHECK(kSecondaryCodes[1][0] == 0);
  CHECK(kSecondaryCodes[1][1] == 1);
  CHECK(kSecondaryCodes[1][2] == 1);
  CHECK(kSecondaryCodes[1][3] == 1);
}

TEST_CASE("matched_code_epoch produces binary chip values") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  REQUIRE(matched_code_epoch(PrnId{1}, 0, out) == MatchedCodeStatus::kOk);
  for (const auto chip : out) {
    CHECK((chip == 0 || chip == 1));
  }
}

TEST_CASE("matched_code_epoch equals primary XOR secondary XOR tertiary") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  REQUIRE(matched_code_epoch(PrnId{1}, 0, out) == MatchedCodeStatus::kOk);

  PrnCode primary;
  REQUIRE(weil10230_prn_packed(PrnId{1}, primary) == PrnStatus::kOk);
  PrnCode tertiary;
  REQUIRE(weil1500_prn_packed(PrnId{1}, tertiary) == PrnStatus::kOk);

  const uint8_t sec_chip = 1; // S0[0]

  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t p_chip = 0;
    REQUIRE(unpack_chip(primary, static_cast<uint16_t>(i), &p_chip) == PrnStatus::kOk);
    uint8_t t_chip = 0;
    REQUIRE(unpack_chip(tertiary, 0, &t_chip) == PrnStatus::kOk);

    const uint8_t expected = p_chip ^ sec_chip ^ t_chip;
    if (out[i] != expected) {
      FAIL("Mismatch at chip " << i);
    }
  }
}

TEST_CASE("matched_code_epoch differs across secondary code boundary") {
  std::array<uint8_t, kWeil10230ChipLength> e0{};
  std::array<uint8_t, kWeil10230ChipLength> e3{};
  
  REQUIRE(matched_code_epoch(PrnId{1}, 0, e0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{1}, 3, e3) == MatchedCodeStatus::kOk);

  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    CHECK(e0[i] != e3[i]);
  }
}

TEST_CASE("matched_code_epoch changes across tertiary chip boundary") {
  std::array<uint8_t, kWeil10230ChipLength> e0{};
  std::array<uint8_t, kWeil10230ChipLength> e4{};

  PrnCode tertiary;
  REQUIRE(weil1500_prn_packed(PrnId{1}, tertiary) == PrnStatus::kOk);
  uint8_t t0 = 0, t1 = 0;
  REQUIRE(unpack_chip(tertiary, 0, &t0) == PrnStatus::kOk);
  REQUIRE(unpack_chip(tertiary, 1, &t1) == PrnStatus::kOk);

  if (t0 != t1) {
    REQUIRE(matched_code_epoch(PrnId{1}, 0, e0) == MatchedCodeStatus::kOk);
    REQUIRE(matched_code_epoch(PrnId{1}, 4, e4) == MatchedCodeStatus::kOk);
    CHECK(e0[0] != e4[0]);
  }
}

TEST_CASE("matched_code_epoch different PRNs produce different sequences") {
  std::array<uint8_t, kWeil10230ChipLength> out1{};
  std::array<uint8_t, kWeil10230ChipLength> out2{};
  std::array<uint8_t, kWeil10230ChipLength> out{};

  REQUIRE(matched_code_epoch(PrnId{1}, 0, out1) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch(PrnId{2}, 0, out2) == MatchedCodeStatus::kOk);
  CHECK(out1 != out2);

  REQUIRE(matched_code_epoch(PrnId{13}, 0, out) == MatchedCodeStatus::kInvalidPrn);
}

TEST_CASE("kEpochsPerFrame is 6000") {
  CHECK(kEpochsPerFrame == 6000);
}

TEST_CASE("default interim mapping only applies to PRN 1-12") {
  MatchedCodeAssignment a{};
  CHECK(default_matched_assignment_checked(PrnId{1}, &a) == MatchedCodeStatus::kOk);
  CHECK(default_matched_assignment_checked(PrnId{12}, &a) == MatchedCodeStatus::kOk);
  CHECK(default_matched_assignment_checked(PrnId{13}, &a) == MatchedCodeStatus::kInvalidPrn);
}

TEST_CASE("matched_code_epoch_checked rejects invalid inputs") {
  std::array<uint8_t, kWeil10230ChipLength> out{};
  std::array<uint8_t, 100> short_out{};
  MatchedCodeAssignment a{};
  REQUIRE(default_matched_assignment_checked(PrnId{1}, &a) == MatchedCodeStatus::kOk);

  REQUIRE(matched_code_epoch_checked(a, kEpochsPerFrame, out) ==
          MatchedCodeStatus::kInvalidEpoch);
  REQUIRE(matched_code_epoch_checked(a, 0, short_out) ==
          MatchedCodeStatus::kOutputTooSmall);
}

TEST_CASE("matched_code_epoch_checked supports tertiary phase offset") {
  std::array<uint8_t, kWeil10230ChipLength> out0{};
  std::array<uint8_t, kWeil10230ChipLength> out1{};
  MatchedCodeAssignment a0{}, a1{};
  
  REQUIRE(default_matched_assignment_checked(PrnId{1}, &a0) == MatchedCodeStatus::kOk);
  a1 = a0;
  a1.tertiary_phase_offset = 1;

  REQUIRE(matched_code_epoch_checked(a0, 0, out0) == MatchedCodeStatus::kOk);
  REQUIRE(matched_code_epoch_checked(a1, 0, out1) == MatchedCodeStatus::kOk);

  PrnCode t;
  REQUIRE(weil1500_prn_packed(PrnId{1}, t) == PrnStatus::kOk);
  uint8_t c0=0, c1=0;
  REQUIRE(unpack_chip(t, 0, &c0) == PrnStatus::kOk);
  REQUIRE(unpack_chip(t, 1, &c1) == PrnStatus::kOk);

  if (c0 != c1) {
    CHECK(out0 != out1);
  }
}

TEST_CASE("checked matched helper APIs reject invalid inputs") {
  MatchedCodeAssignment a{};
  REQUIRE(default_matched_assignment_checked(PrnId{13}, &a) ==
          MatchedCodeStatus::kInvalidPrn);
  REQUIRE(default_matched_assignment_checked(PrnId{1}, nullptr) ==
          MatchedCodeStatus::kInvalidAssignment);
}
