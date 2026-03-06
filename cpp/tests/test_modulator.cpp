#include "lunalink/signal/modulator.hpp"
#include "lunalink/signal/prn.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace lunalink::signal;

TEST_CASE("modulate_bpsk_i chip mapping with +1 symbol") {
  // Per spec section 2.3.3, Table 8: logic 0 -> +1, logic 1 -> -1
  const std::array<uint8_t, 4> chips = {{0, 1, 0, 1}};
  std::array<int8_t, 4> out{};
  modulate_bpsk_i(chips.data(), 4, 1, out.data());
  REQUIRE(out[0] == 1);
  REQUIRE(out[1] == -1);
  REQUIRE(out[2] == 1);
  REQUIRE(out[3] == -1);
}

TEST_CASE("modulate_bpsk_i chip mapping with -1 symbol") {
  const std::array<uint8_t, 4> chips = {{0, 1, 0, 1}};
  std::array<int8_t, 4> out{};
  modulate_bpsk_i(chips.data(), 4, -1, out.data());
  REQUIRE(out[0] == -1);
  REQUIRE(out[1] == 1);
  REQUIRE(out[2] == -1);
  REQUIRE(out[3] == 1);
}

TEST_CASE("modulate_bpsk_i full PRN 1 all values in {-1, +1}") {
  // Unpack Gold PRN 1 chips from packed format
  const auto *packed = gold_prn_packed(1);
  std::array<uint8_t, kGoldChipLength> chips{};
  for (uint16_t i = 0; i < kGoldChipLength; ++i)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    chips[i] = unpack_chip(packed, i);

  std::array<int8_t, kGoldChipLength> out{};
  modulate_bpsk_i(chips.data(), kGoldChipLength, 1, out.data());
  for (auto v : out)
    REQUIRE((v == -1 || v == 1));
}
