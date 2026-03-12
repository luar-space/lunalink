// NOLINTBEGIN(hicpp-*,cppcoreguidelines-pro-bounds-*)
#include "lunalink/signal/ldpc.hpp"
#include "lunalink/signal/safety.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace lunalink::signal {

namespace {
// C2: Portable sentinel for "no identity sub-matrix in this check layer."
constexpr size_t kNoIdentity = std::numeric_limits<size_t>::max();

// Maximum combined row degree across all layer configurations.
// Validated statically in tests: max(A+B) and max(C+D+1) both fit within 16.
constexpr uint8_t kMaxRowDegree = 16;

// High-confidence LLR for known-zero filler bits (Q8.7, ~+250 in float).
constexpr int16_t kFillerLlr = 32000;

// Syndrome check interval: check every kSyndromeInterval iterations.
constexpr uint8_t kSyndromeInterval = 5;

// CFI magic patterns for control-flow integrity verification (§5).
constexpr uint32_t kCfiMagicEncode = 0xA5A5A5A5U;
constexpr uint32_t kCfiMagicDecode = 0x5A5A5A5AU;

// C1: Absolute value for int16_t on the symmetric [-32767, +32767] range.
// SAFETY: All values entering abs16 are produced by sat_add/sat_sub (which
// clamp to [-32767, +32767]) or by compute_cv_messages (where max output is
// 32767 * 7/8 = 28671). INT16_MIN (-32768) cannot occur in this pipeline.
inline int16_t abs16(int16_t x) noexcept {
    return (x < 0) ? static_cast<int16_t>(-x) : x;
}

inline uint8_t get_sign(int16_t val) noexcept {
    return static_cast<uint8_t>((static_cast<uint16_t>(val) >> 15U) & 1U);
}

// Saturating arithmetic clamped to [-32767, +32767] (symmetric range).
// Avoids -32768 so that abs16() is always safe without UB on INT16_MIN.
inline int16_t sat_add(int16_t a, int16_t b) noexcept {
    int32_t res = static_cast<int32_t>(a) + static_cast<int32_t>(b);
    if (res > 32767) { res = 32767; }
    if (res < -32767) { res = -32767; }
    return static_cast<int16_t>(res);
}

inline int16_t sat_sub(int16_t a, int16_t b) noexcept {
    int32_t res = static_cast<int32_t>(a) - static_cast<int32_t>(b);
    if (res > 32767) { res = 32767; }
    if (res < -32767) { res = -32767; }
    return static_cast<int16_t>(res);
}

// JIT integrity check: verify all matrices for a given subframe type.
[[nodiscard]] bool verify_tables(LdpcSubframe type) noexcept {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
    const LdpcCsrMatrix& mB = is_sf2 ? kLdpc_sf2_b : kLdpc_sf3_b;
    const LdpcCsrMatrix& mBinv = is_sf2 ? kLdpc_sf2_b_inv : kLdpc_sf3_b_inv;
    const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
    const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;
    return mA.verify_integrity() && mB.verify_integrity() &&
           mBinv.verify_integrity() && mC.verify_integrity() &&
           mD.verify_integrity();
}

// --- Syndrome checking (H4: single-exit) ---

// C4: Bounds-checked sparse syndrome for one layer. Returns false on syndrome
// failure OR on any out-of-bounds access (fault detection).
[[nodiscard]] bool check_layer_syndrome_bounded(
    const LdpcCsrMatrix& mSys, const LdpcCsrMatrix& mPar,
    size_t sys_offset, size_t par_offset,
    size_t identity_offset,  // kNoIdentity if no identity column
    std::span<const int16_t> L_v) noexcept {

    bool ok = true;
    for (uint16_t r = 0; r < mSys.num_rows && ok; ++r) {
        uint8_t parity = 0;
        for (uint32_t i = mSys.row_ptr[r]; i < mSys.row_ptr[r + 1] && ok; ++i) {
            const size_t c = sys_offset + mSys.col_indices[i];
            if (c >= L_v.size()) { ok = false; }
            else { parity ^= static_cast<uint8_t>(L_v[c] < 0 ? 1U : 0U); }
        }
        for (uint32_t i = mPar.row_ptr[r]; i < mPar.row_ptr[r + 1] && ok; ++i) {
            const size_t c = par_offset + mPar.col_indices[i];
            if (c >= L_v.size()) { ok = false; }
            else { parity ^= static_cast<uint8_t>(L_v[c] < 0 ? 1U : 0U); }
        }
        if (identity_offset != kNoIdentity && ok) {
            const size_t c = identity_offset + r;
            if (c >= L_v.size()) { ok = false; }
            else { parity ^= static_cast<uint8_t>(L_v[c] < 0 ? 1U : 0U); }
        }
        if (ok && parity != 0U) { ok = false; }
    }
    return ok;
}

[[nodiscard]] bool check_syndrome(LdpcSubframe type, std::span<const int16_t> L_v) noexcept {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const uint16_t K_full = is_sf2 ? 1200U : 880U;
    const size_t p2_start = K_full + (is_sf2 ? 480U : 352U);
    const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
    const LdpcCsrMatrix& mB = is_sf2 ? kLdpc_sf2_b : kLdpc_sf3_b;
    const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
    const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;
    return check_layer_syndrome_bounded(mA, mB, 0, K_full, kNoIdentity, L_v) &&
           check_layer_syndrome_bounded(mC, mD, 0, K_full, p2_start, L_v);
}

// --- Sparse matrix operations (H1: actual hardening, C4: bounds checks) ---

// H1: multiply_sparse_hardened now validates row_ptr monotonicity and column
// bounds. Returns false on any detected corruption (not just "always true").
[[nodiscard]] bool multiply_sparse_hardened(const LdpcCsrMatrix& m,
                                           std::span<const uint8_t> vec,
                                           std::span<uint8_t> out) noexcept {
    bool ok = (m.num_rows <= out.size());
    uint32_t ops_count = 0;
    for (uint16_t r = 0; r < m.num_rows && ok; ++r) {
        uint8_t acc = 0;
        const uint32_t start = m.row_ptr[r];
        const uint32_t end = m.row_ptr[r + 1];
        if (start > end || end > m.num_entries) { ok = false; }
        else {
            for (uint32_t i = start; i < end && ok; ++i) {
                if (m.col_indices[i] >= vec.size()) { ok = false; }
                else { acc ^= vec[m.col_indices[i]]; ops_count++; }
            }
            if (ok) { out[r] = static_cast<uint8_t>(acc & 1U); }
        }
    }
    // CFI: verify total operations matches expected non-zero count.
    if (ok && ops_count != m.num_entries) { ok = false; }
    return ok;
}

[[nodiscard]] bool validate_bits(std::span<const uint8_t> bits) noexcept {
    uint8_t fault_mask = 0;
    for (const uint8_t b : bits)
        fault_mask |= (b & 0xFEU);
    return (fault_mask == 0);
}

// --- Layer update: decomposed into gather, compute, scatter (A6) ---

// Per-row state collected during the gather phase.
// MA §6: min1/min2/sign are mirrored (JAXA pattern) to detect ALU/register
// corruption. Mirror stores bitwise inverse; verified before compute phase.
struct RowGatherResult {
    std::array<int16_t, kMaxRowDegree> L_vc{};
    std::array<size_t, kMaxRowDegree> cols{};
    int16_t min1 = 32767;
    int16_t min2 = 32767;
    int16_t min1_mir = ~static_cast<int16_t>(32767);  // ~min1
    int16_t min2_mir = ~static_cast<int16_t>(32767);  // ~min2
    uint8_t sign = 0;
    uint8_t sign_mir = 0xFFU;  // ~sign
    uint8_t degree = 0;
    bool valid = true;

