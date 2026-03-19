// Bounds-checked array access: all workspace array indices are validated by
// precondition checks (bounds_ok, K_nominal, K_full) before use. Suppressing
// constant-array-index for workspace member access is intentional.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/core/safety.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace lunalink::signal {

namespace {
// Sentinel for "no identity sub-matrix in this check layer."
// Layer 1 (H1 = [A|B]) has no identity; Layer 2 (H2 = [C|D|I]) has an
// identity sub-matrix for p2 bits.
constexpr size_t kNoIdentity = std::numeric_limits<size_t>::max();

// Maximum combined row degree across all layer configurations.
// Validated statically in tests: max(A+B) and max(C+D+1) both fit within 16.
constexpr uint8_t kMaxRowDegree = 16;

// High-confidence LLR for known-zero filler bits (Q8.7, ~+250 in float).
// 32000 is the maximum safe Q8.7 value within the symmetric [-32767, +32767]
// range, giving ~+250 in float — very high confidence that filler bits are zero.
constexpr int16_t kFillerLlr = 32000;

// Syndrome check interval: check every kSyndromeInterval iterations.
// Every 5 iterations balances early-termination benefit against syndrome check
// cost. Empirically, every-iteration adds ~15% runtime; every-10 delays
// convergence detection.
constexpr uint8_t kSyndromeInterval = 5;

// CFI magic patterns for control-flow integrity verification (§5).
constexpr uint32_t kCfiMagicEncode = 0xA5A5A5A5U;
constexpr uint32_t kCfiMagicDecode = 0x5A5A5A5AU;

// C1: Absolute value for int16_t on the symmetric [-32767, +32767] range.
// SAFETY: All values entering abs16 are produced by sat_add/sat_sub (which
// clamp to [-32767, +32767]) or by compute_cv_messages (where max output is
// 32767 * 7/8 = 28671). INT16_MIN (-32768) cannot occur in this pipeline.
inline int16_t abs16(int16_t x) noexcept {
    return static_cast<int16_t>((x < 0) ? -static_cast<int32_t>(x) : static_cast<int32_t>(x));
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

// Type-specific LDPC parameters: K_nominal = message bits, K_full = K_nominal +
// filler, z = filler count, N_output = transmitted codeword length.
struct LdpcParams {
    uint16_t K_nominal, K_full, z, N_output;
    const LdpcCsrMatrix *mA, *mB, *mBinv, *mC, *mD;
};

constexpr LdpcParams ldpc_params(LdpcSubframe type) noexcept {
    const bool s = (type == LdpcSubframe::kSF2);
    return {
        static_cast<uint16_t>(s ? 1200U : 870U),
        static_cast<uint16_t>(s ? 1200U : 880U),
        static_cast<uint16_t>(s ? 240U : 176U),
        static_cast<uint16_t>(s ? 2400U : 1740U),
        s ? &kLdpc_sf2_a : &kLdpc_sf3_a,
        s ? &kLdpc_sf2_b : &kLdpc_sf3_b,
        s ? &kLdpc_sf2_b_inv : &kLdpc_sf3_b_inv,
        s ? &kLdpc_sf2_c : &kLdpc_sf3_c,
        s ? &kLdpc_sf2_d : &kLdpc_sf3_d,
    };
}

// JIT integrity check: verify all matrices for a given subframe type.
[[nodiscard]] bool verify_tables(LdpcSubframe type) noexcept {
    const auto p = ldpc_params(type);
    return p.mA->verify_integrity() && p.mB->verify_integrity() &&
           p.mBinv->verify_integrity() && p.mC->verify_integrity() &&
           p.mD->verify_integrity();
}

// --- Syndrome checking (H4: single-exit) ---

// Per-row parity check for syndrome verification. Bounds-checked.
// Returns true if parity is zero (row satisfied) and all accesses in-bounds.
[[nodiscard]] bool check_single_row_parity(
    const LdpcCsrMatrix& mSys, const LdpcCsrMatrix& mPar,
    uint16_t r, size_t sys_offset, size_t par_offset,
    size_t identity_offset,
    std::span<const int16_t> L_v) noexcept {

    uint8_t parity = 0;
    for (uint32_t i = mSys.row_ptr[r]; i < mSys.row_ptr[r + 1]; ++i) {
        const size_t c = sys_offset + mSys.col_indices[i];
        if (c >= L_v.size()) { return false; }
        parity ^= get_sign(L_v[c]);
    }
    for (uint32_t i = mPar.row_ptr[r]; i < mPar.row_ptr[r + 1]; ++i) {
        const size_t c = par_offset + mPar.col_indices[i];
        if (c >= L_v.size()) { return false; }
        parity ^= get_sign(L_v[c]);
    }
    if (identity_offset != kNoIdentity) {
        const size_t c = identity_offset + r;
        if (c >= L_v.size()) { return false; }
        parity ^= get_sign(L_v[c]);
    }
    return (parity == 0U);
}

// C4: Bounds-checked sparse syndrome for one layer. Returns false on syndrome
// failure OR on any out-of-bounds access (fault detection).
[[nodiscard]] bool check_layer_syndrome_bounded(
    const LdpcCsrMatrix& mSys, const LdpcCsrMatrix& mPar,
    size_t sys_offset, size_t par_offset,
    size_t identity_offset,  // kNoIdentity if no identity column
    std::span<const int16_t> L_v) noexcept {

    for (uint16_t r = 0; r < mSys.num_rows; ++r) {
        if (!check_single_row_parity(mSys, mPar, r, sys_offset, par_offset,
                                     identity_offset, L_v)) {
            return false;
        }
    }
    return true;
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

// Per-row sparse dot product with bounds checking.
struct SparseRowResult {
    uint8_t acc = 0;
    uint32_t ops = 0;
    bool ok = true;
};

[[nodiscard]] SparseRowResult sparse_row_dot_product(
    const LdpcCsrMatrix& m, uint16_t r,
    std::span<const uint8_t> vec) noexcept {
    SparseRowResult res{};
    const uint32_t start = m.row_ptr[r];
    const uint32_t end = m.row_ptr[r + 1];
    if (start > end || end > m.num_entries) {
        res.ok = false;
        return res;
    }
    for (uint32_t i = start; i < end; ++i) {
        if (m.col_indices[i] >= vec.size()) {
            res.ok = false;
            return res;
        }
        res.acc ^= vec[m.col_indices[i]];
        res.ops++;
    }
    return res;
}

// H1: multiply_sparse_hardened now validates row_ptr monotonicity and column
// bounds. Returns false on any detected corruption (not just "always true").
[[nodiscard]] bool multiply_sparse_hardened(const LdpcCsrMatrix& m,
                                           std::span<const uint8_t> vec,
                                           std::span<uint8_t> out) noexcept {
    if (m.num_rows > out.size()) { return false; }
    uint32_t ops_count = 0;
    for (uint16_t r = 0; r < m.num_rows; ++r) {
        const auto res = sparse_row_dot_product(m, r, vec);
        if (!res.ok) { return false; }
        out[r] = static_cast<uint8_t>(res.acc & 1U);
        ops_count += res.ops;
    }
    // CFI: verify total operations matches expected non-zero count.
    return (ops_count == m.num_entries);
}

[[nodiscard]] bool validate_bits(std::span<const uint8_t> bits) noexcept {
    uint8_t fault_mask = 0;
    for (const uint8_t b : bits) {
        fault_mask |= (b & 0xFEU);
    }
    return (fault_mask == 0);
}

// --- Layer update: decomposed into gather, compute, scatter (A6) ---

// Per-row state for the NMS gather phase. Stores L_vc (variable-to-check
// messages), column indices, min1/min2 for the NMS exclusion trick, sign parity,
// and mirrored copies for radiation fault detection (JAXA TMR pattern).
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

// Update min1/min2 pair with mirrored copies for fault detection.
void update_min_pair(int16_t abs_v, RowGatherResult& res) noexcept {
    if (abs_v < res.min1) {
        res.min2 = res.min1;
        res.min2_mir = static_cast<int16_t>(~res.min2);
        res.min1 = abs_v;
        res.min1_mir = static_cast<int16_t>(~res.min1);
    } else if (abs_v < res.min2) {
        res.min2 = abs_v;
        res.min2_mir = static_cast<int16_t>(~res.min2);
    }
}

// Process one variable node contribution: compute L_vc, update sign/min pair.
void gather_one_entry(RowGatherResult& res, size_t col,
                      int16_t lv, int16_t lcv) noexcept {
    int16_t lvc = sat_sub(lv, lcv);
    int16_t abs_v = abs16(lvc);
    res.sign ^= get_sign(lvc);
    res.sign_mir = static_cast<uint8_t>(~res.sign);
    update_min_pair(abs_v, res);
    res.L_vc[res.degree] = lvc;
    res.cols[res.degree] = col;
    res.degree++;
}

// Gather variable-to-check messages from sparse matrix entries for one row.
void gather_matrix_entries(RowGatherResult& res, const LdpcCsrMatrix& m,
                           size_t c_off, uint16_t r,
                           std::span<const int16_t> L_v,
                           std::span<const int16_t> L_cv,
                           size_t cv_ptr) noexcept {
    for (uint32_t i = m.row_ptr[r]; i < m.row_ptr[r + 1] && res.valid; ++i) {
        if (res.degree >= kMaxRowDegree) { res.valid = false; return; }
        const size_t c = c_off + m.col_indices[i];
        const size_t cv_idx = cv_ptr + res.degree;
        if (c >= L_v.size() || cv_idx >= L_cv.size()) { res.valid = false; return; }
        gather_one_entry(res, c, L_v[c], L_cv[cv_idx]);
    }
}

// Gather the identity sub-matrix entry (layer 2 only).
void gather_identity_entry(RowGatherResult& res, size_t identity_col_offset,
                           uint16_t r, std::span<const int16_t> L_v,
                           std::span<const int16_t> L_cv_identity) noexcept {
    if (identity_col_offset == kNoIdentity || !res.valid) { return; }
    if (res.degree >= kMaxRowDegree) { res.valid = false; return; }
    const size_t ic = identity_col_offset + r;
    if (ic >= L_v.size() || r >= L_cv_identity.size()) { res.valid = false; return; }
    gather_one_entry(res, ic, L_v[ic], L_cv_identity[r]);
}

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
        gather_matrix_entries(res, *matrices[m_idx], col_offsets[m_idx], r,
                              L_v, L_cv, cv_ptr);
    }
    gather_identity_entry(res, identity_col_offset, r, L_v, L_cv_identity);
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

// Scatter one entry: write CV message and update L_v for a single variable node.
[[nodiscard]] bool scatter_one_entry(
    size_t c, int16_t lvc, int16_t cv_new_val,
    bool is_identity_col, uint16_t r,
    std::span<int16_t> L_v, std::span<int16_t> L_cv,
    std::span<int16_t> L_cv_identity, size_t cv_idx) noexcept {
    if (c >= L_v.size()) { return false; }
    if (is_identity_col) {
        if (r >= L_cv_identity.size()) { return false; }
        L_v[c] = sat_add(lvc, cv_new_val);
        L_cv_identity[r] = cv_new_val;
    } else {
        if (cv_idx >= L_cv.size()) { return false; }
        L_cv[cv_idx] = cv_new_val;
        L_v[c] = sat_add(lvc, cv_new_val);
    }
    return true;
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

    for (uint8_t d = 0; d < gr.degree; ++d) {
        const bool is_id = has_identity && gr.cols[d] == identity_c;
        if (!scatter_one_entry(gr.cols[d], gr.L_vc[d], cv_new[d],
                               is_id, r, L_v, L_cv, L_cv_identity,
                               cv_ptr + d)) {
            return false;
        }
    }
    return true;
}

// Validate that all matrices in a layer have the same number of rows.
[[nodiscard]] bool validate_layer_row_counts(
    const LdpcCsrMatrix* const* matrices, size_t num_matrices,
    uint16_t& num_rows) noexcept {
    if (num_matrices == 0) { return false; }
    num_rows = matrices[0]->num_rows;
    for (size_t m = 1; m < num_matrices; ++m) {
        if (matrices[m]->num_rows != num_rows) { return false; }
    }
    return true;
}

// Full layer update: iterate rows, gather then scatter.
// H4: Single exit. H5: Row-count CFI. H6: cv_ptr bounds validation.
[[nodiscard]] bool update_check_layer(
    const LdpcCsrMatrix* const* matrices, const size_t* col_offsets,
    size_t num_matrices, size_t identity_col_offset,
    int16_t nms_num, int16_t nms_den,
    std::span<int16_t> L_v, std::span<int16_t> L_cv,
    std::span<int16_t> L_cv_identity) noexcept {

    uint16_t num_rows = 0;
    if (!validate_layer_row_counts(matrices, num_matrices, num_rows)) {
        return false;
    }

    const bool has_identity = (identity_col_offset != kNoIdentity);
    size_t cv_ptr = 0;
    uint16_t rows_processed = 0;  // H5: CFI row counter

    for (uint16_t r = 0; r < num_rows; ++r) {
        RowGatherResult gr = gather_row(
            matrices, col_offsets, num_matrices, identity_col_offset,
            L_v, L_cv, L_cv_identity, cv_ptr, r);
        if (!gr.valid || !gr.verify_mirrors()) { return false; }

        std::array<int16_t, kMaxRowDegree> cv_new{};
        compute_cv_messages(gr, nms_num, nms_den, cv_new);
        if (!scatter_row(gr, cv_new, identity_col_offset, r,
                         L_v, L_cv, L_cv_identity, cv_ptr)) {
            return false;
        }
        cv_ptr += (gr.degree - (has_identity ? 1U : 0U));
        // H6: Validate cv_ptr hasn't exceeded the L_cv span.
        if (cv_ptr > L_cv.size()) { return false; }
        rows_processed = rows_processed + 1U;
    }

    // H5: Verify all rows were processed (detects radiation-induced loop skip).
    return (rows_processed == num_rows);
}

// Per-row parity check for encoder self-check (uint8_t domain).
[[nodiscard]] bool compute_row_parity(
    const LdpcCsrMatrix& mSys, const LdpcCsrMatrix& mPar,
    uint16_t r,
    std::span<const uint8_t> s, std::span<const uint8_t> p,
    std::span<const uint8_t> identity_span) noexcept {
    uint8_t parity = 0;
    for (uint32_t i = mSys.row_ptr[r]; i < mSys.row_ptr[r + 1]; ++i) {
        if (mSys.col_indices[i] >= s.size()) { return false; }
        parity ^= s[mSys.col_indices[i]];
    }
    for (uint32_t i = mPar.row_ptr[r]; i < mPar.row_ptr[r + 1]; ++i) {
        if (mPar.col_indices[i] >= p.size()) { return false; }
        parity ^= p[mPar.col_indices[i]];
    }
    if (!identity_span.empty()) {
        if (r >= identity_span.size()) { return false; }
        parity ^= identity_span[r];
    }
    return ((parity & 1U) == 0U);
}

// Verify H * codeword = 0 (mod 2) for one layer (systematic + parity matrices).
[[nodiscard]] bool verify_layer_parity(
    const LdpcCsrMatrix& mSys, const LdpcCsrMatrix& mPar,
    std::span<const uint8_t> s, std::span<const uint8_t> p,
    std::span<const uint8_t> identity_span) noexcept {
    for (uint16_t r = 0; r < mSys.num_rows; ++r) {
        if (!compute_row_parity(mSys, mPar, r, s, p, identity_span)) {
            return false;
        }
    }
    return true;
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

    return verify_layer_parity(mA, mB, s, p1, {}) &&
           verify_layer_parity(mC, mD, s, p1, p2);
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

// Validate NMS scaling factor range.
[[nodiscard]] bool validate_nms_alpha(int16_t nms_num, int16_t nms_den) noexcept {
    return (nms_num >= 1 && nms_num <= 7 &&
            nms_den >= 1 && nms_den <= 8 &&
            nms_num <= nms_den);
}

// Encoder DSP core: sparse multiply chain + p2 computation + self-check.
// Milestones 1-4 are tracked via cfi_count reference.
[[nodiscard]] bool encode_compute_parity(
    LdpcSubframe type, std::span<const uint8_t> msg,
    LdpcEncodeWorkspace& ws,
    volatile uint32_t& cfi_count) noexcept {

    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const uint16_t K_nominal = is_sf2 ? 1200U : 870U;
    const uint16_t K_full = is_sf2 ? 1200U : 880U;
    const LdpcCsrMatrix& mA = is_sf2 ? kLdpc_sf2_a : kLdpc_sf3_a;
    const LdpcCsrMatrix& mBinv = is_sf2 ? kLdpc_sf2_b_inv : kLdpc_sf3_b_inv;
    const LdpcCsrMatrix& mC = is_sf2 ? kLdpc_sf2_c : kLdpc_sf3_c;
    const LdpcCsrMatrix& mD = is_sf2 ? kLdpc_sf2_d : kLdpc_sf3_d;

    auto& s = ws.s;
    auto& p1 = ws.p1;
    // cppcheck-suppress variableScope
    auto& p2_full = ws.p2;
    auto& t_as = ws.t_as;
    auto& t_cs = ws.t_cs;
    auto& t_dp1 = ws.t_dp1;

    std::ranges::fill(s, uint8_t{0});
    for (size_t i = 0; i < static_cast<size_t>(K_nominal); ++i) { s[i] = msg[i]; }
    cfi_count = cfi_count + 1U;  // milestone 1: input copied

    bool ok =
        multiply_sparse_hardened(mA, std::span(s).subspan(0, K_full),
                                 std::span(t_as).subspan(0, mA.num_rows)) &&
        multiply_sparse_hardened(mBinv, std::span(t_as).subspan(0, mA.num_rows),
                                 std::span(p1).subspan(0, mBinv.num_rows)) &&
        multiply_sparse_hardened(mC, std::span(s).subspan(0, K_full),
                                 std::span(t_cs).subspan(0, mC.num_rows)) &&
        multiply_sparse_hardened(mD, std::span(p1).subspan(0, mD.num_cols),
                                 std::span(t_dp1).subspan(0, mD.num_rows));
    cfi_count = cfi_count + 1U;  // milestone 2: sparse multiplies done

    if (ok) {
        for (uint16_t i = 0; i < mC.num_rows; ++i) {
            p2_full[i] = static_cast<uint8_t>(t_cs[i] ^ t_dp1[i]);
        }
        cfi_count = cfi_count + 1U;  // milestone 3: p2 computed

        ok = verify_codeword(type,
                             std::span(s).subspan(0, K_full),
                             std::span(p1).subspan(0, mA.num_rows),
                             std::span(p2_full).subspan(0, mC.num_rows));
        cfi_count = cfi_count + 1U;  // milestone 4: H*c=0 verified
    }
    return ok;
}

// Pack encoded output bits per LSIS §2.4.4: (K_nominal - z) systematic bits
// (skipping z filler), p1_count parity-1 bits, then p2 parity-2 bits.
// Total = N_output.
void pack_output_bits(const LdpcEncodeWorkspace& ws,
                      std::span<uint8_t> out, bool is_sf2) noexcept {
    const uint16_t K_nominal = is_sf2 ? 1200U : 870U;
    const uint16_t z = is_sf2 ? 240U : 176U;
    const uint16_t N_output = is_sf2 ? 2400U : 1740U;
    const uint16_t p1_count = is_sf2 ? 480U : 352U;

    size_t out_idx = 0;
    for (size_t i = z; i < static_cast<size_t>(K_nominal); ++i) {
        out[out_idx++] = ws.s[i];
    }
    for (size_t i = 0; i < static_cast<size_t>(p1_count); ++i) {
        out[out_idx++] = ws.p1[i];
    }
    const size_t p2_needed = static_cast<size_t>(N_output) - out_idx;
    for (size_t i = 0; i < p2_needed && i < ws.p2.size(); ++i) {
        out[out_idx++] = ws.p2[i];
    }
}

// Load channel LLRs from received symbols into the L_v workspace.
// Convert float LLR to Q8.7 fixed-point (x128). Clamp to [-32000, +32000]
// symmetric range. Filler bits for SF3/SF4 shortening set to kFillerLlr
// (high-confidence zero).
void load_channel_llrs(std::span<const float> symbols,
                       std::span<int16_t> L_v,
                       uint16_t z, uint16_t K_nominal, uint16_t K_full,
                       uint16_t p1_count, size_t p1_start,
                       size_t p2_start) noexcept {
    size_t sym_idx = 0;
    for (size_t i = z; i < static_cast<size_t>(K_nominal); ++i) {
        L_v[i] = static_cast<int16_t>(
            std::clamp(symbols[sym_idx++] * 128.0F, -32000.0F, 32000.0F));
    }
    for (size_t i = K_nominal; i < static_cast<size_t>(K_full); ++i) {
        L_v[i] = kFillerLlr;
    }
    for (size_t i = p1_start; i < (p1_start + p1_count); ++i) {
        L_v[i] = static_cast<int16_t>(
            std::clamp(symbols[sym_idx++] * 128.0F, -32000.0F, 32000.0F));
    }
    for (size_t i = p2_start; i < L_v.size() && sym_idx < symbols.size(); ++i) {
        L_v[i] = static_cast<int16_t>(
            std::clamp(symbols[sym_idx++] * 128.0F, -32000.0F, 32000.0F));
    }
}

// Result of belief propagation iteration loop.
struct BpResult {
    bool converged = false;
    bool layer_fault = false;
};

// Run layered NMS belief propagation iterations.
// A3: NMS alpha is re-validated at syndrome check points.
BpResult run_bp_iterations(
    LdpcSubframe type,
    const LdpcCsrMatrix& mA, const LdpcCsrMatrix& mB,
    const LdpcCsrMatrix& mC, const LdpcCsrMatrix& mD,
    size_t K_full, size_t l1_cv_count, size_t p2_start,
    int16_t nms_num_init, int16_t nms_den_init,
    uint8_t iterations,
    std::span<int16_t> L_v, std::span<int16_t> L_cv,
    std::span<int16_t> L_cv_identity) noexcept {

    // volatile: prevent compiler from eliding periodic re-validation (A3)
    // that detects register-level SEU corruption of these scalars.
    volatile int16_t nms_num = nms_num_init;
    volatile int16_t nms_den = nms_den_init;

    const std::array<const LdpcCsrMatrix*, 2> l1_mats = {&mA, &mB};
    const std::array<size_t, 2> l1_offs = {0, K_full};
    const std::array<const LdpcCsrMatrix*, 2> l2_mats = {&mC, &mD};
    const std::array<size_t, 2> l2_offs = {0, K_full};

    BpResult result{check_syndrome(type, L_v), false};  // noiseless short-circuit

    for (uint8_t iter = 0; iter < iterations && !result.converged; ++iter) {
        if (!update_check_layer(l1_mats.data(), l1_offs.data(), 2, kNoIdentity,
                                nms_num, nms_den,
                                L_v, L_cv, L_cv_identity)) {
            result.layer_fault = true;
            break;
        }

        if (!update_check_layer(l2_mats.data(), l2_offs.data(), 2, p2_start,
                                nms_num, nms_den,
                                L_v, L_cv.subspan(l1_cv_count),
                                L_cv_identity)) {
            result.layer_fault = true;
            break;
        }

        // A3: Re-validate NMS alpha at syndrome check points.
        const bool check_now = ((iter + 1U) % kSyndromeInterval == 0U) ||
                               (iter + 1U == iterations);
        if (check_now && !validate_nms_alpha(nms_num, nms_den)) {
            result.layer_fault = true;
            break;
        }
        if (check_now && check_syndrome(type, L_v)) {
            result.converged = true;
            break;
        }
        wip_tick();
    }
    return result;
}

// Determine final decode status from BP result and CFI counter.
LdpcStatus decode_final_status(const BpResult& bp,
                               uint32_t final_cfi) noexcept {
    constexpr uint32_t kExpectedCfi = kCfiMagicDecode + 4U;
    if (bp.layer_fault || final_cfi != kExpectedCfi) {
        return LdpcStatus::kFaultDetected;  // FIZ-1
    }
    if (bp.converged) {
        return LdpcStatus::kOk;
    }
    return LdpcStatus::kNotConverged;
}

// Extract hard-decision bits from LLR workspace.
void extract_hard_bits(std::span<const int16_t> L_v, uint16_t K_nominal,
                       std::span<uint8_t> out) noexcept {
    for (size_t i = 0; i < static_cast<size_t>(K_nominal); ++i) {
        out[i] = (L_v[i] < 0 ? 1U : 0U);
    }
}
} // namespace

// --- Encoder (A4: single exit, A5: CFI sentinel) ---

LdpcStatus ldpc_encode(LdpcSubframe type, std::span<const uint8_t> msg,
                       std::span<uint8_t> out, LdpcEncodeWorkspace& workspace) noexcept {
    if (type != LdpcSubframe::kSF2 && type != LdpcSubframe::kSF3 && type != LdpcSubframe::kSF4) {
        return LdpcStatus::kInvalidInput;
    }
    const bool is_sf2 = (type == LdpcSubframe::kSF2);
    const uint16_t K_nominal = is_sf2 ? 1200U : 870U;
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
        // --- DSP core (milestones 1-4 tracked inside) ---
        bool dsp_ok = encode_compute_parity(type, msg, workspace, cfi_count);

        if (dsp_ok) {
            pack_output_bits(workspace, out, is_sf2);
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
    if (type != LdpcSubframe::kSF2 && type != LdpcSubframe::kSF3 && type != LdpcSubframe::kSF4) {
        return LdpcStatus::kInvalidInput;
    }
    const auto p = ldpc_params(type);
    LdpcStatus status = LdpcStatus::kFaultDetected;

    // A5: CFI sentinel counter.
    volatile uint32_t cfi_count = kCfiMagicDecode;

    // --- Precondition checks ---
    // M4: Reject zero iterations. M5: Validate symbol count matches N_output.
    if (iterations == 0 || symbols.size() != static_cast<size_t>(p.N_output)) {
        status = LdpcStatus::kInvalidInput;
    } else if (out.size() < static_cast<size_t>(p.K_nominal)) {
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
        const int16_t nms_num_init = workspace.alpha_numerator;
        const int16_t nms_den_init = workspace.alpha_denominator;

        const size_t l1_cv_count = p.mA->num_entries + p.mB->num_entries;
        const size_t l2_cv_count = p.mC->num_entries + p.mD->num_entries;
        const size_t p1_start = p.K_full;
        const size_t p2_start = p1_start + p.mA->num_rows;

        bool bounds_ok = validate_nms_alpha(nms_num_init, nms_den_init) &&
                         (l1_cv_count + l2_cv_count <= L_cv.size()) &&
                         (p.mC->num_rows <= L_cv_identity.size());
        cfi_count = cfi_count + 1U;  // milestone 1: bounds validated

        if (bounds_ok) {
            // Initialize workspace
            std::ranges::fill(L_v, static_cast<int16_t>(0));
            std::ranges::fill(L_cv, static_cast<int16_t>(0));
            std::ranges::fill(L_cv_identity, static_cast<int16_t>(0));

            load_channel_llrs(symbols, L_v, p.z, p.K_nominal, p.K_full,
                              p.mA->num_rows, p1_start, p2_start);
            cfi_count = cfi_count + 1U;  // milestone 2: LLRs loaded

            auto bp = run_bp_iterations(type, *p.mA, *p.mB, *p.mC, *p.mD,
                                        p.K_full, l1_cv_count, p2_start,
                                        nms_num_init, nms_den_init,
                                        iterations, L_v, L_cv, L_cv_identity);
            cfi_count = cfi_count + 1U;  // milestone 3: iteration loop complete

            extract_hard_bits(L_v, p.K_nominal, out);
            cfi_count = cfi_count + 1U;  // milestone 4: output extracted

            scrub_workspace(workspace);

            const uint32_t final_cfi = cfi_count;  // read volatile
            status = decode_final_status(bp, final_cfi);
        }
    }

    return status;
}

} // namespace lunalink::signal
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
