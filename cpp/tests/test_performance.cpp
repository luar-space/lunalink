#include <array>
#include <numeric>
#include <vector>

#include "lunalink/signal/bch.hpp"
#include "lunalink/signal/ldpc.hpp"
#include "lunalink/signal/sync_detector.hpp"
#include "lunalink/signal/telemetry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

using namespace lunalink::signal;

namespace {
// Helper: encode a test pattern and produce soft symbols for benchmarking.
void prepare_soft_symbols(LdpcSubframe type,
                          std::span<float> soft,
                          float snr_scale = 1.0f) {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const size_t k = is_sf2 ? 1200U : 870U;
    const size_t n = is_sf2 ? 2400U : 1740U;

    std::vector<uint8_t> msg(k);
    std::iota(msg.begin(), msg.end(), uint8_t{0});
    for (auto& b : msg) b %= 2;

    std::vector<uint8_t> encoded(n);
    LdpcEncodeWorkspace enc_ws{};
    (void)ldpc_encode(type, msg, encoded, enc_ws);

    for (size_t i = 0; i < n && i < soft.size(); ++i)
        soft[i] = (encoded[i] == 1 ? -snr_scale : snr_scale);
}
} // namespace

TEST_CASE("Gateway 2: WCET Analysis", "[performance]") {
    std::array<uint8_t, 1200> msg_sf2{};
    std::array<uint8_t, 2400> encoded_sf2{};
    std::array<float, 2400> soft_sf2{};
    std::array<uint8_t, 1200> decoded_sf2{};

    LdpcEncodeWorkspace enc_ws{};

    BENCHMARK("LDPC encode SF2") {
        return ldpc_encode(LdpcSubframe::kSF2, msg_sf2, encoded_sf2, enc_ws);
    };

    soft_sf2.fill(1.0f);
    LdpcWorkspace ws{};

    BENCHMARK("LDPC decode SF2 (50 iter)") {
        return ldpc_decode(LdpcSubframe::kSF2, soft_sf2, 50, decoded_sf2, ws);
    };

    std::array<uint8_t, 52> bch_in{};

    BENCHMARK("BCH ML decode") {
        return bch_decode(bch_in);
    };
}

TEST_CASE("Gateway 5: Sync Detector WCET", "[performance]") {
    NullTelemetryHal hal;
    SyncDetector det(hal);

    // Build a 6000-symbol stream with sync at offset 0.
    std::array<float, 6000> symbols{};
    for (uint16_t i = 0; i < kSyncLength; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        symbols[i] = (kSyncPattern[i] == 0U) ? 1.0F : -1.0F;
    }

    SyncResult result{};

    BENCHMARK("sync_detect (6000 symbols)") {
        return det.detect(std::span<const float>(symbols), result);
    };
}

TEST_CASE("Gateway 2: LDPC Decode Throughput", "[performance]") {
    SECTION("SF2 — alpha sweep throughput") {
        std::array<float, 2400> soft{};
        prepare_soft_symbols(LdpcSubframe::kSF2, soft);

        struct AlphaConfig { int16_t num; int16_t den; const char* label; };
        AlphaConfig configs[] = {
            {3, 4, "3/4 (0.75)"},
        };

        for (const auto& cfg : configs) {
            std::array<uint8_t, 1200> decoded{};
            LdpcWorkspace ws{};
            ws.alpha_numerator = cfg.num;
            ws.alpha_denominator = cfg.den;

            BENCHMARK("LDPC decode SF2 alpha=0.75") {
                return ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws);
            };
        }
    }

    SECTION("SF3 — baseline throughput") {
        std::array<float, 1740> soft{};
        prepare_soft_symbols(LdpcSubframe::kSF3, soft);

        std::array<uint8_t, 870> decoded{};
        LdpcWorkspace ws{};

        BENCHMARK("LDPC decode SF3 alpha=0.75") {
            return ldpc_decode(LdpcSubframe::kSF3, soft, 50, decoded, ws);
        };
    }
}