    // Verify mirrored invariants. Returns false if any corruption detected.
    [[nodiscard]] bool verify_mirrors() const noexcept {
        return (min1 == static_cast<int16_t>(~min1_mir)) &&
               (min2 == static_cast<int16_t>(~min2_mir)) &&
               (sign == static_cast<uint8_t>(~sign_mir));
    }
};

// Gather: collect variable-to-check messages and find min1/min2 for one row.
// H4: Single exit. C4: Bounds-checked array access.
[[nodiscard]] RowGatherResult gather_row(
    const LdpcCsrMatrix* const* matrices, const size_t* col_offsets,
    size_t num_matrices, size_t identity_col_offset,
    std::span<const int16_t> L_v, std::span<const int16_t> L_cv,
    std::span<const int16_t> L_cv_identity,
    size_t cv_ptr, uint16_t r) noexcept {

    RowGatherResult res{};

    for (size_t m_idx = 0; m_idx < num_matrices && res.valid; ++m_idx) {
        const auto& m = *matrices[m_idx];
        const size_t c_off = col_offsets[m_idx];
        for (uint32_t i = m.row_ptr[r]; i < m.row_ptr[r + 1] && res.valid; ++i) {
            if (res.degree >= kMaxRowDegree) { res.valid = false; }
            else {
                const size_t c = c_off + m.col_indices[i];
                const size_t cv_idx = cv_ptr + res.degree;
                if (c >= L_v.size() || cv_idx >= L_cv.size()) { res.valid = false; }
                else {
                    int16_t lvc = sat_sub(L_v[c], L_cv[cv_idx]);
                    int16_t abs_v = abs16(lvc);
                    res.sign ^= get_sign(lvc);
                    res.sign_mir = static_cast<uint8_t>(~res.sign);
                    if (abs_v < res.min1) {
                        res.min2 = res.min1;
                        res.min2_mir = static_cast<int16_t>(~res.min2);
                        res.min1 = abs_v;
                        res.min1_mir = static_cast<int16_t>(~res.min1);
                    } else if (abs_v < res.min2) {
                        res.min2 = abs_v;
                        res.min2_mir = static_cast<int16_t>(~res.min2);
                    }
                    res.L_vc[res.degree] = lvc;
                    res.cols[res.degree] = c;
                    res.degree++;
                }
            }
        }
    }

    if (identity_col_offset != kNoIdentity && res.valid) {
        if (res.degree >= kMaxRowDegree) { res.valid = false; }
        else {
            const size_t ic = identity_col_offset + r;
            if (ic >= L_v.size() || r >= L_cv_identity.size()) { res.valid = false; }
            else {
                int16_t lvc = sat_sub(L_v[ic], L_cv_identity[r]);
                int16_t abs_v = abs16(lvc);
                res.sign ^= get_sign(lvc);
                res.sign_mir = static_cast<uint8_t>(~res.sign);
                if (abs_v < res.min1) {
                    res.min2 = res.min1;
                    res.min2_mir = static_cast<int16_t>(~res.min2);
                    res.min1 = abs_v;
                    res.min1_mir = static_cast<int16_t>(~res.min1);
                } else if (abs_v < res.min2) {
                    res.min2 = abs_v;
                    res.min2_mir = static_cast<int16_t>(~res.min2);
                }
                res.L_vc[res.degree] = lvc;
                res.cols[res.degree] = ic;
                res.degree++;
            }
        }
    }

    return res;
}

// Compute new check-to-variable messages in batch (contiguous, auto-vectorizable).
void compute_cv_messages(
    const RowGatherResult& gr,
    int16_t nms_num, int16_t nms_den,
    std::array<int16_t, kMaxRowDegree>& cv_new) noexcept {

    for (uint8_t d = 0; d < gr.degree; ++d) {
        int16_t abs_v = abs16(gr.L_vc[d]);
        int16_t min_val = (abs_v == gr.min1) ? gr.min2 : gr.min1;
        // C1: max output = 32767 * 7 / 1 = 229369 as int, then truncated to int16_t.
        // But valid α has num <= den, so max = 32767 * 7/8 = 28671 — safe for int16_t.
        int16_t val = static_cast<int16_t>(
            (static_cast<int>(min_val) * nms_num) / nms_den);
        uint8_t out_sign = gr.sign ^ get_sign(gr.L_vc[d]);
        cv_new[d] = (out_sign == 0U) ? val : static_cast<int16_t>(-val);
    }
}

// Scatter: distribute pre-computed CV messages and update L_v for one row.
// C4: Bounds-checked. All L_v/L_cv writes are validated against span size.
// Returns false on any out-of-bounds access (fault detection).
[[nodiscard]] bool scatter_row(
    const RowGatherResult& gr,
    const std::array<int16_t, kMaxRowDegree>& cv_new,
    size_t identity_col_offset, uint16_t r,
    std::span<int16_t> L_v, std::span<int16_t> L_cv,
    std::span<int16_t> L_cv_identity, size_t cv_ptr) noexcept {

    const bool has_identity = (identity_col_offset != kNoIdentity);
    const size_t identity_c = has_identity ? (identity_col_offset + r) : 0;
    bool ok = true;

    for (uint8_t d = 0; d < gr.degree && ok; ++d) {
        const size_t c = gr.cols[d];
        if (c >= L_v.size()) { ok = false; }
        else if (has_identity && c == identity_c) {
            if (r >= L_cv_identity.size()) { ok = false; }
            else {
                L_v[c] = sat_add(gr.L_vc[d], cv_new[d]);
                L_cv_identity[r] = cv_new[d];
            }
        } else {
            const size_t cv_idx = cv_ptr + d;
            if (cv_idx >= L_cv.size()) { ok = false; }
            else {
                L_cv[cv_idx] = cv_new[d];
                L_v[c] = sat_add(gr.L_vc[d], cv_new[d]);
            }
        }
    }
    return ok;
}

// Full layer update: iterate rows, gather then scatter.
// H4: Single exit. H5: Row-count CFI. H6: cv_ptr bounds validation.
[[nodiscard]] bool update_check_layer(
    const LdpcCsrMatrix* const* matrices, const size_t* col_offsets,
    size_t num_matrices, size_t identity_col_offset,
    int16_t nms_num, int16_t nms_den,
    std::span<int16_t> L_v, std::span<int16_t> L_cv,
    std::span<int16_t> L_cv_identity) noexcept {

    bool ok = (num_matrices > 0);
    const uint16_t num_rows = ok ? matrices[0]->num_rows : 0;
    for (size_t m = 1; m < num_matrices && ok; ++m) {
        if (matrices[m]->num_rows != num_rows) { ok = false; }
    }

    const bool has_identity = (identity_col_offset != kNoIdentity);
    size_t cv_ptr = 0;
    uint16_t rows_processed = 0;  // H5: CFI row counter

    for (uint16_t r = 0; r < num_rows && ok; ++r) {
        RowGatherResult gr = gather_row(
            matrices, col_offsets, num_matrices, identity_col_offset,
            L_v, L_cv, L_cv_identity, cv_ptr, r);
        if (!gr.valid || !gr.verify_mirrors()) { ok = false; }
        else {
            std::array<int16_t, kMaxRowDegree> cv_new{};
            compute_cv_messages(gr, nms_num, nms_den, cv_new);
            if (!scatter_row(gr, cv_new, identity_col_offset, r,
                             L_v, L_cv, L_cv_identity, cv_ptr)) {
                ok = false;
            }
            cv_ptr += (gr.degree - (has_identity ? 1U : 0U));
            // H6: Validate cv_ptr hasn't exceeded the L_cv span.
            if (cv_ptr > L_cv.size()) { ok = false; }
            rows_processed = rows_processed + 1U;
        }
    }

    // H5: Verify all rows were processed (detects radiation-induced loop skip).
    if (rows_processed != num_rows) { ok = false; }
    return ok;
}

// Encoder self-check: verify H * codeword = 0 (mod 2). H4: single exit.
[[nodiscard]] bool verify_codeword(LdpcSubframe type,
                                   std::span<const uint8_t> s,
                                   std::span<const uint8_t> p1,
                                   std::span<const uint8_t> p2) noexcept {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
    const LdpcCsrMatrix& mB = is_sf2 ? kLdpc_sf2_b : kLdpc_sf3_b;
    const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
    const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

    bool ok = true;
    for (uint16_t r = 0; r < mA.num_rows && ok; ++r) {
        uint8_t parity = 0;
        for (uint32_t i = mA.row_ptr[r]; i < mA.row_ptr[r + 1] && ok; ++i) {
            if (mA.col_indices[i] >= s.size()) { ok = false; }
            else { parity ^= s[mA.col_indices[i]]; }
        }
        for (uint32_t i = mB.row_ptr[r]; i < mB.row_ptr[r + 1] && ok; ++i) {
            if (mB.col_indices[i] >= p1.size()) { ok = false; }
            else { parity ^= p1[mB.col_indices[i]]; }
        }
        if (ok && (parity & 1U) != 0U) { ok = false; }
    }

    for (uint16_t r = 0; r < mC.num_rows && ok; ++r) {
        uint8_t parity = 0;
        for (uint32_t i = mC.row_ptr[r]; i < mC.row_ptr[r + 1] && ok; ++i) {
            if (mC.col_indices[i] >= s.size()) { ok = false; }
            else { parity ^= s[mC.col_indices[i]]; }
        }
        for (uint32_t i = mD.row_ptr[r]; i < mD.row_ptr[r + 1] && ok; ++i) {
            if (mD.col_indices[i] >= p1.size()) { ok = false; }
            else { parity ^= p1[mD.col_indices[i]]; }
        }
        if (ok && r < p2.size()) { parity ^= p2[r]; }
        else if (ok) { ok = false; }
        if (ok && (parity & 1U) != 0U) { ok = false; }
    }
    return ok;
}

// Scrub all encoder temporaries.
void scrub_encode_workspace(LdpcEncodeWorkspace& ws) noexcept {
    secure_scrub(ws.s); secure_scrub(ws.p1); secure_scrub(ws.p2);
    secure_scrub(ws.t_as); secure_scrub(ws.t_cs); secure_scrub(ws.t_dp1);
}

// Scrub decoder workspace.
void scrub_workspace(LdpcWorkspace& ws) noexcept {
    secure_scrub(ws.L_v);
    secure_scrub(ws.L_cv);
    secure_scrub(ws.L_cv_identity);
}
} // namespace

