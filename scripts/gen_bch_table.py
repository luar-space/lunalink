#!/usr/bin/env python3
"""Generate the 400-entry codebook LUT for the BCH(51,8) ML decoder."""


def bch_encode_bits(fid, toi):
    """Symmetry check: identical logic to cpp/signal/bch.cpp."""
    # SB1 = 9 bits: [FID(1:0), TOI(6:0)]
    bit0 = (fid >> 1) & 1
    # Bits 1-8 are fed into the LFSR (FID bit 0 and TOI bits 6-0)
    state = ((fid & 1) << 7) | (toi & 0x7F)

    # Polynomial: 1 + X^3 + X^4 + X^5 + X^6 + X^7 + X^8
    # Tap mask: bits 0, 3, 4, 5, 6, 7
    feedback_mask = 0b11111001
    codeword_bits = [bit0]

    for _ in range(51):
        output = (state >> 7) & 1
        # Parity of tapped stages
        fb = bin(state & feedback_mask).count("1") % 2
        # Shift and load feedback
        state = ((state << 1) | fb) & 0xFF
        codeword_bits.append(output ^ bit0)

    # Pack 52 bits into a uint64_t (LSB of u64 is first bit of codeword)
    val = 0
    for i, b in enumerate(codeword_bits):
        if b:
            val |= 1 << i
    return val


def main():
    """Generate the BCH codebook."""
    header = """// GENERATED - do not edit.
// Re-run scripts/gen_bch_table.py to regenerate.
#include "lunalink/signal/bch.hpp"
#include <cstdint>
#include <array>

namespace lunalink::signal {

// Packed codebook for BCH(51,8) ML decoder.
// Each uint64_t contains 52 symbols (packed LSB-first).
// Indexing: 100 * FID + TOI (400 entries total).
// Aligned to 64 bytes for optimal cache-line fetching on flight hardware.
alignas(64) extern const std::array<uint64_t, 400> kBchCodebook = {{
"""
    footer = """}};

} // namespace lunalink::signal
"""

    with open("cpp/signal/bch_table.cpp", "w") as f:
        f.write(header)
        for fid in range(4):
            f.write(f"  // FID {fid}\n")
            for toi in range(100):
                cw = bch_encode_bits(fid, toi)
                f.write(f"  0x{cw:016X}ULL,")
                if (toi + 1) % 4 == 0:
                    f.write("\n")
            f.write("\n")
        f.write(footer)


if __name__ == "__main__":
    main()
