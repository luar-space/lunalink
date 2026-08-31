// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
/// @file test_sise.cpp
/// @brief Tests for SISE (Signal-In-Space Error) per LSIS V1.0 §2.2.2. [LSIS-SISE]

#include "lunalink/core/sise.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <span>
#include <vector>

using namespace lunalink::core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ─── compute_sise (4-D: x, y, z, ct) ───────────────────────────────────────

TEST_CASE("[LSIS-SISE][core] compute_sise zero error", "[core]") {
  const SiseVec pos{1000.0, 2000.0, 3000.0, 0.0};
  const SiseVec vel{1.0, 2.0, 3.0, 0.0};
  const auto r = compute_sise(pos, pos, vel, vel);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_error_m, WithinAbs(0.0, 1e-15));
  REQUIRE_THAT(r.velocity_error_mps, WithinAbs(0.0, 1e-15));
}

TEST_CASE("[LSIS-SISE][core] compute_sise known spatial error (3-4-5)", "[core]") {
  // 3-4-5 triangle in position, zero clock bias.
  const SiseVec pos_true{100.0, 200.0, 300.0, 0.0};
  const SiseVec pos_bcast{103.0, 204.0, 300.0, 0.0}; // dx=3, dy=4, dz=0 → RSS=5
  const SiseVec vel_true{10.0, 20.0, 30.0, 0.0};
  const SiseVec vel_bcast{11.0, 21.0, 31.0, 0.0}; // dv=1 each → RSS=sqrt(3)

  const auto r = compute_sise(pos_true, pos_bcast, vel_true, vel_bcast);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_error_m, WithinAbs(5.0, 1e-12));
  REQUIRE_THAT(r.velocity_error_mps, WithinAbs(std::sqrt(3.0), 1e-12));
}

TEST_CASE("[LSIS-SISE][core] compute_sise with clock bias (4th component)", "[core]") {
  // Pure clock error: 10 ns bias → ct = 10e-9 * c ≈ 2.998 m
  const double clock_bias_m = 10e-9 * kSpeedOfLightMps; // ~2.998 m
  const SiseVec pos_true{0.0, 0.0, 0.0, 0.0};
  const SiseVec pos_bcast{0.0, 0.0, 0.0, clock_bias_m};
  const SiseVec vel{0.0, 0.0, 0.0, 0.0};

  const auto r = compute_sise(pos_true, pos_bcast, vel, vel);
  REQUIRE(r.status == SiseStatus::kOk);
  // SISE_pos = sqrt(0 + 0 + 0 + (ct)²) = |ct| ≈ 2.998 m
  REQUIRE_THAT(r.position_error_m, WithinRel(clock_bias_m, 1e-10));
  REQUIRE_THAT(r.velocity_error_mps, WithinAbs(0.0, 1e-15));
}

TEST_CASE("[LSIS-SISE][core] compute_sise combined spatial + clock", "[core]") {
  // 3 m spatial + 4 m clock → RSS = 5 m
  const SiseVec pos_true{0.0, 0.0, 0.0, 0.0};
  const SiseVec pos_bcast{3.0, 0.0, 0.0, 4.0};
  const SiseVec vel{0.0, 0.0, 0.0, 0.0};

  const auto r = compute_sise(pos_true, pos_bcast, vel, vel);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_error_m, WithinAbs(5.0, 1e-12));
}

TEST_CASE("[LSIS-SISE][core] compute_sise clock drift in velocity", "[core]") {
  // 1 ns/s clock drift → ct_dot = 1e-9 * c ≈ 0.2998 m/s
  const double clock_drift_mps = 1e-9 * kSpeedOfLightMps;
  const SiseVec pos{0.0, 0.0, 0.0, 0.0};
  const SiseVec vel_true{0.0, 0.0, 0.0, 0.0};
  const SiseVec vel_bcast{0.0, 0.0, 0.0, clock_drift_mps};

  const auto r = compute_sise(pos, pos, vel_true, vel_bcast);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.velocity_error_mps, WithinRel(clock_drift_mps, 1e-10));
}

TEST_CASE("[LSIS-SISE][core] compute_sise position only", "[core]") {
  const SiseVec pos_true{0.0, 0.0, 0.0, 0.0};
  const SiseVec pos_bcast{40.0, 0.0, 0.0, 0.0}; // 40 m spatial error
  const SiseVec vel{0.0, 0.0, 0.0, 0.0};

  const auto r = compute_sise(pos_true, pos_bcast, vel, vel);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_error_m, WithinAbs(40.0, 1e-12));
  REQUIRE_THAT(r.velocity_error_mps, WithinAbs(0.0, 1e-15));
}