// --- Encoder (A4: single exit, A5: CFI sentinel) ---

LdpcStatus ldpc_encode(LdpcSubframe type, std::span<const uint8_t> msg,
                       std::span<uint8_t> out, LdpcEncodeWorkspace& workspace) noexcept {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const uint16_t K_nominal = is_sf2 ? 1200U : 870U;
    const uint16_t K_full = is_sf2 ? 1200U : 880U;
    const uint16_t z = is_sf2 ? 240U : 176U;
    const uint16_t N_output = is_sf2 ? 2400U : 1740U;
    LdpcStatus status = LdpcStatus::kFaultDetected;

    // A5: CFI sentinel counter — tracks expected execution milestones.
    volatile uint32_t cfi_count = kCfiMagicEncode;

    // --- Precondition checks ---
    if (msg.size() != static_cast<size_t>(K_nominal) || !validate_bits(msg)) {
        status = LdpcStatus::kInvalidInput;
    } else if (out.size() < static_cast<size_t>(N_output)) {
        status = LdpcStatus::kOutputTooSmall;
    } else if (spans_overlap(msg.data(), msg.size_bytes(), out.data(), out.size_bytes()) ||
               !verify_tables(type)) {
        status = LdpcStatus::kFaultDetected;  // FIZ-1 (aliasing) / FIZ-3 (LUT integrity)
    } else {
        // --- DSP core ---
        const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
        const LdpcCsrMatrix& mBinv = is_sf2 ? kLdpc_sf2_b_inv : kLdpc_sf3_b_inv;
        const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
        const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

        auto& s = workspace.s;
        auto& p1 = workspace.p1;
        auto& p2_full = workspace.p2;
        auto& t_as = workspace.t_as;
        auto& t_cs = workspace.t_cs;
        auto& t_dp1 = workspace.t_dp1;

        std::ranges::fill(s, uint8_t{0});
        for (size_t i = 0; i < static_cast<size_t>(K_nominal); ++i) s[i] = msg[i];
        cfi_count = cfi_count + 1U;  // milestone 1: input copied

        bool dsp_ok =
            multiply_sparse_hardened(mA, std::span(s).subspan(0, K_full),
                                     std::span(t_as).subspan(0, mA.num_rows)) &&
            multiply_sparse_hardened(mBinv, std::span(t_as).subspan(0, mA.num_rows),
                                     std::span(p1).subspan(0, mBinv.num_rows)) &&
            multiply_sparse_hardened(mC, std::span(s).subspan(0, K_full),
                                     std::span(t_cs).subspan(0, mC.num_rows)) &&
            multiply_sparse_hardened(mD, std::span(p1).subspan(0, mD.num_cols),
                                     std::span(t_dp1).subspan(0, mD.num_rows));
        cfi_count = cfi_count + 1U;  // milestone 2: sparse multiplies done

        if (dsp_ok) {
            for (uint16_t i = 0; i < mC.num_rows; ++i)
                p2_full[i] = static_cast<uint8_t>(t_cs[i] ^ t_dp1[i]);
            cfi_count = cfi_count + 1U;  // milestone 3: p2 computed

            dsp_ok = verify_codeword(type,
                                     std::span(s).subspan(0, K_full),
                                     std::span(p1).subspan(0, mA.num_rows),
                                     std::span(p2_full).subspan(0, mC.num_rows));
            cfi_count = cfi_count + 1U;  // milestone 4: H*c=0 verified
        }

        if (dsp_ok) {
            size_t out_idx = 0;
            for (size_t i = z; i < static_cast<size_t>(K_nominal); ++i)
                out[out_idx++] = s[i];
            for (size_t i = 0; i < static_cast<size_t>(mA.num_rows); ++i)
                out[out_idx++] = p1[i];
            const size_t p2_needed = static_cast<size_t>(N_output) - out_idx;
            for (size_t i = 0; i < p2_needed && i < p2_full.size(); ++i)
                out[out_idx++] = p2_full[i];
            cfi_count = cfi_count + 1U;  // milestone 5: output written
        }

        scrub_encode_workspace(workspace);

        // A5: CFI terminal check — all 5 milestones must have been reached.
        constexpr uint32_t kExpectedCfi = kCfiMagicEncode + 5U;
        if (dsp_ok && cfi_count == kExpectedCfi) {
            status = LdpcStatus::kOk;
        }
    }

    return status;
}

