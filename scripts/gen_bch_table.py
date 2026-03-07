#!/usr/bin/env python3
"""Generate the 400-entry codebook LUT for the BCH(51,8) ML decoder.

Applies Class A Hardening: alignas(64) and constinit.
Packing: LSB-first (First bit of codeword is bit 0 of uint64_t).
"""


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
        # count set bits in state & mask
        count = bin(state & feedback_mask).count("1")
        fb = count % 2
        state = ((state << 1) | fb) & 0xFF
        codeword_bits.append(output ^ bit0)

    # Pack 52 bits into a uint64_t (LSB of u64 is first bit of codeword)
    val = 0
    for i, b in enumerate(codeword_bits):
        if b:
            val |= 1 << i
    return val


def main():
    """Process all FID/TOI pairs and generate hardened C++ LUT."""
    header = (
        '#include "lunalink/signal/bch.hpp"\n'
        '#include "lunalink/signal/safety.hpp"\n'
        "#include <cstdint>\n"
        "#include <array>\n\n"
        "namespace lunalink::signal {\n\n"
        "/**\n"
        " * @brief Pre-computed BCH(51,8) codebook for all 400 FID/TOI pairs. "
        "[LSIS-AFS-501]\n"
        " * Generated using generator polynomial 1 + X^3 + X^4 + X^5 + X^6 + "
        "X^7 + X^8.\n"
        " * Flight-Hardened: 64-byte alignment, section pinning, and constinit.\n"
        " * Packing: LSB-first (First bit of codeword is bit 0 of uint64_t).\n"
        " */\n"
        "alignas(64) LUNALINK_LUT_SECTION extern constinit const "
        "std::array<uint64_t, 400> kBchCodebook = {\n"
    )
    footer = "};\n\n} // namespace lunalink::signal\n"

    with open("cpp/signal/bch_table.cpp", "w") as f:
        f.write(header)
        for fid in range(4):
            f.write(f"  // FID {fid}\n")
            for toi in range(100):
                val = bch_encode_bits(fid, toi)
                f.write(f"  0x{val:016X}ULL,")
                if (toi + 1) % 4 == 0:
                    f.write("\n")
            f.write("\n")
        f.write(footer)
    print("Generated bch_table.cpp: kBchCodebook (alignas(64), constinit, LSB-first)")


if __name__ == "__main__":
    main()