TEST_CASE("[LSIS-SISE][core] compute_sise NaN rejection", "[core]") {
  const SiseVec pos{1.0, 2.0, 3.0, 0.0};
  const SiseVec vel{0.0, 0.0, 0.0, 0.0};
  const SiseVec bad_x{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0};
  const SiseVec bad_ct{0.0, 0.0, 0.0, std::numeric_limits<double>::quiet_NaN()};

  SECTION("NaN in spatial component") {
    const auto r = compute_sise(bad_x, pos, vel, vel);
    REQUIRE(r.status == SiseStatus::kInvalidInput);
    REQUIRE(r.position_error_m == 0.0); // Scrubbed
  }
  SECTION("NaN in clock component") {
    const auto r = compute_sise(bad_ct, pos, vel, vel);
    REQUIRE(r.status == SiseStatus::kInvalidInput);
  }
  SECTION("NaN velocity broadcast") {
    const auto r = compute_sise(pos, pos, vel, bad_x);
    REQUIRE(r.status == SiseStatus::kInvalidInput);
  }
}

TEST_CASE("[LSIS-SISE][core] compute_sise Inf rejection", "[core]") {
  const SiseVec pos{1.0, 2.0, 3.0, 0.0};
  const SiseVec vel{0.0, 0.0, 0.0, 0.0};
  const SiseVec bad{std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0};

  const auto r = compute_sise(bad, pos, vel, vel);
  REQUIRE(r.status == SiseStatus::kInvalidInput);
  REQUIRE(r.position_error_m == 0.0); // Scrubbed
  REQUIRE(r.velocity_error_mps == 0.0);
}

TEST_CASE("[LSIS-SISE][core] compute_sise large values", "[core]") {
  // Lunar orbital radius ~400,000 km = 4e8 m
  const SiseVec pos_true{4.0e8, 0.0, 0.0, 0.0};
  const SiseVec pos_bcast{4.0e8 + 10.0, 0.0, 0.0, 0.0};
  const SiseVec vel{1000.0, 0.0, 0.0, 0.0};

  const auto r = compute_sise(pos_true, pos_bcast, vel, vel);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_error_m, WithinAbs(10.0, 1e-6));
}

// ─── compute_sise_percentile ────────────────────────────────────────────────

TEST_CASE("[LSIS-SISE][core] percentile sorted data", "[core]") {
  // 100 samples: errors 1..100. 95th percentile = ceil(0.95*100)-1 = 94 → value 95.
  std::vector<double> pos(100);
  std::vector<double> vel(100);
  for (size_t i = 0; i < 100; ++i) {
    pos[i] = static_cast<double>(i + 1);        // 1..100 m
    vel[i] = static_cast<double>(i + 1) * 1e-4; // 0.0001..0.01 m/s
  }

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_percentile_m, WithinAbs(95.0, 1e-12));
  REQUIRE_THAT(r.velocity_percentile_mps, WithinAbs(0.0095, 1e-12));
  REQUIRE_FALSE(r.position_pass); // 95 > 40
  REQUIRE(r.velocity_pass);       // 0.0095 <= 0.01
}

TEST_CASE("[LSIS-SISE][core] percentile all pass", "[core]") {
  std::vector<double> pos(20, 5.0);   // 5 m (< 40 m)
  std::vector<double> vel(20, 0.001); // 0.001 m/s (< 0.01 m/s)

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE(r.position_pass);
  REQUIRE(r.velocity_pass);
}

TEST_CASE("[LSIS-SISE][core] percentile boundary at limit", "[core]") {
  std::vector<double> pos(10, 40.0);
  std::vector<double> vel(10, 0.01);

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE(r.position_pass); // 40.0 <= 40.0
  REQUIRE(r.velocity_pass); // 0.01 <= 0.01
}

TEST_CASE("[LSIS-SISE][core] percentile empty data", "[core]") {
  const auto r = compute_sise_percentile(std::span<double>(), std::span<double>(), 0.95);
  REQUIRE(r.status == SiseStatus::kEmptyData);
}

TEST_CASE("[LSIS-SISE][core] percentile mismatched spans", "[core]") {
  double pos = 1.0;
  const auto r = compute_sise_percentile(std::span<double>(&pos, 1), std::span<double>(), 0.95);
  REQUIRE(r.status == SiseStatus::kInvalidInput);
}

TEST_CASE("[LSIS-SISE][core] percentile invalid percentile value", "[core]") {
  double pos = 1.0;
  double vel = 0.001;

  SECTION("percentile > 1") {
    const auto r =
        compute_sise_percentile(std::span<double>(&pos, 1), std::span<double>(&vel, 1), 1.5);
    REQUIRE(r.status == SiseStatus::kInvalidInput);
  }
  SECTION("percentile < 0") {
    const auto r =
        compute_sise_percentile(std::span<double>(&pos, 1), std::span<double>(&vel, 1), -0.1);
    REQUIRE(r.status == SiseStatus::kInvalidInput);
  }
  SECTION("percentile NaN") {
    const auto r = compute_sise_percentile(std::span<double>(&pos, 1),
                                           std::span<double>(&vel, 1),
                                           std::numeric_limits<double>::quiet_NaN());
    REQUIRE(r.status == SiseStatus::kInvalidInput);
  }
}

