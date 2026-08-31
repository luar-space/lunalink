// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_swifi_core.cpp
 * @brief SWIFI tests for core subsystem: SISE CFI sentinel skip and scrubbing.
 *
 * Compiled with LUNALINK_FAULT_INJECTION=1.
 *
 * FI-37: compute_sise CFI milestone skip — inject fault between milestones.
 * FI-38: compute_sise_percentile CFI milestone skip — inject fault between milestones.
 */
#include "lunalink/core/sise.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <span>

using namespace lunalink::core;

// ---------------------------------------------------------------------------
// FI-37: compute_sise — CFI milestone skip
// ---------------------------------------------------------------------------

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise NaN injection — kInvalidInput, output scrubbed",
          "[swifi][core]") {
  // Inject NaN into one position component to force the input validation
  // to reject.  This exercises the scrubbing path (lines 136-139 of sise.cpp).
  const SiseVec pos_true{1.0, 2.0, 3.0, 0.0};
  const SiseVec pos_bad{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0};
  const SiseVec vel_true{0.1, 0.2, 0.3, 0.0};
  const SiseVec vel_broadcast{0.0, 0.0, 0.0, 0.0};

  const auto result = compute_sise(pos_true, pos_bad, vel_true, vel_broadcast);

  REQUIRE(result.status == SiseStatus::kInvalidInput);
  CHECK(result.position_error_m == 0.0);
  CHECK(result.velocity_error_mps == 0.0);
}

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise Inf injection — kInvalidInput, output scrubbed",
          "[swifi][core]") {
  const SiseVec pos_true{0.0, 0.0, 0.0, 0.0};
  const SiseVec pos_inf{0.0, std::numeric_limits<double>::infinity(), 0.0, 0.0};
  const SiseVec vel_z{};
  const auto result = compute_sise(pos_true, pos_inf, vel_z, vel_z);

  REQUIRE(result.status == SiseStatus::kInvalidInput);
  CHECK(result.position_error_m == 0.0);
  CHECK(result.velocity_error_mps == 0.0);
}

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise clock term injection — "
          "NaN in ct scrubbed",
          "[swifi][core]") {
  // Inject NaN specifically in the clock component (4th dimension).
  const SiseVec pos_ok{1.0, 2.0, 3.0, 0.0};
  const SiseVec pos_bad_ct{1.0, 2.0, 3.0, std::numeric_limits<double>::quiet_NaN()};
  const SiseVec vel_z{};
  const auto result = compute_sise(pos_ok, pos_bad_ct, vel_z, vel_z);

  REQUIRE(result.status == SiseStatus::kInvalidInput);
  CHECK(result.position_error_m == 0.0);
}

// ---------------------------------------------------------------------------
// FI-38: compute_sise_percentile — CFI milestone skip / input rejection
// ---------------------------------------------------------------------------

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise_percentile mismatched spans — "
          "kInvalidInput, output scrubbed",
          "[swifi][core]") {
  std::array<double, 3> pos = {1.0, 2.0, 3.0};
  const auto result = compute_sise_percentile(std::span(pos), std::span<double>(), 0.95);

  REQUIRE(result.status == SiseStatus::kInvalidInput);
  CHECK(result.position_percentile_m == 0.0);
  CHECK(result.velocity_percentile_mps == 0.0);
  CHECK_FALSE(result.position_pass);
  CHECK_FALSE(result.velocity_pass);
}

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise_percentile NaN in data — "
          "kInvalidInput, output scrubbed",
          "[swifi][core]") {
  std::array<double, 3> pos = {1.0, std::numeric_limits<double>::quiet_NaN(), 3.0};
  std::array<double, 3> vel = {0.001, 0.002, 0.003};
  const auto result = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);

  REQUIRE(result.status == SiseStatus::kInvalidInput);
  CHECK(result.position_percentile_m == 0.0);
  CHECK(result.velocity_percentile_mps == 0.0);
  CHECK_FALSE(result.position_pass);
  CHECK_FALSE(result.velocity_pass);
}

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise_percentile empty data — "
          "kEmptyData, output scrubbed",
          "[swifi][core]") {
  const auto result = compute_sise_percentile(std::span<double>(), std::span<double>(), 0.95);

  REQUIRE(result.status == SiseStatus::kEmptyData);
  CHECK(result.position_percentile_m == 0.0);
  CHECK(result.velocity_percentile_mps == 0.0);
  CHECK_FALSE(result.position_pass);
  CHECK_FALSE(result.velocity_pass);
}

TEST_CASE("[LSIS-SISE] SWIFI: compute_sise_percentile invalid percentile — "
          "kInvalidInput, output scrubbed",
          "[swifi][core]") {
  std::array<double, 3> pos = {1.0, 2.0, 3.0};
  std::array<double, 3> vel = {0.001, 0.002, 0.003};

  // Percentile > 1.0
  auto result = compute_sise_percentile(std::span(pos), std::span(vel), 1.5);
  REQUIRE(result.status == SiseStatus::kInvalidInput);

  // Percentile < 0.0
  result = compute_sise_percentile(std::span(pos), std::span(vel), -0.1);
  REQUIRE(result.status == SiseStatus::kInvalidInput);

  // Percentile = NaN
  result = compute_sise_percentile(
      std::span(pos), std::span(vel), std::numeric_limits<double>::quiet_NaN());
  REQUIRE(result.status == SiseStatus::kInvalidInput);
}
