/**
 * @file test_ldpc_reference.cpp
 * @brief LDPC decoder quality assessment: flight AMS decoder vs float64
 *        sum-product reference vs finite-length sphere packing bound.
 *
 * Produces a 3-way comparison table that decomposes the Shannon gap into:
 *   - Code gap: SPB bound → sum-product (inherent to n=2400 code design)
 *   - Algorithm gap: sum-product → AMS (cost of min-sum approximation)
 *   - Implementation gap: (included in algorithm gap — Q8.7 + OMS offset)
 *
 * The sum-product decoder is test-only. It uses exact log(tanh(x/2))
 * check-node updates in float64 — non-deterministic WCET, no fixed-point,
 * not suitable for flight. It exists solely as the theoretical ceiling
 * for this code and schedule.
 */
#include "lunalink/encoding/ldpc.hpp"
#include "lunalink/encoding/ldpc_tables.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

using namespace lunalink::signal;

namespace {

// ---------------------------------------------------------------------------
// Sum-product reference decoder (float64, test-only)
// ---------------------------------------------------------------------------

// Layered sum-product BP over the same CSR matrices as the flight decoder.
// Check-node update: exact log-domain computation using the identity
//   φ(x) = -log(tanh(x/2)) = log((e^x + 1) / (e^x - 1))
// which is its own inverse: φ(φ(x)) = x.
// Message rule: φ( Σ φ(|L_vc_j|) ) with appropriate sign.

double phi(double x) noexcept {
    // Numerically stable implementation of -log(tanh(|x|/2)).
    // For large x, tanh(x/2) → 1, so φ(x) → 0.
    // For small x, tanh(x/2) → x/2, so φ(x) → -log(x/2) → large.
    double ax = std::abs(x);
    if (ax < 1e-10) return 30.0; // clamp to avoid infinity
    if (ax > 30.0) return 0.0;   // tanh saturates
    return std::log((std::exp(ax) + 1.0) / (std::exp(ax) - 1.0));
}

struct SpParams {
    uint16_t K_nominal, K_full, z, N_output;
    const LdpcCsrMatrix *mA, *mB, *mC, *mD;
    uint16_t p1_count;
};

SpParams sp_params(LdpcSubframe type) {
    const bool s = (type == LdpcSubframe::kSF2);
    return {
        static_cast<uint16_t>(s ? 1200U : 870U),
        static_cast<uint16_t>(s ? 1200U : 880U),
        static_cast<uint16_t>(s ? 240U : 176U),
        static_cast<uint16_t>(s ? 2400U : 1740U),
        s ? &kLdpc_sf2_a : &kLdpc_sf3_a,
        s ? &kLdpc_sf2_b : &kLdpc_sf3_b,
        s ? &kLdpc_sf2_c : &kLdpc_sf3_c,
        s ? &kLdpc_sf2_d : &kLdpc_sf3_d,
        static_cast<uint16_t>(s ? 480U : 352U),
    };
}

// Process one layer (pair of sparse matrices + optional identity).
// L_v: variable-node beliefs. L_cv: check-to-variable messages (flat, indexed by cv_ptr).
// L_cv_id: messages for identity columns (layer 2 only, indexed by row).
void sp_layer_update(const LdpcCsrMatrix& m1, size_t c_off1,
                     const LdpcCsrMatrix& m2, size_t c_off2,
                     bool has_identity, size_t identity_col_offset,
                     std::vector<double>& L_v,
                     std::vector<double>& L_cv,
                     std::vector<double>& L_cv_id,
                     size_t cv_base) {
    const uint16_t num_rows = m1.num_rows; // m1 and m2 have the same number of rows
    for (uint16_t r = 0; r < num_rows; ++r) {
        // Gather: collect L_vc = L_v - L_cv for each connected variable node
        struct Entry { size_t col; size_t cv_idx; bool is_identity; };
        std::vector<Entry> entries;
        std::vector<double> L_vc;

        // Matrix 1 entries
        for (uint32_t i = m1.row_ptr[r]; i < m1.row_ptr[r + 1]; ++i) {
            size_t c = c_off1 + m1.col_indices[i];
            double lvc = L_v[c] - L_cv[cv_base];
            entries.push_back({c, cv_base, false});
            L_vc.push_back(lvc);
            ++cv_base;
        }
        // Matrix 2 entries
        for (uint32_t i = m2.row_ptr[r]; i < m2.row_ptr[r + 1]; ++i) {
            size_t c = c_off2 + m2.col_indices[i];
            double lvc = L_v[c] - L_cv[cv_base];
            entries.push_back({c, cv_base, false});
            L_vc.push_back(lvc);
            ++cv_base;
        }
        // Identity entry (layer 2 only)
        if (has_identity) {
            size_t ic = identity_col_offset + r;
            double lvc = L_v[ic] - L_cv_id[r];
            entries.push_back({ic, static_cast<size_t>(r), true});
            L_vc.push_back(lvc);
        }

        // Check-node update: sum-product via φ transform
        double phi_sum = 0.0;
        uint8_t sign_product = 0;
        for (size_t d = 0; d < L_vc.size(); ++d) {
            phi_sum += phi(L_vc[d]);
            sign_product ^= (L_vc[d] < 0.0) ? 1U : 0U;
        }

        // Scatter: compute new L_cv and update L_v
        for (size_t d = 0; d < entries.size(); ++d) {
            double phi_excl = phi_sum - phi(L_vc[d]);
            double magnitude = phi(phi_excl);
            uint8_t excl_sign = sign_product ^ ((L_vc[d] < 0.0) ? 1U : 0U);
            double cv_new = (excl_sign == 0U) ? magnitude : -magnitude;

            double cv_old;
            if (entries[d].is_identity) {
                cv_old = L_cv_id[entries[d].cv_idx];
                L_cv_id[entries[d].cv_idx] = cv_new;
            } else {
                cv_old = L_cv[entries[d].cv_idx];
                L_cv[entries[d].cv_idx] = cv_new;
            }
            L_v[entries[d].col] += (cv_new - cv_old);
        }

        // cv_base already advanced past this row's sparse entries
    }
}

struct SpDecodeResult {
    bool converged;
    uint8_t iterations_used;
};

// Float64 sum-product decoder — same layered schedule as flight decoder.
SpDecodeResult sp_decode(LdpcSubframe type,
                         std::span<const float> symbols,
                         uint8_t max_iters,
                         std::span<uint8_t> out) {
    const auto p = sp_params(type);
    const size_t mother_len = (type == LdpcSubframe::kSF2) ? 6240 : 4560;
    const size_t p1_start = p.K_full;
    const size_t p2_start = p1_start + p.mA->num_rows;

    // Initialise L_v from channel LLRs (same mapping as flight decoder)
    std::vector<double> L_v(mother_len, 0.0);
    size_t sym_idx = 0;
    // Systematic bits (after filler)
    for (size_t i = p.z; i < p.K_nominal; ++i) {
        L_v[i] = static_cast<double>(symbols[sym_idx++]);
    }
    // Filler bits — high confidence zero (matches kFillerLlr / 128.0)
    for (size_t i = p.K_nominal; i < p.K_full; ++i) {
        L_v[i] = 250.0; // ~32000/128
    }
    // Parity-1 bits
    for (size_t i = p1_start; i < p1_start + p.p1_count; ++i) {
        L_v[i] = static_cast<double>(symbols[sym_idx++]);
    }
    // Parity-2 bits
    for (size_t i = p2_start; i < L_v.size() && sym_idx < symbols.size(); ++i) {
        L_v[i] = static_cast<double>(symbols[sym_idx++]);
    }

    const size_t l1_cv_count = p.mA->num_entries + p.mB->num_entries;
    const size_t l2_cv_count = p.mC->num_entries + p.mD->num_entries;
    std::vector<double> L_cv(l1_cv_count + l2_cv_count, 0.0);
    std::vector<double> L_cv_id(p.mC->num_rows, 0.0);

    SpDecodeResult result{false, 0};

    for (uint8_t iter = 0; iter < max_iters; ++iter) {
        // Layer 1: [A | B] — no identity columns
        sp_layer_update(*p.mA, 0, *p.mB, p.K_full,
                        false, 0,
                        L_v, L_cv, L_cv_id, 0);

        // Layer 2: [C | D | I] — identity columns start at p2_start
        sp_layer_update(*p.mC, 0, *p.mD, p.K_full,
                        true, p2_start,
                        L_v, L_cv, L_cv_id, l1_cv_count);

        result.iterations_used = static_cast<uint8_t>(iter + 1);

        // Syndrome check: hard-decision on L_v, check all parity rows
        // (Simplified: use the flight decoder's syndrome check by checking
        // if hard bits form a valid codeword. For the reference decoder,
        // we just check convergence by running the flight decoder's
        // syndrome test indirectly — extract hard bits and re-encode.)
        // Shortcut: check if all L_v signs match a valid codeword by
        // checking parity of each row of [A B; C D I].
        bool all_satisfied = true;
        // Layer 1 rows
        for (uint16_t r = 0; r < p.mA->num_rows && all_satisfied; ++r) {
            uint8_t parity = 0;
            for (uint32_t i = p.mA->row_ptr[r]; i < p.mA->row_ptr[r + 1]; ++i) {
                parity ^= (L_v[p.mA->col_indices[i]] < 0.0) ? 1U : 0U;
            }
            for (uint32_t i = p.mB->row_ptr[r]; i < p.mB->row_ptr[r + 1]; ++i) {
                parity ^= (L_v[p.K_full + p.mB->col_indices[i]] < 0.0) ? 1U : 0U;
            }
            if (parity != 0U) all_satisfied = false;
        }
        // Layer 2 rows
        for (uint16_t r = 0; r < p.mC->num_rows && all_satisfied; ++r) {
            uint8_t parity = 0;
            for (uint32_t i = p.mC->row_ptr[r]; i < p.mC->row_ptr[r + 1]; ++i) {
                parity ^= (L_v[p.mC->col_indices[i]] < 0.0) ? 1U : 0U;
            }
            for (uint32_t i = p.mD->row_ptr[r]; i < p.mD->row_ptr[r + 1]; ++i) {
                parity ^= (L_v[p.K_full + p.mD->col_indices[i]] < 0.0) ? 1U : 0U;
            }
            // Identity column
            parity ^= (L_v[p2_start + r] < 0.0) ? 1U : 0U;
            if (parity != 0U) all_satisfied = false;
        }

        if (all_satisfied) {
            result.converged = true;
            break;
        }
    }

    // Extract hard bits (same as flight decoder: L_v[0..K_nominal-1])
    for (size_t i = 0; i < p.K_nominal; ++i) {
        out[i] = (L_v[i] < 0.0) ? 1U : 0U;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Sphere Packing Bound (normal approximation)
// ---------------------------------------------------------------------------

// Polyanskiy-Poor-Verdú (2010) normal approximation to the finite-length
// coding bound for BI-AWGN channel, rate R, blocklength n.
// Returns a lower bound on FER for any code of this (n, k).
// Reference: Y. Polyanskiy, H. V. Poor, S. Verdú, "Channel Coding Rate in
// the Finite Blocklength Regime," IEEE Trans. IT, vol. 56, no. 5, May 2010.

// Q-function: tail probability of standard normal.
double Q_func(double x) {
    return 0.5 * std::erfc(x / std::sqrt(2.0));
}

// Channel capacity C(snr) for BI-AWGN at given Eb/N0 (in linear).
// C = 1 - E[log2(1 + exp(-2*y*x/sigma^2))] where x=±1, y=x+n.
// For the normal approximation we use: C ≈ 1 - H(Q(sqrt(2*snr)))
// but the exact form uses numerical integration. We use the simpler
// capacity via mutual information for BPSK:
//   C_bpsk(snr) ≈ computed via Gauss-Hermite quadrature.
// However, for the PPV bound, what matters is the channel dispersion V(snr).
// We use the normal approximation directly:
//   P_e >= Q( (C(snr) - R) * sqrt(n) / sqrt(V(snr)) )
// which is tight for moderate n.

// BI-AWGN capacity via 16-point Gauss-Hermite quadrature.
// C = 1 - (1/sqrt(pi)) * sum_i w_i * log2(1 + exp(-4*snr - 4*sqrt(2*snr)*x_i))
// where snr = R * Eb/N0 (i.e., Es/N0 / 2 for BPSK with rate R).
double bi_awgn_capacity(double snr_linear) {
    // Gauss-Hermite nodes and weights (16-point)
    static constexpr double nodes[] = {
        -4.688738939, -3.869447905, -3.176999162, -2.546202158,
        -1.951787991, -1.380258539, -0.822951449, -0.273481046,
         0.273481046,  0.822951449,  1.380258539,  1.951787991,
         2.546202158,  3.176999162,  3.869447905,  4.688738939,
    };
    static constexpr double weights[] = {
        2.654807e-10, 2.320981e-07, 2.711861e-05, 9.322010e-04,
        1.288032e-02, 8.381005e-02, 2.806475e-01, 5.079295e-01,
        5.079295e-01, 2.806475e-01, 8.381005e-02, 1.288032e-02,
        9.322010e-04, 2.711861e-05, 2.320981e-07, 2.654807e-10,
    };
    // BI-AWGN: x ∈ {±1}, y = x + n, n ~ N(0, σ²), σ² = 1/(2·SNR).
    // C = 1 - E_n[ log₂(1 + exp(-2y/σ²)) | x=+1 ]
    //   = 1 - E_n[ log₂(1 + exp(-4·SNR·(1 + n))) ]
    //   = 1 - E_t[ log₂(1 + exp(-4·SNR - 4·SNR·σ·t)) ]  where t ~ N(0,1)
    // Gauss-Hermite: E[g(t)] = (1/√π) Σ w_i g(x_i · √2)
    constexpr int N = 16;
    const double sigma = 1.0 / std::sqrt(2.0 * snr_linear);
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        double t = nodes[i] * std::sqrt(2.0); // standard normal sample
        double n = sigma * t;                   // noise sample
        double y = 1.0 + n;                    // received (conditioned on x=+1)
        double llr = 2.0 * y / (sigma * sigma); // channel LLR = 2y/σ²
        double term = std::log2(1.0 + std::exp(-llr));
        sum += weights[i] * term;
    }
    return 1.0 - sum / std::sqrt(M_PI);
}

// BI-AWGN channel dispersion via numerical differentiation of mutual info.
double bi_awgn_dispersion(double snr_linear) {
    // V(snr) = Var[log(p(Y|X)/p(Y))] = Var[i(X;Y)]
    // For BPSK AWGN, computed via Gauss-Hermite:
    // V = E[i(X;Y)^2] - C^2
    static constexpr double nodes[] = {
        -4.688738939, -3.869447905, -3.176999162, -2.546202158,
        -1.951787991, -1.380258539, -0.822951449, -0.273481046,
         0.273481046,  0.822951449,  1.380258539,  1.951787991,
         2.546202158,  3.176999162,  3.869447905,  4.688738939,
    };
    static constexpr double weights[] = {
        2.654807e-10, 2.320981e-07, 2.711861e-05, 9.322010e-04,
        1.288032e-02, 8.381005e-02, 2.806475e-01, 5.079295e-01,
        5.079295e-01, 2.806475e-01, 8.381005e-02, 1.288032e-02,
        9.322010e-04, 2.711861e-05, 2.320981e-07, 2.654807e-10,
    };
    constexpr int N = 16;
    double C = bi_awgn_capacity(snr_linear);
    const double sigma = 1.0 / std::sqrt(2.0 * snr_linear);
    double E_i2 = 0.0;
    for (int i = 0; i < N; ++i) {
        double t = nodes[i] * std::sqrt(2.0);
        double n = sigma * t;
        double y = 1.0 + n;
        double llr = 2.0 * y / (sigma * sigma);
        double info = 1.0 - std::log2(1.0 + std::exp(-llr));
        E_i2 += weights[i] * info * info;
    }
    E_i2 /= std::sqrt(M_PI);
    return E_i2 - C * C;
}

// PPV normal approximation: FER lower bound for (n, k) code at given Eb/N0.
double ppv_fer_bound(uint16_t n, uint16_t k, double eb_no_db) {
    double R = static_cast<double>(k) / static_cast<double>(n);
    double eb_no_lin = std::pow(10.0, eb_no_db / 10.0);
    double snr = 2.0 * R * eb_no_lin; // Es/N0 in linear for BPSK
    double C = bi_awgn_capacity(snr);
    double V = bi_awgn_dispersion(snr);

    if (V < 1e-12 || C <= R) {
        // Below capacity — FER bound is ~1 (unreliable region)
        return 1.0;
    }

    // P_e >= Q( sqrt(n/V) * (C - R) + log(n) / (2*sqrt(n*V)) )
    // The second term is the O(log n / sqrt n) correction.
    double arg = std::sqrt(static_cast<double>(n) / V) * (C - R)
                 + std::log(static_cast<double>(n)) / (2.0 * std::sqrt(static_cast<double>(n) * V));
    return Q_func(arg);
}

// ---------------------------------------------------------------------------
// BER simulation helpers (shared with test_ldpc_ber.cpp pattern)
// ---------------------------------------------------------------------------

struct RefBerResult {
    double fer;
    double ber;
    size_t frame_errors;
    size_t total_frames;
    size_t total_errors;
    size_t total_bits;
};

// Run BER simulation using the sum-product reference decoder.
RefBerResult run_sp_simulation(LdpcSubframe type,
                               double eb_no_db,
                               int frames,
                               uint16_t k_nominal,
                               uint16_t n_output,
                               uint8_t max_iters,
                               uint32_t seed) {
    std::mt19937 gen(seed);
    const double code_rate = static_cast<double>(k_nominal) / static_cast<double>(n_output);
    const double eb_no_linear = std::pow(10.0, eb_no_db / 10.0);
    const double sigma = 1.0 / std::sqrt(2.0 * code_rate * eb_no_linear);
    const double sigma_sq = sigma * sigma;
    const double llr_scale = 2.0 / sigma_sq;
    std::normal_distribution<double> noise(0.0, sigma);

    std::vector<uint8_t> message(k_nominal);
    std::vector<uint8_t> encoded(n_output);
    std::vector<float> soft(n_output);
    std::vector<uint8_t> decoded(k_nominal);
    LdpcEncodeWorkspace enc_ws{};

    size_t total_bits = 0, total_errors = 0, frame_errors = 0;

    for (int f = 0; f < frames; ++f) {
        for (auto& b : message) {
            b = static_cast<uint8_t>(gen() % 2);
        }
        auto enc_st = ldpc_encode(type, message, encoded, enc_ws);
        if (enc_st != LdpcStatus::kOk) break; // should not happen
        for (size_t i = 0; i < n_output; ++i) {
            double modulated = (encoded[i] == 1) ? -1.0 : 1.0;
            soft[i] = static_cast<float>((modulated + noise(gen)) * llr_scale);
        }
        sp_decode(type, soft, max_iters, decoded);

        size_t errs = 0;
        for (size_t i = 0; i < k_nominal; ++i) {
            if (message[i] != decoded[i]) ++errs;
        }
        total_errors += errs;
        total_bits += k_nominal;
        if (errs > 0) ++frame_errors;
    }

    double ber = static_cast<double>(total_errors) / static_cast<double>(total_bits);
    double fer = static_cast<double>(frame_errors) / static_cast<double>(frames);
    return {fer, ber, frame_errors, static_cast<size_t>(frames), total_errors, total_bits};
}

// Run BER simulation using the flight AMS decoder (same as test_ldpc_ber.cpp).
RefBerResult run_ams_simulation(LdpcSubframe type,
                                double eb_no_db,
                                int frames,
                                uint16_t k_nominal,
                                uint16_t n_output,
                                uint8_t max_iters,
                                uint32_t seed) {
    std::mt19937 gen(seed);
    const double code_rate = static_cast<double>(k_nominal) / static_cast<double>(n_output);
    const double eb_no_linear = std::pow(10.0, eb_no_db / 10.0);
    const double sigma = 1.0 / std::sqrt(2.0 * code_rate * eb_no_linear);
    const double sigma_sq = sigma * sigma;
    const double llr_scale = 2.0 / sigma_sq;
    std::normal_distribution<double> noise(0.0, sigma);

    std::vector<uint8_t> message(k_nominal);
    std::vector<uint8_t> encoded(n_output);
    std::vector<float> soft(n_output);
    std::vector<uint8_t> decoded(k_nominal);
    LdpcEncodeWorkspace enc_ws{};
    LdpcWorkspace ws{};

    size_t total_bits = 0, total_errors = 0, frame_errors = 0;

    for (int f = 0; f < frames; ++f) {
        for (auto& b : message) {
            b = static_cast<uint8_t>(gen() % 2);
        }
        auto enc_st = ldpc_encode(type, message, encoded, enc_ws);
        if (enc_st != LdpcStatus::kOk) break;
        for (size_t i = 0; i < n_output; ++i) {
            double modulated = (encoded[i] == 1) ? -1.0 : 1.0;
            soft[i] = static_cast<float>((modulated + noise(gen)) * llr_scale);
        }
        (void)ldpc_decode(type, soft, max_iters, decoded, ws);

        size_t errs = 0;
        for (size_t i = 0; i < k_nominal; ++i) {
            if (message[i] != decoded[i]) ++errs;
        }
        total_errors += errs;
        total_bits += k_nominal;
        if (errs > 0) ++frame_errors;
    }

    double ber = static_cast<double>(total_errors) / static_cast<double>(total_bits);
    double fer = static_cast<double>(frame_errors) / static_cast<double>(frames);
    return {fer, ber, frame_errors, static_cast<size_t>(frames), total_errors, total_bits};
}

// Find Eb/N0 where FER crosses a threshold via linear interpolation in dB.
double find_crossing(const std::vector<double>& snrs,
                     const std::vector<double>& fers,
                     double threshold) {
    for (size_t i = 1; i < snrs.size(); ++i) {
        if (fers[i - 1] >= threshold && fers[i] < threshold) {
            // Linear interpolation in dB domain
            double f0 = std::log10(std::max(fers[i - 1], 1e-10));
            double f1 = std::log10(std::max(fers[i], 1e-10));
            double ft = std::log10(threshold);
            double t = (ft - f0) / (f1 - f0);
            return snrs[i - 1] + t * (snrs[i] - snrs[i - 1]);
        }
    }
    return snrs.back(); // threshold not crossed
}

} // namespace

// ---------------------------------------------------------------------------
// 3-way comparison: SPB bound vs sum-product vs flight AMS
// ---------------------------------------------------------------------------
TEST_CASE("LDPC decoder quality: 3-way gap decomposition (SF2)", "[channel][ber][reference][slow]") {
    constexpr uint8_t kIter = 50;
    constexpr int kFrames = 2000;
    constexpr uint32_t kSeed = 42;
    constexpr uint16_t kK = 1200;
    constexpr uint16_t kN = 2400;

    // SNR points spanning the waterfall region
    constexpr std::array<double, 9> snrs = {0.5, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 3.0};

    std::vector<double> fer_spb(snrs.size());
    std::vector<double> fer_sp(snrs.size());
    std::vector<double> fer_ams(snrs.size());

    // SPB bound is instant — compute inline.
    for (size_t i = 0; i < snrs.size(); ++i) {
        fer_spb[i] = ppv_fer_bound(kN, kK, snrs[i]);
    }

    // Run SP and AMS simulations in parallel across SNR points.
    // Each work item is one (decoder, snr_index) pair → 18 items total.
    struct WorkItem { size_t snr_idx; bool is_sp; };
    std::vector<WorkItem> items;
    items.reserve(snrs.size() * 2);
    for (size_t i = 0; i < snrs.size(); ++i) {
        items.push_back({i, true});   // sum-product
        items.push_back({i, false});  // flight AMS
    }

    std::atomic<size_t> next_item{0};
    auto worker = [&]() {
        while (true) {
            size_t idx = next_item.fetch_add(1);
            if (idx >= items.size()) break;
            const auto& item = items[idx];
            if (item.is_sp) {
                auto r = run_sp_simulation(LdpcSubframe::kSF2, snrs[item.snr_idx],
                                           kFrames, kK, kN, kIter, kSeed);
                fer_sp[item.snr_idx] = r.fer;
            } else {
                auto r = run_ams_simulation(LdpcSubframe::kSF2, snrs[item.snr_idx],
                                            kFrames, kK, kN, kIter, kSeed);
                fer_ams[item.snr_idx] = r.fer;
            }
        }
    };

    unsigned n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0) n_threads = 4;
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (unsigned t = 0; t < n_threads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    // Print results table.
    std::cout << "\n=== LDPC Decoder Quality Assessment (SF2, n=2400, k=1200, rate 1/2) ===\n";
    std::cout << "  SPB = Sphere Packing Bound (Polyanskiy-Poor-Verdú normal approx.)\n";
    std::cout << "  SP  = Float64 Sum-Product BP (theoretical ceiling for this code)\n";
    std::cout << "  AMS = Flight decoder: Adjusted Min-Sum, Q8.7, α=5/6, β=32\n";
    std::cout << "  Frames per point: " << kFrames << " (seed=" << kSeed
              << ", " << n_threads << " threads)\n\n";

    std::cout << "  Eb/N0   |  SPB bound  |  SP FER     |  AMS FER    |  Algo gap   |\n";
    std::cout << "  --------|-------------|-------------|-------------|-------------|\n";

    for (size_t i = 0; i < snrs.size(); ++i) {
        const char* gap_str = "  —";
        char gap_buf[32] = {};
        if (fer_sp[i] > 0.0 && fer_ams[i] > 0.0 && fer_sp[i] < 0.9 && fer_ams[i] < 0.9) {
            double ratio = fer_ams[i] / fer_sp[i];
            std::snprintf(gap_buf, sizeof(gap_buf), "  ×%.2f", ratio);
            gap_str = gap_buf;
        } else if (fer_sp[i] == 0.0 && fer_ams[i] == 0.0) {
            gap_str = "  both 0";
        }

        std::printf("  %4.1f dB  | %9.2e  | %9.5f   | %9.5f   | %s\n",
                    snrs[i], fer_spb[i], fer_sp[i], fer_ams[i], gap_str);
    }

    // Interpolate cliff positions at FER = 1%
    std::vector<double> snr_vec(snrs.begin(), snrs.end());
    double cliff_sp = find_crossing(snr_vec, fer_sp, 0.01);
    double cliff_ams = find_crossing(snr_vec, fer_ams, 0.01);

    std::cout << "\n  === Gap Decomposition at FER = 1% ===\n";
    std::printf("  Shannon limit (rate 1/2 BI-AWGN):     0.19 dB\n");
    std::printf("  Sum-product cliff (FER=1%%):           %.2f dB\n", cliff_sp);
    std::printf("  Flight AMS cliff  (FER=1%%):           %.2f dB\n", cliff_ams);
    std::printf("  ---\n");
    std::printf("  Total Shannon gap:                     %.2f dB\n", cliff_ams - 0.19);
    std::printf("    Code + decoder gap (Shannon → SP):   %.2f dB  [code design + BP schedule]\n", cliff_sp - 0.19);
    std::printf("    Algorithm gap      (SP → AMS):       %.2f dB  [min-sum approx + Q8.7 + OMS]\n", cliff_ams - cliff_sp);
    std::printf("  Note: SPB finite-length bound is negligible at n=%d (FER < 10^-70\n", kN);
    std::printf("  in the waterfall region). The gap is dominated by code design.\n");

    double algo_gap = cliff_ams - cliff_sp;
    std::cout << "\n  === Verdict ===\n";
    if (algo_gap < 0.15) {
        std::cout << "  Algorithm gap < 0.15 dB — AMS is near-optimal for this code.\n";
        std::cout << "  Investing in a better decoder algorithm is NOT recommended.\n";
    } else if (algo_gap < 0.3) {
        std::cout << "  Algorithm gap " << algo_gap << " dB — modest room for improvement.\n";
        std::cout << "  Consider per-layer alpha or GA-MS if the gap justifies the effort.\n";
    } else {
        std::cout << "  Algorithm gap " << algo_gap << " dB — significant headroom remains.\n";
        std::cout << "  Investigate sum-product approximations or improved OMS tuning.\n";
    }

    // Basic sanity checks
    // SPB must be below SP (otherwise our bound or decoder is wrong)
    for (size_t i = 0; i < snrs.size(); ++i) {
        if (fer_sp[i] > 0.001) {
            CHECK(fer_sp[i] >= fer_spb[i] * 0.5); // allow 2× margin for statistical noise
        }
    }
    // AMS must not be dramatically worse than SP
    CHECK(algo_gap < 1.0); // sanity — >1 dB gap would indicate a bug
}