TEST_CASE("[LSIS-SISE][core] percentile NaN in data", "[core]") {
  std::vector<double> pos = {1.0, std::numeric_limits<double>::quiet_NaN(), 3.0};
  std::vector<double> vel = {0.001, 0.002, 0.003};

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);
  REQUIRE(r.status == SiseStatus::kInvalidInput);
}

TEST_CASE("[LSIS-SISE][core] percentile single sample", "[core]") {
  double pos = 25.0;
  double vel = 0.005;

  const auto r =
      compute_sise_percentile(std::span<double>(&pos, 1), std::span<double>(&vel, 1), 0.95);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_percentile_m, WithinAbs(25.0, 1e-12));
  REQUIRE_THAT(r.velocity_percentile_mps, WithinAbs(0.005, 1e-12));
  REQUIRE(r.position_pass);
  REQUIRE(r.velocity_pass);
}

TEST_CASE("[LSIS-SISE][core] percentile large dataset", "[core]") {
  constexpr size_t kN = 1000;
  std::vector<double> pos(kN);
  std::vector<double> vel(kN);
  for (size_t i = 0; i < kN; ++i) {
    pos[i] = 0.04 + static_cast<double>(i) * (40.0 - 0.04) / static_cast<double>(kN - 1);
    vel[i] = 1e-5 + static_cast<double>(i) * (0.01 - 1e-5) / static_cast<double>(kN - 1);
  }

  // Save expected values before the call — spans are mutated in-place.
  const double expected_pos = pos[949];
  const double expected_vel = vel[949];

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.95);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_percentile_m, WithinRel(expected_pos, 1e-10));
  REQUIRE_THAT(r.velocity_percentile_mps, WithinRel(expected_vel, 1e-10));
}

TEST_CASE("[LSIS-SISE][core] percentile 50th (median)", "[core]") {
  std::vector<double> pos = {10.0, 20.0, 30.0, 40.0, 50.0};
  std::vector<double> vel = {0.001, 0.002, 0.003, 0.004, 0.005};

  const auto r = compute_sise_percentile(std::span(pos), std::span(vel), 0.5);
  REQUIRE(r.status == SiseStatus::kOk);
  REQUIRE_THAT(r.position_percentile_m, WithinAbs(30.0, 1e-12));
}

// ─── Realistic trajectory ───────────────────────────────────────────────────

TEST_CASE("[LSIS-SISE][core] 24-hour SISE trajectory with age-of-data growth", "[core]") {
  // Simulate broadcast ephemeris degradation over 24 hours.
  // Model: SISE grows as sqrt(a² + b²·t²) where t is age-of-data in hours.
  // Typical lunar SISE: 1 m at epoch → 6-8 m at 6 h → 20 m at 10 h.
  //
  // Parameters: a = 1.0 m (initial accuracy), b = 2.0 m/h (drift rate).
  // SISE(t) = sqrt(1 + 4·t²):
  //   t=0h  → 1.0 m,  t=3h  → 6.1 m,  t=6h  → 12.0 m,
  //   t=10h → 20.0 m, t=12h → 24.0 m

  constexpr size_t kEpochs = 240; // One sample per 6 min over 24 h
  constexpr double kA = 1.0;      // Initial SISE (m)
  constexpr double kB = 2.0;      // Drift rate (m/h)

  SECTION("24h no refresh — exceeds 40m limit") {
    std::vector<double> pos_errors(kEpochs);
    std::vector<double> vel_errors(kEpochs);
    for (size_t i = 0; i < kEpochs; ++i) {
      const double t_hours = static_cast<double>(i) * 24.0 / static_cast<double>(kEpochs);
      pos_errors[i] = std::sqrt(kA * kA + kB * kB * t_hours * t_hours);
      vel_errors[i] = pos_errors[i] * 1e-4;
    }

    const auto r = compute_sise_percentile(std::span(pos_errors), std::span(vel_errors));
    REQUIRE(r.status == SiseStatus::kOk);
    CHECK_FALSE(r.position_pass);
    CHECK(r.velocity_pass);
    CHECK(r.position_percentile_m > 40.0);
  }

  SECTION("10h refresh cycle — within 40m limit") {
    std::vector<double> pos_errors(kEpochs);
    std::vector<double> vel_errors(kEpochs);
    for (size_t i = 0; i < kEpochs; ++i) {
      const double t_hours = static_cast<double>(i) * 24.0 / static_cast<double>(kEpochs);
      const double age = std::fmod(t_hours, 10.0);
      pos_errors[i] = std::sqrt(kA * kA + kB * kB * age * age);
      vel_errors[i] = pos_errors[i] * 1e-4;
    }

    const auto r = compute_sise_percentile(std::span(pos_errors), std::span(vel_errors));
    REQUIRE(r.status == SiseStatus::kOk);
    CHECK(r.position_pass);
    CHECK(r.velocity_pass);
    CHECK(r.position_percentile_m < 40.0);
  }
}
