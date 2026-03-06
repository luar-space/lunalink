#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/tiered_code.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// --- C2-Q: BPSK(5) pilot modulator ---

TEST_CASE("modulate_bpsk_q chip mapping matches Table 8") {
  // Test 0->+1, 1->-1 mapping explicitly
  std::array<uint8_t, 2> chips = {0, 1};
  std::array<int8_t, 2> out{};
  REQUIRE(modulate_bpsk_any(chips, 1, out) == ModulationStatus::kOk);
  REQUIRE(out[0] == 1);
  REQUIRE(out[1] == -1);
}

TEST_CASE("modulate_bpsk_q equals modulate_bpsk_i with data_symbol +1") {
  // Q modulation (pilot, no data) should equal I modulation with symbol=+1
  PrnCode packed;
  REQUIRE(weil10230_prn_packed(PrnId{1}, packed) == PrnStatus::kOk);
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(packed, i, &chip) ==
            PrnStatus::kOk);
    chips[i] = chip;
  }

  std::array<int8_t, kWeil10230ChipLength> out_q{};
  std::array<int8_t, kWeil10230ChipLength> out_i{};
  REQUIRE(modulate_bpsk_q(chips, out_q) == ModulationStatus::kOk);
  REQUIRE(modulate_bpsk_any(chips, 1, out_i) == ModulationStatus::kOk);
  REQUIRE(out_q == out_i);
}

TEST_CASE("modulate_bpsk_q output values are {-1, +1}") {
  PrnCode packed;
  REQUIRE(weil10230_prn_packed(PrnId{1}, packed) == PrnStatus::kOk);
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(packed, i, &chip) ==
            PrnStatus::kOk);
    chips[i] = chip;
  }

  std::array<int8_t, kWeil10230ChipLength> out{};
  REQUIRE(modulate_bpsk_q(chips, out) == ModulationStatus::kOk);
  for (auto v : out) {
    REQUIRE((v == -1 || v == 1));
  }
}

// --- C4: IQ multiplexer ---

TEST_CASE("multiplex_iq rejects undersized inputs and outputs") {
  std::array<int8_t, kGoldChipLength - 1> i_small{};
  std::array<int8_t, kWeil10230ChipLength> q_ok{};
  std::array<int16_t, 2 * kIqSamplesPerEpoch> out_ok{};
  
  // Input I too small
  REQUIRE(multiplex_iq(i_small, q_ok, out_ok) == IqMuxStatus::kInputTooSmall);
  
  std::array<int8_t, kGoldChipLength> i_ok{};
  std::array<int16_t, 2 * kIqSamplesPerEpoch - 1> out_small{};

  // Output too small
  REQUIRE(multiplex_iq(i_ok, q_ok, out_small) == IqMuxStatus::kOutputTooSmall);
}

TEST_CASE("multiplex_iq upsamples I by factor 5") {
  // Create simple I pattern: alternating +1, -1 for first few chips
  std::array<int8_t, kGoldChipLength> i_samples{};
  i_samples.fill(1);
  i_samples[0] = 1;
  i_samples[1] = -1;
  i_samples[2] = 1;

  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  q_samples.fill(1);

  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // I is upsampled 5x: sample n uses i_samples[n/5]
  // Interleaved: out[2n] = I, out[2n+1] = Q
  for (size_t n = 0; n < 5; ++n) {
    REQUIRE(out[2U * n] == 1);      // I chip 0 repeated 5x
  }
  for (size_t n = 5; n < 10; ++n) {
    REQUIRE(out[2U * n] == -1);     // I chip 1 repeated 5x
  }
  for (size_t n = 10; n < 15; ++n) {
    REQUIRE(out[2U * n] == 1);      // I chip 2 repeated 5x
  }
}

TEST_CASE("multiplex_iq Q channel passes through at chip rate") {
  std::array<int8_t, kGoldChipLength> i_samples{};
  i_samples.fill(1);

  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  q_samples.fill(1);
  q_samples[0] = 1;
  q_samples[1] = -1;
  q_samples[2] = 1;

  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // Q is at native rate: out[2n+1] = q_samples[n]
  REQUIRE(out[1] == 1);
  REQUIRE(out[3] == -1);
  REQUIRE(out[5] == 1);
}

TEST_CASE("multiplex_iq output length is 2 * 10230 interleaved") {
  // Verify the interleaving structure with a full epoch
  std::array<int8_t, kGoldChipLength> i_samples{};
  i_samples.fill(1);

  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  q_samples.fill(-1);

  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // Every I sample should be +1, every Q sample should be -1
  for (uint16_t n = 0; n < kIqSamplesPerEpoch; ++n) {
    REQUIRE(out[2U * n] == 1);       // I
    REQUIRE(out[2U * n + 1U] == -1); // Q
  }
}

TEST_CASE("multiplex_iq 50/50 power: I and Q have equal amplitude") {
  // With real PRN data, verify both channels have the same RMS power
  PrnCode i_packed;
  REQUIRE(gold_prn_packed(PrnId{1}, i_packed) == PrnStatus::kOk);
  std::array<uint8_t, kGoldChipLength> i_chips{};
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(i_packed, i, &chip) == PrnStatus::kOk);
    i_chips[i] = chip;
  }
  std::array<int8_t, kGoldChipLength> i_samples{};
  REQUIRE(modulate_bpsk_i(i_chips, 1, i_samples) == ModulationStatus::kOk);

  std::array<uint8_t, kWeil10230ChipLength> q_chips{};
  REQUIRE(tiered_code_epoch(PrnId{1}, 0, q_chips) == TieredCodeStatus::kOk);
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  REQUIRE(modulate_bpsk_q(q_chips, q_samples) == ModulationStatus::kOk);

  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kOk);

  // Both channels output {-1, +1} with equal amplitude → equal mean-square
  int64_t power_i = 0;
  int64_t power_q = 0;
  for (uint16_t n = 0; n < kIqSamplesPerEpoch; ++n) {
    const int64_t iv = out[2U * n];
    const int64_t qv = out[2U * n + 1U];
    power_i += iv * iv;
    power_q += qv * qv;
  }
  // Both are {-1, +1}^2 = 1 for every sample → power = kIqSamplesPerEpoch
  REQUIRE(power_i == kIqSamplesPerEpoch);
  REQUIRE(power_q == kIqSamplesPerEpoch);
}

TEST_CASE("kIqSamplesPerEpoch is 10230") {
  REQUIRE(kIqSamplesPerEpoch == 10230);
}

TEST_CASE("kIqUpsampleFactor is 5") {
  REQUIRE(kIqUpsampleFactor == 5);
}

TEST_CASE("modulate_bpsk_q returns explicit error status for undersized output") {
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  std::array<int8_t, kWeil10230ChipLength - 1> out_small{};
  REQUIRE(modulate_bpsk_q(chips, out_small) == ModulationStatus::kOutputTooSmall);
}

TEST_CASE("modulate_bpsk_q rejects non-binary chips") {
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  chips.fill(0);
  chips[10] = 2; // Invalid chip value
  std::array<int8_t, kWeil10230ChipLength> out{};
  REQUIRE(modulate_bpsk_q(chips, out) == ModulationStatus::kInvalidChipValue);
}

TEST_CASE("multiplex_iq rejects invalid sample values") {
  std::array<int8_t, kGoldChipLength> i_samples{};
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  i_samples.fill(1);
  q_samples.fill(1);
  i_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kInvalidISample);
  i_samples[0] = 1;
  q_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples, q_samples, out) == IqMuxStatus::kInvalidQSample);
}
