#include <chrono>
#include <iostream>
#include <vector>
#include <array>
#include <numeric>
#include "lunalink/signal/ldpc.hpp"
#include "lunalink/signal/bch.hpp"
#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("Gateway 2: WCET Analysis") {
    std::array<uint8_t, 1200> msg_sf2{};
    std::array<uint8_t, 2400> encoded_sf2{};
    std::array<float, 2400> soft_sf2{};
    std::array<uint8_t, 1200> decoded_sf2{};

    LdpcEncodeWorkspace enc_ws{};

    // 1. LDPC Encoding WCET
    auto start = std::chrono::steady_clock::now();
    for(int i=0; i<100; ++i) {
        if (ldpc_encode(LdpcSubframe::kSF2, msg_sf2, encoded_sf2, enc_ws) != LdpcStatus::kOk) {
            FAIL("LDPC encode failed in performance test");
        }
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double avg_enc = static_cast<double>(duration) / 100.0;

    std::cout << "[WCET] LDPC Encode (SF2): " << avg_enc << " ms (Benchmark: <100ms)" << std::endl;
    CHECK(avg_enc < 100.0);

    // 2. LDPC Decoding WCET (50 iterations)
    soft_sf2.fill(1.0f);
    LdpcWorkspace ws{};
    start = std::chrono::steady_clock::now();
    if (ldpc_decode(LdpcSubframe::kSF2, soft_sf2, 50, decoded_sf2, ws) != LdpcStatus::kOk) {
        FAIL("LDPC decode failed in performance test");
    }
    end = std::chrono::steady_clock::now();
    auto dec_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "[WCET] LDPC Decode (SF2, 50 iter): " << dec_duration << " ms (Benchmark: <1000ms)" << std::endl;
    CHECK(dec_duration < 1000);

    // 3. BCH ML Decoding WCET
    std::array<uint8_t, 52> bch_in{};
    start = std::chrono::steady_clock::now();
    for(int i=0; i<1000; ++i) {
        const auto res = bch_decode(bch_in);
        (void)res; // suppress unused warning
    }
    end = std::chrono::steady_clock::now();
    auto bch_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_bch = static_cast<double>(bch_duration) / 1000.0;

    std::cout << "[WCET] BCH ML Decode: " << avg_bch << " us" << std::endl;
}

TEST_CASE("Gateway 2: LDPC Decode Throughput") {
    constexpr int kRuns = 20;

    SECTION("SF2 — alpha sweep throughput") {
        std::array<float, 2400> soft{};
        prepare_soft_symbols(LdpcSubframe::kSF2, soft);

        struct AlphaConfig { int16_t num; int16_t den; const char* label; };
        AlphaConfig configs[] = {
            {1, 2, "1/2 (0.50)"},
            {5, 8, "5/8 (0.625)"},
            {3, 4, "3/4 (0.75)"},
            {7, 8, "7/8 (0.875)"},
        };

        for (const auto& cfg : configs) {
            std::array<uint8_t, 1200> decoded{};
            LdpcWorkspace ws{};
            ws.alpha_numerator = cfg.num;
            ws.alpha_denominator = cfg.den;

            // Warmup
            (void)ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws);

            auto start = std::chrono::steady_clock::now();
            for (int r = 0; r < kRuns; ++r)
                (void)ldpc_decode(LdpcSubframe::kSF2, soft, 50, decoded, ws);
            auto end = std::chrono::steady_clock::now();

            auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            double avg_us = static_cast<double>(us) / kRuns;
            std::cout << "[Throughput] SF2 decode alpha=" << cfg.label
                      << ": " << avg_us << " us/decode (50 iter)" << std::endl;
            CHECK(avg_us < 50000.0); // 50ms ceiling
        }
    }

    SECTION("SF3 — baseline throughput") {
        std::array<float, 1740> soft{};
        prepare_soft_symbols(LdpcSubframe::kSF3, soft);

        std::array<uint8_t, 870> decoded{};
        LdpcWorkspace ws{};

        // Warmup
        (void)ldpc_decode(LdpcSubframe::kSF3, soft, 50, decoded, ws);

        auto start = std::chrono::steady_clock::now();
        for (int r = 0; r < kRuns; ++r)
            (void)ldpc_decode(LdpcSubframe::kSF3, soft, 50, decoded, ws);
        auto end = std::chrono::steady_clock::now();

        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double avg_us = static_cast<double>(us) / kRuns;
        std::cout << "[Throughput] SF3 decode alpha=3/4: " << avg_us
                  << " us/decode (50 iter)" << std::endl;
        CHECK(avg_us < 50000.0);
    }
}
