"""Generate BCH(51,8) codebook table as C++ source."""


def original_bch_encode(fid, toi):  # noqa: D103
    bit0 = (fid >> 1) & 1
    state = ((fid & 1) << 7) | (toi & 0x7F)
    feedback_mask = 0b11111001

    out = [bit0]
    for _ in range(51):
        output = (state >> 7) & 1

        # Popcount feedback
        acc = 0
        for i in range(8):
            if (feedback_mask >> i) & 1:
                if (state >> i) & 1:
                    acc += 1
        fb = acc % 2

        state = ((state << 1) | fb) & 0xFF
        out.append(output ^ bit0)
    return out


def pack_bits_to_u64(bits):  # noqa: D103
    val = 0
    for i, b in enumerate(bits):
        if b:
            val |= 1 << i
    return val


print('#include "lunalink/signal/bch_table.hpp"')
print("#include <array>")
print("")
print("namespace lunalink::signal {")
print("")
print("/**")
print(" * @brief Precomputed BCH(51,8) codewords for ML Correlation decoding.")
print(" * Each entry contains 52 bits packed into a uint64_t.")
print(" */")
print(
    "alignas(64) LUNALINK_LUT_SECTION constinit"
    " const std::array<uint64_t, 400> kBchCodebook = {{"
)

for fid in range(4):
    for toi in range(100):
        bits = original_bch_encode(fid, toi)
        val = pack_bits_to_u64(bits)
        print(f"    0x{val:016X}ULL, // FID={fid}, TOI={toi}")

print("}};")
print("")
print("} // namespace lunalink::signal")