// --- Decoder (A4: single exit, A5: CFI sentinel) ---

LdpcStatus ldpc_decode(LdpcSubframe type, std::span<const float> symbols,
                       uint8_t iterations, std::span<uint8_t> out,
                       LdpcWorkspace& workspace) noexcept {
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const uint16_t K_nominal = is_sf2 ? 1200U : 870U;
    const uint16_t K_full = is_sf2 ? 1200U : 880U;
    const uint16_t z = is_sf2 ? 240U : 176U;
    const uint16_t N_output = is_sf2 ? 2400U : 1740U;
    LdpcStatus status = LdpcStatus::kFaultDetected;

    // A5: CFI sentinel counter.
    volatile uint32_t cfi_count = kCfiMagicDecode;

    // --- Precondition checks ---
    // M4: Reject zero iterations. M5: Validate symbol count matches N_output.
    if (iterations == 0 || symbols.size() != static_cast<size_t>(N_output)) {
        status = LdpcStatus::kInvalidInput;
    } else if (out.size() < static_cast<size_t>(K_nominal)) {
        status = LdpcStatus::kOutputTooSmall;
    } else if (spans_overlap(symbols.data(), symbols.size_bytes(),
                              out.data(), out.size_bytes()) ||
               !verify_tables(type)) {
        status = LdpcStatus::kFaultDetected;  // FIZ-1 (aliasing) / FIZ-3 (LUT integrity)
    } else {
        // --- DSP core ---
        std::span<int16_t> L_v(workspace.L_v);
        std::span<int16_t> L_cv(workspace.L_cv);
        std::span<int16_t> L_cv_identity(workspace.L_cv_identity);

        // P5: Read NMS scaling factor from workspace.
        // volatile: prevent compiler from eliding the periodic re-validation
        // (A3) that detects register-level SEU corruption of these scalars.
        volatile int16_t nms_num = workspace.alpha_numerator;
        volatile int16_t nms_den = workspace.alpha_denominator;
        const bool alpha_ok = (nms_num >= 1 && nms_num <= 7 &&
                               nms_den >= 1 && nms_den <= 8 &&
                               nms_num <= nms_den);

        const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
        const LdpcCsrMatrix& mB = is_sf2 ? kLdpc_sf2_b : kLdpc_sf3_b;
        const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
        const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

        const size_t l1_cv_count = mA.num_entries + mB.num_entries;
        const size_t l2_cv_count = mC.num_entries + mD.num_entries;
        const size_t p1_start = K_full;
        const size_t p2_start = p1_start + mA.num_rows;

        bool bounds_ok = alpha_ok &&
                         (l1_cv_count + l2_cv_count <= L_cv.size()) &&
                         (mC.num_rows <= L_cv_identity.size());
        cfi_count = cfi_count + 1U;  // milestone 1: bounds validated

        if (bounds_ok) {
            // Initialize workspace
            std::ranges::fill(L_v, static_cast<int16_t>(0));
            std::ranges::fill(L_cv, static_cast<int16_t>(0));
            std::ranges::fill(L_cv_identity, static_cast<int16_t>(0));

            // Load channel LLRs — M5: symbols.size() == N_output is guaranteed.
            size_t sym_idx = 0;
            for (size_t i = z; i < static_cast<size_t>(K_nominal); ++i) {
                L_v[i] = static_cast<int16_t>(
                    std::clamp(symbols[sym_idx++] * 128.0f, -32000.0f, 32000.0f));
            }
            for (size_t i = K_nominal; i < static_cast<size_t>(K_full); ++i)
                L_v[i] = kFillerLlr;
            for (size_t i = p1_start; i < (p1_start + mA.num_rows); ++i) {
                L_v[i] = static_cast<int16_t>(
                    std::clamp(symbols[sym_idx++] * 128.0f, -32000.0f, 32000.0f));
            }
            for (size_t i = p2_start; i < L_v.size() && sym_idx < symbols.size(); ++i)
                L_v[i] = static_cast<int16_t>(
                    std::clamp(symbols[sym_idx++] * 128.0f, -32000.0f, 32000.0f));
            cfi_count = cfi_count + 1U;  // milestone 2: LLRs loaded

            const LdpcCsrMatrix* l1_mats[] = {&mA, &mB};
            const size_t l1_offs[] = {0, K_full};
            const LdpcCsrMatrix* l2_mats[] = {&mC, &mD};
            const size_t l2_offs[] = {0, K_full};

            bool converged = check_syndrome(type, L_v);  // noiseless short-circuit
            bool layer_fault = false;

            if (!converged) {
                for (uint8_t iter = 0; iter < iterations; ++iter) {
                    if (!update_check_layer(l1_mats, l1_offs, 2, kNoIdentity,
                                            nms_num, nms_den,
                                            L_v, L_cv, L_cv_identity)) {
                        layer_fault = true;
                        break;
                    }

                    if (!update_check_layer(l2_mats, l2_offs, 2, p2_start,
                                            nms_num, nms_den,
                                            L_v, L_cv.subspan(l1_cv_count),
                                            L_cv_identity)) {
                        layer_fault = true;
                        break;
                    }

                    // A3: Re-validate NMS alpha at syndrome check points.
                    // Detects register corruption of scaling parameters.
                    if (((iter + 1U) % kSyndromeInterval == 0U) ||
                        (iter + 1U == iterations)) {
                        if (nms_num < 1 || nms_num > 7 ||
                            nms_den < 1 || nms_den > 8 ||
                            nms_num > nms_den) {
                            layer_fault = true;
                            break;
                        }
                        if (check_syndrome(type, L_v)) {
                            converged = true;
                            break;
                        }
                    }
                    wip_tick();
                }
            }
            cfi_count = cfi_count + 1U;  // milestone 3: iteration loop complete

            // Extract decoded bits
            for (size_t i = 0; i < static_cast<size_t>(K_nominal); ++i)
                out[i] = (L_v[i] < 0 ? 1U : 0U);
            cfi_count = cfi_count + 1U;  // milestone 4: output extracted

            scrub_workspace(workspace);

            // A5: CFI terminal check — all 4 milestones must have been reached.
            constexpr uint32_t kExpectedCfi = kCfiMagicDecode + 4U;
            if (layer_fault || cfi_count != kExpectedCfi) {
                status = LdpcStatus::kFaultDetected;  // FIZ-1
            } else if (converged) {
                status = LdpcStatus::kOk;
            } else {
                status = LdpcStatus::kNotConverged;
            }
        }
    }

    return status;
}

} // namespace lunalink::signal
// NOLINTEND(hicpp-*,cppcoreguidelines-pro-bounds-*)
