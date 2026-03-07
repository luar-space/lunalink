#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/matched_code.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

TEST_CASE("multiplex_iq upsamples I by factor 5") {
  // PrnId 1
  std::array<uint8_t, kGoldChipLength> i_chips{};
  PrnCode u_i;
  REQUIRE(gold_prn_packed(PrnId{1}, u_i) == PrnStatus::kOk);
  // Unpack full PRN to bytes
  for (size_t i = 0; i < kGoldChipLength; ++i) {
    REQUIRE(unpack_chip(u_i, static_cast<uint16_t>(i), &i_chips[i]) == PrnStatus::kOk);
  }
  
  std::array<int8_t, kGoldChipLength> i_samples{};
  REQUIRE(modulate_bpsk_i(i_chips, 1, i_samples) == ModulationStatus::kOk);

  std::array<uint8_t, kWeil10230ChipLength> q_chips{};
  // Use matched_code_epoch for Q chips
  REQUIRE(matched_code_epoch(PrnId{1}, 0, q_chips) == MatchedCodeStatus::kOk);
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  REQUIRE(modulate_bpsk_q(q_chips, q_samples) == ModulationStatus::kOk);

  std::array<int16_t, kIqSamplesPerEpoch * 2> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // Check I channel upsampling (every even index in output)
  for (size_t i = 0; i < kGoldChipLength; ++i) {
    const auto expected_i = static_cast<int16_t>(i_samples[i]);
    for (size_t k = 0; k < 5; ++k) {
      const size_t out_idx = (i * 5 + k) * 2; // *2 for interleaved
      CHECK(out[out_idx] == expected_i);
    }
  }
}

TEST_CASE("multiplex_iq Q channel passes through at chip rate") {
  std::array<uint8_t, kGoldChipLength> i_chips{};
  PrnCode u_i;
  REQUIRE(gold_prn_packed(PrnId{1}, u_i) == PrnStatus::kOk);
  for (size_t i=0; i<kGoldChipLength; ++i) {
    REQUIRE(unpack_chip(u_i, static_cast<uint16_t>(i), &i_chips[i]) == PrnStatus::kOk);
  }
  std::array<int8_t, kGoldChipLength> i_samples{};
  REQUIRE(modulate_bpsk_i(i_chips, 1, i_samples) == ModulationStatus::kOk);

  std::array<uint8_t, kWeil10230ChipLength> q_chips{};
  REQUIRE(matched_code_epoch(PrnId{1}, 0, q_chips) == MatchedCodeStatus::kOk);
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  REQUIRE(modulate_bpsk_q(q_chips, q_samples) == ModulationStatus::kOk);

  std::array<int16_t, kIqSamplesPerEpoch * 2> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // Check Q channel (odd indices)
  for (size_t i = 0; i < kWeil10230ChipLength; ++i) {
    const auto expected_q = static_cast<int16_t>(q_samples[i]);
    const size_t out_idx = i * 2 + 1;
    CHECK(out[out_idx] == expected_q);
  }
}

TEST_CASE("multiplex_iq output length is 2 * 10230 interleaved") {
  CHECK(kIqSamplesPerEpoch == 10230);
  CHECK(kIqUpsampleFactor == 5);
}

TEST_CASE("multiplex_iq 50/50 power: I and Q have equal amplitude") {
  std::array<int8_t, 2046> i_s; i_s.fill(1);
  std::array<int8_t, 10230> q_s; q_s.fill(-1);
  std::array<int16_t, 20460> out;
  
  REQUIRE(multiplex_iq(i_s, q_s, out) == IqMuxStatus::kOk);
  
  CHECK(out[0] == 1);
  CHECK(out[1] == -1);
}

TEST_CASE("multiplex_iq rejects undersized inputs and outputs") {
  std::array<int8_t, 2046> i_ok{};
  std::array<int8_t, 10230> q_ok{};
  std::array<int16_t, 20460> out_ok{};
  
  std::array<int8_t, 10> i_short{};
  CHECK(multiplex_iq(i_short, q_ok, out_ok) == IqMuxStatus::kInputTooSmall);

  std::array<int8_t, 10> q_short{};
  CHECK(multiplex_iq(i_ok, q_short, out_ok) == IqMuxStatus::kInputTooSmall);

  std::array<int16_t, 10> out_short{};
  CHECK(multiplex_iq(i_ok, q_ok, out_short) == IqMuxStatus::kOutputTooSmall);
}

TEST_CASE("multiplex_iq rejects invalid sample values") {
  std::array<int8_t, 2046> i_samples{};
  std::array<int8_t, 10230> q_samples{};
  std::array<int16_t, 20460> out{};
  i_samples.fill(1);
  q_samples.fill(1);
  i_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kInvalidISample);
  i_samples[0] = 1;
  q_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kInvalidQSample);
}
