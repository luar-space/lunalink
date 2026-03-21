#include "lunalink/messages/time_of_transmission.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using namespace lunalink::signal;

// ═══ compute_tot ═════════════════════════════════════════════════════════════

TEST_CASE("[LSIS-AFS-810][messages] compute_tot all zeros", "[messages]") {
    const auto r = compute_tot(0, 0, 0, 0.0);
    REQUIRE(r.status == TotStatus::kOk);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot basic", "[messages]") {
    // WN=1, ITOW=2, TOI=5, delta=0
    // t_F = 1*604800 + 2*1200 + 5*12 = 604800 + 2400 + 60 = 607260
    const auto r = compute_tot(1, 2, 5, 0.0);
    REQUIRE(r.status == TotStatus::kOk);
    REQUIRE(r.t_f == 607260.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot with delta_t_lrt", "[messages]") {
    // WN=0, ITOW=0, TOI=0, delta=1.5
    const auto r = compute_tot(0, 0, 0, 1.5);
    REQUIRE(r.status == TotStatus::kOk);
    REQUIRE(r.t_f == 1.5);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot boundary values", "[messages]") {
    // WN=8191, ITOW=503, TOI=99, delta=0
    // t_F = 8191*604800 + 503*1200 + 99*12
    //     = 4953916800 + 603600 + 1188 = 4954521588
    const auto r = compute_tot(kWnMax, kItowMax, kTotToiMax, 0.0);
    REQUIRE(r.status == TotStatus::kOk);
    REQUIRE(r.t_f == 4954521588.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot negative delta", "[messages]") {
    const auto r = compute_tot(1, 0, 0, -0.5);
    REQUIRE(r.status == TotStatus::kOk);
    REQUIRE(r.t_f == 604799.5);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot individual components", "[messages]") {
    SECTION("WN only") {
        const auto r = compute_tot(1, 0, 0, 0.0);
        REQUIRE(r.status == TotStatus::kOk);
        REQUIRE(r.t_f == 604800.0);
    }
    SECTION("ITOW only") {
        const auto r = compute_tot(0, 1, 0, 0.0);
        REQUIRE(r.status == TotStatus::kOk);
        REQUIRE(r.t_f == 1200.0);
    }
    SECTION("TOI only") {
        const auto r = compute_tot(0, 0, 1, 0.0);
        REQUIRE(r.status == TotStatus::kOk);
        REQUIRE(r.t_f == 12.0);
    }
}

// ─── Invalid inputs ──────────────────────────────────────────────────────────

TEST_CASE("[LSIS-AFS-810][messages] compute_tot invalid WN", "[messages]") {
    const auto r = compute_tot(8192, 0, 0, 0.0);
    REQUIRE(r.status == TotStatus::kInvalidWn);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot invalid ITOW", "[messages]") {
    const auto r = compute_tot(0, 504, 0, 0.0);
    REQUIRE(r.status == TotStatus::kInvalidItow);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot invalid TOI", "[messages]") {
    const auto r = compute_tot(0, 0, 100, 0.0);
    REQUIRE(r.status == TotStatus::kInvalidToi);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot NaN delta rejected", "[messages]") {
    const auto r = compute_tot(0, 0, 0, std::numeric_limits<double>::quiet_NaN());
    REQUIRE(r.status == TotStatus::kInvalidDelta);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot +Inf delta rejected", "[messages]") {
    const auto r = compute_tot(0, 0, 0, std::numeric_limits<double>::infinity());
    REQUIRE(r.status == TotStatus::kInvalidDelta);
    REQUIRE(r.t_f == 0.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot -Inf delta rejected", "[messages]") {
    const auto r = compute_tot(0, 0, 0, -std::numeric_limits<double>::infinity());
    REQUIRE(r.status == TotStatus::kInvalidDelta);
    REQUIRE(r.t_f == 0.0);
}

// ─── Round-trip with BCH TOI values ──────────────────────────────────────────

TEST_CASE("[LSIS-AFS-810][messages] compute_tot TOI 0 and 99", "[messages]") {
    const auto r0 = compute_tot(0, 0, 0, 0.0);
    const auto r99 = compute_tot(0, 0, 99, 0.0);
    REQUIRE(r0.status == TotStatus::kOk);
    REQUIRE(r99.status == TotStatus::kOk);
    // Difference should be exactly 99 * 12 = 1188 seconds
    REQUIRE(r99.t_f - r0.t_f == 1188.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot ITOW 0 and 503", "[messages]") {
    const auto r0 = compute_tot(0, 0, 0, 0.0);
    const auto r503 = compute_tot(0, 503, 0, 0.0);
    REQUIRE(r0.status == TotStatus::kOk);
    REQUIRE(r503.status == TotStatus::kOk);
    // 503 block intervals × 1200 s = 603600 s
    REQUIRE(r503.t_f - r0.t_f == 603600.0);
}

TEST_CASE("[LSIS-AFS-810][messages] compute_tot one week consistency", "[messages]") {
    // 504 BIs per week: 504 × 1200 = 604800 = SECWEEK
    // ITOW wraps at 504 (0..503), meaning 504 intervals = 1 week
    // WN=0, ITOW=503, TOI=99 is the last frame of the last BI in a week
    const auto r_last = compute_tot(0, 503, 99, 0.0);
    // t_F = 503*1200 + 99*12 = 603600 + 1188 = 604788
    // Next frame would be WN=1, ITOW=0, TOI=0 = 604800
    const auto r_next = compute_tot(1, 0, 0, 0.0);
    REQUIRE(r_last.status == TotStatus::kOk);
    REQUIRE(r_next.status == TotStatus::kOk);
    // Gap should be 12 seconds (one frame)
    REQUIRE(r_next.t_f - r_last.t_f == 12.0);
}
