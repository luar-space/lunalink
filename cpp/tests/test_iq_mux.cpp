#include "lunalink/signal/iq_mux.hpp"
#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/tiered_code.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

// --- C2-Q: BPSK(5) pilot modulator ---

TEST_CASE("modulate_bpsk_q chip mapping matches Table 8") {
  const std::array<uint8_t, 4> chips = {{0, 1, 0, 1}};
  std::array<int8_t, 4> out{};
  REQUIRE(modulate_bpsk_q(chips.data(), 4, out.data()) ==
          ModulationStatus::kOk);
  REQUIRE(out[0] == 1);
  REQUIRE(out[1] == -1);
  REQUIRE(out[2] == 1);
  REQUIRE(out[3] == -1);
}

TEST_CASE("modulate_bpsk_q equals modulate_bpsk_i with data_symbol +1") {
  // Q modulation (pilot, no data) should equal I modulation with symbol=+1
  const uint8_t *packed = nullptr;
  REQUIRE(weil10230_prn_packed(1, &packed) == PrnStatus::kOk);
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(packed, i, kWeil10230ChipLength, &chip) ==
            PrnStatus::kOk);
    chips[i] = chip;
  }

  std::array<int8_t, kWeil10230ChipLength> out_q{};
  std::array<int8_t, kWeil10230ChipLength> out_i{};
  REQUIRE(modulate_bpsk_q(chips.data(), kWeil10230ChipLength, out_q.data()) ==
          ModulationStatus::kOk);
  REQUIRE(modulate_bpsk_i(chips.data(), kWeil10230ChipLength, 1, out_i.data()) ==
          ModulationStatus::kOk);
  REQUIRE(out_q == out_i);
}

TEST_CASE("modulate_bpsk_q output values are {-1, +1}") {
  const uint8_t *packed = nullptr;
  REQUIRE(weil10230_prn_packed(1, &packed) == PrnStatus::kOk);
  std::array<uint8_t, kWeil10230ChipLength> chips{};
  for (uint16_t i = 0; i < kWeil10230ChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(packed, i, kWeil10230ChipLength, &chip) ==
            PrnStatus::kOk);
    chips[i] = chip;
  }

  std::array<int8_t, kWeil10230ChipLength> out{};
  REQUIRE(modulate_bpsk_q(chips.data(), kWeil10230ChipLength, out.data()) ==
          ModulationStatus::kOk);
  for (auto v : out) {
    REQUIRE((v == -1 || v == 1));
  }
}

// --- C4: IQ multiplexer ---

TEST_CASE("multiplex_iq rejects null inputs") {
  std::array<int8_t, kGoldChipLength> i_buf{};
  std::array<int8_t, kWeil10230ChipLength> q_buf{};
  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};

  REQUIRE(multiplex_iq(nullptr, q_buf.data(), out.data()) ==
          IqMuxStatus::kNullInput);
  REQUIRE(multiplex_iq(i_buf.data(), nullptr, out.data()) ==
          IqMuxStatus::kNullInput);
  REQUIRE(multiplex_iq(i_buf.data(), q_buf.data(), nullptr) ==
          IqMuxStatus::kNullInput);
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
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kOk);

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
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kOk);

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
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kOk);

  // Every I sample should be +1, every Q sample should be -1
  for (uint16_t n = 0; n < kIqSamplesPerEpoch; ++n) {
    REQUIRE(out[2U * n] == 1);       // I
    REQUIRE(out[2U * n + 1U] == -1); // Q
  }
}

TEST_CASE("multiplex_iq 50/50 power: I and Q have equal amplitude") {
  // With real PRN data, verify both channels have the same RMS power
  const uint8_t *i_packed = nullptr;
  REQUIRE(gold_prn_packed(1, &i_packed) == PrnStatus::kOk);
  std::array<uint8_t, kGoldChipLength> i_chips{};
  for (uint16_t i = 0; i < kGoldChipLength; ++i) {
    uint8_t chip = 0;
    REQUIRE(unpack_chip(i_packed, i, kGoldChipLength, &chip) == PrnStatus::kOk);
    i_chips[i] = chip;
  }
  std::array<int8_t, kGoldChipLength> i_samples{};
  REQUIRE(modulate_bpsk_i(i_chips.data(), kGoldChipLength, 1, i_samples.data()) ==
          ModulationStatus::kOk);

  std::array<uint8_t, kWeil10230ChipLength> q_chips{};
  REQUIRE(tiered_code_epoch(1, 0, q_chips.data()) == TieredCodeStatus::kOk);
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  REQUIRE(modulate_bpsk_q(q_chips.data(), kWeil10230ChipLength, q_samples.data()) ==
          ModulationStatus::kOk);

  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kOk);

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

TEST_CASE("modulate_bpsk_q returns explicit error status") {
  std::array<uint8_t, 4> chips = {{0, 1, 0, 1}};
  std::array<int8_t, 4> out = {{9, 9, 9, 9}};
  REQUIRE(modulate_bpsk_q(nullptr, 4, out.data()) == ModulationStatus::kNullInput);
  REQUIRE(modulate_bpsk_q(chips.data(), 4, nullptr) == ModulationStatus::kNullInput);
}

TEST_CASE("modulate_bpsk_q rejects non-binary chips") {
  std::array<uint8_t, 4> chips = {{0, 1, 2, 1}};
  std::array<int8_t, 4> out = {{9, 9, 9, 9}};
  REQUIRE(modulate_bpsk_q(chips.data(), 4, out.data()) ==
          ModulationStatus::kInvalidChipValue);
}

TEST_CASE("multiplex_iq rejects invalid sample values") {
  std::array<int8_t, kGoldChipLength> i_samples{};
  std::array<int8_t, kWeil10230ChipLength> q_samples{};
  std::array<int16_t, 2 * kIqSamplesPerEpoch> out{};
  i_samples.fill(1);
  q_samples.fill(1);
  i_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kInvalidISample);
  i_samples[0] = 1;
  q_samples[0] = 0;
  REQUIRE(multiplex_iq(i_samples.data(), q_samples.data(), out.data()) ==
          IqMuxStatus::kInvalidQSample);
}
