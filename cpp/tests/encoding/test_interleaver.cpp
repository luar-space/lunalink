#include "lunalink/encoding/interleaver.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <numeric>

using namespace lunalink::signal;

TEST_CASE("[LSIS-AFS-471] Block Interleaver: Round-trip correctness", "[interleaver]") {
    // std::iota wraps at 256: positions i and i+256 carry the same value,
    // hiding transpositions between those pairs.  Use XOR of low and high
    // index bytes instead: original[i] = (i&0xFF) ^ ((i>>8)&0xFF), which
    // gives different values at i and i+256 for all i in [0, 5879].
    std::array<uint8_t, kInterleaverSize> original{};
    for (size_t i = 0; i < kInterleaverSize; ++i) {
        original[i] = static_cast<uint8_t>((i & 0xFFU) ^ ((i >> 8U) & 0xFFU));
    }

    std::array<uint8_t, kInterleaverSize> interleaved{};
    std::array<uint8_t, kInterleaverSize> deinterleaved{};

    REQUIRE(block_interleave(original, interleaved) == InterleaverStatus::kOk);
    REQUIRE(block_deinterleave(interleaved, deinterleaved) == InterleaverStatus::kOk);

    CHECK(original == deinterleaved);

    // Spot-check the permutation direction (not just the round-trip identity):
    // in[r*98+c] must appear at out[c*60+r].
    CHECK(interleaved[0]  == original[0]);   // r=0,c=0 → out[0*60+0=0]
    CHECK(interleaved[60] == original[1]);   // r=0,c=1 → out[1*60+0=60]
    CHECK(interleaved[1]  == original[98]);  // r=1,c=0 → out[0*60+1=1]
    CHECK(interleaved[61] == original[99]);  // r=1,c=1 → out[1*60+1=61]
}

TEST_CASE("Block Interleaver: Specific mapping check", "[interleaver]") {
    // Write row-wise (98 cols), read column-wise (60 rows)
    // In:  [0, 1, ..., 97, 98, ..., 5879]
    // Out: [0, 98, 196, ..., 1, 99, 197, ...]
    
    std::array<uint8_t, kInterleaverSize> in{};
    std::array<uint8_t, kInterleaverSize> out{};
    
    // Fill with values that make indexing obvious
    for (size_t i = 0; i < kInterleaverSize; ++i) {
        in[i] = static_cast<uint8_t>(i % 256);
    }
    
    REQUIRE(block_interleave(in, out) == InterleaverStatus::kOk);
    
    // First column of 'array' was in[0], in[98], in[196], ...
    // Read column-wise means these appear first in 'out'.
    CHECK(out[0] == in[0]);
    CHECK(out[1] == in[98]);
    CHECK(out[2] == in[196]);
    
    // Second column starts at in[1], in[99], ...
    // It should appear after the first column (60 elements later)
    CHECK(out[60] == in[1]);
    CHECK(out[61] == in[99]);
}

TEST_CASE("Block Interleaver: Error handling", "[interleaver]") {
    std::array<uint8_t, kInterleaverSize> buf{};
    std::array<uint8_t, kInterleaverSize - 1> small_buf{};
    
    CHECK(block_interleave(small_buf, buf) == InterleaverStatus::kInputTooSmall);
    CHECK(block_interleave(buf, small_buf) == InterleaverStatus::kOutputTooSmall);
    
    // De-interleave
    CHECK(block_deinterleave(small_buf, buf) == InterleaverStatus::kInputTooSmall);
    CHECK(block_deinterleave(buf, small_buf) == InterleaverStatus::kOutputTooSmall);
}

TEST_CASE("deinterleave: aliasing detection (hard)", "[interleaver]") {
    std::array<uint8_t, kInterleaverSize> buf{};
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = static_cast<uint8_t>(i & 1);
    CHECK(block_deinterleave(buf, buf) == InterleaverStatus::kFaultDetected);
}

TEST_CASE("deinterleave_soft: aliasing detection", "[interleaver]") {
    std::array<float, kInterleaverSize> buf{};
    for (size_t i = 0; i < buf.size(); ++i) buf[i] = 1.0F;
    CHECK(block_deinterleave_soft(buf, buf) == InterleaverStatus::kFaultDetected);
}

TEST_CASE("deinterleave: input/output size checks", "[interleaver]") {
    std::array<uint8_t, 100> small{};
    std::array<uint8_t, kInterleaverSize> full{};

    CHECK(block_deinterleave(small, full) == InterleaverStatus::kInputTooSmall);
    CHECK(block_deinterleave(full, small) == InterleaverStatus::kOutputTooSmall);

    std::array<float, 100> small_f{};
    std::array<float, kInterleaverSize> full_f{};
    CHECK(block_deinterleave_soft(small_f, full_f) == InterleaverStatus::kInputTooSmall);
    CHECK(block_deinterleave_soft(full_f, small_f) == InterleaverStatus::kOutputTooSmall);
}

TEST_CASE("interleave: aliasing detection", "[interleaver]") {
    std::array<uint8_t, kInterleaverSize> in{};
    std::array<uint8_t, kInterleaverSize> out{};

    for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<uint8_t>(i % 2);
    CHECK(block_interleave(in, out) == InterleaverStatus::kOk);

    CHECK(block_interleave(in, in) == InterleaverStatus::kFaultDetected);
}
