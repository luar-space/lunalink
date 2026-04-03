#!/usr/bin/env python3
"""Generate publication-quality figures for the LDPC algorithm card.

Usage:
    python scripts/plot_algo_card.py results.json outdir/
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

# ---------------------------------------------------------------------------
# Style
# ---------------------------------------------------------------------------


def setup_style() -> None:
    """Configure matplotlib for publication-quality output."""
    plt.rcParams.update(
        {
            "figure.figsize": (8, 5),
            "figure.dpi": 150,
            "savefig.dpi": 150,
            "font.size": 11,
            "axes.titlesize": 13,
            "axes.labelsize": 12,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "legend.fontsize": 10,
            "lines.linewidth": 1.8,
            "lines.markersize": 6,
            "axes.grid": True,
            "grid.alpha": 0.3,
            "axes.spines.top": False,
            "axes.spines.right": False,
        }
    )


SHANNON_LIMIT = 0.19  # dB, rate-1/2 BI-AWGN


# ---------------------------------------------------------------------------
# Figures
# ---------------------------------------------------------------------------


def plot_waterfall(data: dict, key: str, title: str, outdir: Path) -> None:
    """FER and BER vs Eb/N0 waterfall curve."""
    points = data[key]
    snrs = [p["eb_n0_db"] for p in points]
    fers = [p["fer"] for p in points]
    bers = [p["ber"] for p in points]
    cis = [p["ci_fer"] for p in points]

    fig, ax = plt.subplots()

    # FER with CI error bars
    fer_nonzero = [(s, f, c) for s, f, c in zip(snrs, fers, cis) if f > 0]
    if fer_nonzero:
        s_nz, f_nz, c_nz = zip(*fer_nonzero)
        ax.errorbar(
            s_nz,
            f_nz,
            yerr=c_nz,
            fmt="o-",
            color="#2563eb",
            label="FER",
            capsize=3,
            zorder=3,
        )

    # BER
    ber_nonzero = [(s, b) for s, b in zip(snrs, bers) if b > 0]
    if ber_nonzero:
        s_nz, b_nz = zip(*ber_nonzero)
        ax.plot(s_nz, b_nz, "s--", color="#dc2626", label="BER", zorder=3)

    # Uncoded BPSK reference: BER = Q(sqrt(2 * Eb/N0))
    snr_fine = np.linspace(0.0, max(snrs), 200)
    import math

    uncoded_ber = np.array(
        [0.5 * math.erfc(math.sqrt(10 ** (s / 10))) for s in snr_fine]
    )
    ax.plot(
        snr_fine,
        uncoded_ber,
        "-",
        color="#a3a3a3",
        linewidth=1.0,
        label="Uncoded BPSK",
        zorder=1,
    )

    # Shannon limit
    ax.axvline(
        SHANNON_LIMIT,
        color="#16a34a",
        linestyle=":",
        linewidth=1.2,
        label=f"Shannon limit ({SHANNON_LIMIT} dB)",
        zorder=1,
    )

    # Spec requirement
    ax.axhline(
        1e-5,
        color="#9333ea",
        linestyle=":",
        linewidth=1.0,
        label=r"Spec: BER < $10^{-5}$",
        zorder=1,
    )

    ax.set_yscale("log")
    ax.set_xlabel(r"$E_b/N_0$ (dB)")
    ax.set_ylabel("Error Rate")
    ax.set_title(title)
    ax.legend(loc="upper right", fontsize=8)
    ax.set_ylim(bottom=1e-7, top=1.0)
    ax.set_xlim(left=0.0)

    fname = key + ".png"
    fig.savefig(outdir / fname)
    plt.close(fig)
    print(f"  {fname}")


def plot_waterfall_comparison(data: dict, outdir: Path) -> None:
    """SF2 vs SF3 waterfall overlay."""
    fig, ax = plt.subplots()

    for key, label, color, marker in [
        ("waterfall_sf2", "SF2 (n=2400, k=1200)", "#2563eb", "o"),
        ("waterfall_sf3", "SF3 (n=1740, k=870)", "#dc2626", "s"),
    ]:
        points = data[key]
        snrs = [p["eb_n0_db"] for p in points]
        fers = [p["fer"] for p in points]
        nonzero = [(s, f) for s, f in zip(snrs, fers) if f > 0]
        if nonzero:
            s_nz, f_nz = zip(*nonzero)
            ax.plot(s_nz, f_nz, f"{marker}-", color=color, label=label)

    ax.axvline(
        SHANNON_LIMIT,
        color="#16a34a",
        linestyle=":",
        linewidth=1.2,
        label=f"Shannon limit ({SHANNON_LIMIT} dB)",
    )

    ax.set_yscale("log")
    ax.set_xlabel(r"$E_b/N_0$ (dB)")
    ax.set_ylabel("Frame Error Rate (FER)")
    ax.set_title("Waterfall Comparison: SF2 vs SF3")
    ax.legend()
    ax.set_ylim(bottom=1e-6, top=1.0)
    ax.set_xlim(left=0.0)

    fig.savefig(outdir / "waterfall_comparison.png")
    plt.close(fig)
    print("  waterfall_comparison.png")


def plot_convergence(data: dict, outdir: Path) -> None:
    """Convergence CDF: P(converge | iter <= N)."""
    points = data["convergence_cdf"]
    iters = [p["max_iters"] for p in points]
    # P(converge) = 1 - FER (at that max_iter setting)
    p_conv = [1.0 - p["fer"] for p in points]

    fig, ax = plt.subplots()
    ax.step(iters, p_conv, where="post", color="#2563eb", linewidth=2)
    ax.fill_between(iters, p_conv, step="post", alpha=0.15, color="#2563eb")

    # Annotate knee
    for i, (it, pc) in enumerate(zip(iters, p_conv)):
        if pc >= 0.95 and (i == 0 or p_conv[i - 1] < 0.95):
            ax.annotate(
                f"95% @ {it} iter",
                xy=(it, pc),
                xytext=(it + 5, pc - 0.08),
                arrowprops={"arrowstyle": "->", "color": "#666"},
                fontsize=10,
                color="#666",
            )
            break

    # Mean iterations via E[X] = Σ_{k=1}^{max} (1 - F(k)).
    # Interpolate the sampled CDF to every integer for correct summation.
    cdf_interp = np.interp(range(1, max(iters) + 1), iters, p_conv)
    mean_iter = sum(1.0 - f for f in cdf_interp)
    ax.axvline(
        mean_iter,
        color="#dc2626",
        linestyle="--",
        linewidth=1.0,
        label=f"Mean ≈ {mean_iter:.0f} iterations",
    )

    ax.set_xlabel("Maximum Iterations")
    ax.set_ylabel("P(converge)")
    ax.set_title(r"Convergence CDF — SF2 at $E_b/N_0$ = 1.5 dB")
    ax.set_ylim(0, 1.05)
    ax.set_xlim(0, 55)
    ax.legend()

    fig.savefig(outdir / "convergence_cdf.png")
    plt.close(fig)
    print("  convergence_cdf.png")


def plot_loss_budget(data: dict, outdir: Path) -> None:
    """Plot implementation loss budget bar chart."""
    loss = data["loss_budget"]

    configs = [
        ("Baseline\n(float64 LLR)", "loss_baseline"),
        ("int8 LLR\nquantisation", "loss_int8"),
        ("int4 LLR\nquantisation", "loss_int4"),
        ("15 iterations\n(vs 50)", "loss_15iter"),
    ]

    labels = [c[0] for c in configs]
    fers = [loss[c[1]]["fer"] for c in configs]
    cis = [loss[c[1]]["ci_fer"] for c in configs]
    colors = ["#2563eb", "#60a5fa", "#f97316", "#dc2626"]

    fig, ax = plt.subplots(figsize=(8, 4.5))
    bars = ax.barh(labels, fers, xerr=cis, color=colors, capsize=4, height=0.6)

    # Annotate with FER values
    for bar, fer, ci in zip(bars, fers, cis):
        ax.text(
            fer + ci + 0.001,
            bar.get_y() + bar.get_height() / 2,
            f"FER = {fer:.4f}",
            va="center",
            fontsize=9,
        )

    ax.set_xlabel(r"Frame Error Rate at $E_b/N_0$ = 1.2 dB")
    ax.set_title("Implementation Loss Budget — SF2")
    ax.set_xlim(0, max(fers) * 1.5)

    fig.savefig(outdir / "loss_budget.png")
    plt.close(fig)
    print("  loss_budget.png")


def plot_error_patterns(data: dict, outdir: Path) -> None:
    """Histogram of bit errors per failed frame at multiple SNR points."""
    ep_data = data["error_patterns"]

    # Handle both old (dict) and new (list) formats
    if isinstance(ep_data, dict):
        ep_list = [ep_data]
    else:
        ep_list = ep_data

    # Filter to points that have errors
    ep_with_errors = [ep for ep in ep_list if ep.get("raw_errors")]
    if not ep_with_errors:
        print("  error_patterns.png — SKIPPED (no errors at any SNR)")
        return

    n_plots = len(ep_with_errors)
    fig, axes = plt.subplots(1, n_plots, figsize=(5 * n_plots, 4.5), squeeze=False)

    for i, ep in enumerate(ep_with_errors):
        ax = axes[0][i]
        raw = ep["raw_errors"]
        k_half = ep["k_nominal"] // 2

        ax.hist(raw, bins=40, color="#2563eb", alpha=0.8, edgecolor="#1e40af")
        ax.axvline(
            k_half,
            color="#dc2626",
            linestyle="--",
            linewidth=1.5,
            label=f"k/2 = {k_half}",
        )

        snr = ep["eb_n0_db"]
        ax.set_xlabel("Bit Errors per Failed Frame")
        if i == 0:
            ax.set_ylabel("Count")
        ax.set_title(rf"$E_b/N_0$ = {snr} dB")
        ax.legend(loc="upper right", fontsize=8)

        # Annotate
        max_errs = ep["max_bit_errors"]
        near_miss = sum(1 for e in raw if e < k_half * 0.5)
        ax.text(
            0.03,
            0.95,
            f"Failed: {ep['frame_errors']}/{ep['total_frames']}\n"
            f"Max: {max_errs}\n"
            f"Near-miss (<{k_half // 2}): {near_miss}",
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=8,
            bbox={"boxstyle": "round", "facecolor": "wheat", "alpha": 0.8},
        )

    fig.suptitle("Bit-Error Pattern Analysis — SF2", fontsize=13, y=1.02)
    fig.tight_layout()
    fig.savefig(outdir / "error_patterns.png")
    plt.close(fig)
    print("  error_patterns.png")


def plot_shannon_gap(data: dict, outdir: Path) -> None:
    """Annotated Shannon gap decomposition diagram."""
    # Find FER=1% cliff from SF2 waterfall
    points = data["waterfall_sf2"]
    snrs = [p["eb_n0_db"] for p in points]
    fers = [p["fer"] for p in points]

    cliff = snrs[-1]  # default
    for i in range(1, len(snrs)):
        if fers[i - 1] >= 0.01 and fers[i] < 0.01:
            # Linear interpolation in log domain
            f0 = np.log10(max(fers[i - 1], 1e-10))
            f1 = np.log10(max(fers[i], 1e-10))
            ft = np.log10(0.01)
            t = (ft - f0) / (f1 - f0)
            cliff = snrs[i - 1] + t * (snrs[i] - snrs[i - 1])
            break

    fig, ax = plt.subplots(figsize=(10, 3.2))
    ax.set_xlim(-0.3, cliff + 0.8)
    ax.set_ylim(-0.5, 1.8)
    ax.axis("off")

    # Number line
    ax.plot([0, cliff + 0.5], [0.5, 0.5], "k-", linewidth=2)

    # Shannon limit
    ax.plot(SHANNON_LIMIT, 0.5, "o", color="#16a34a", markersize=12, zorder=5)
    ax.annotate(
        f"Shannon\n{SHANNON_LIMIT} dB",
        xy=(SHANNON_LIMIT, 0.5),
        xytext=(SHANNON_LIMIT, -0.2),
        ha="center",
        fontsize=9,
        color="#16a34a",
        arrowprops={"arrowstyle": "-", "color": "#16a34a"},
    )

    # SP cliff
    ax.plot(cliff, 0.5, "o", color="#2563eb", markersize=12, zorder=5)
    ax.annotate(
        f"Sum-Product\n{cliff:.2f} dB",
        xy=(cliff, 0.5),
        xytext=(cliff, -0.2),
        ha="center",
        fontsize=9,
        color="#2563eb",
        arrowprops={"arrowstyle": "-", "color": "#2563eb"},
    )

    # Gap annotation
    mid = (SHANNON_LIMIT + cliff) / 2
    gap = cliff - SHANNON_LIMIT
    ax.annotate(
        "",
        xy=(cliff, 0.8),
        xytext=(SHANNON_LIMIT, 0.8),
        arrowprops={"arrowstyle": "<->", "color": "#333", "linewidth": 1.5},
    )
    ax.text(
        mid,
        1.05,
        f"Total gap: {gap:.2f} dB\n(code design + BP schedule)",
        ha="center",
        fontsize=10,
        fontweight="bold",
    )

    ax.text(
        0.5,
        0.97,
        "Shannon Gap Decomposition — Rate-1/2 BPSK AWGN",
        ha="center",
        fontsize=12,
        transform=ax.transAxes,
        va="top",
    )

    fig.savefig(outdir / "shannon_gap.png")
    plt.close(fig)
    print("  shannon_gap.png")


def plot_quantisation(data: dict, outdir: Path) -> None:
    """Quantisation sweep: FER vs LLR precision."""
    points = data["quantisation"]

    configs = []
    fers = []
    cis = []
    for p in points:
        configs.append(p["config"])
        fers.append(p["fer"])
        cis.append(p["ci_fer"])

    fig, ax = plt.subplots(figsize=(7, 4))
    colors = ["#f97316", "#f59e0b", "#eab308", "#84cc16", "#22c55e", "#2563eb"]
    bars = ax.bar(
        configs,
        fers,
        yerr=cis,
        color=colors[: len(configs)],
        capsize=4,
        width=0.6,
        edgecolor="#333",
        linewidth=0.5,
    )

    for bar, fer in zip(bars, fers):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.002,
            f"{fer:.4f}",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    ax.set_xlabel("LLR Quantisation")
    ax.set_ylabel(r"FER at $E_b/N_0$ = 1.2 dB")
    ax.set_title("Quantisation Impact on Decoder Performance — SF2")

    fig.savefig(outdir / "quantisation_sweep.png")
    plt.close(fig)
    print("  quantisation_sweep.png")


# ---------------------------------------------------------------------------
# RST table generation
# ---------------------------------------------------------------------------


def generate_rst_tables(data: dict, outdir: Path) -> None:
    """Generate RST table fragments for inclusion in the algo card."""
    # Waterfall table
    for key in ("waterfall_sf2", "waterfall_sf3"):
        points = data[key]
        lines = [
            f".. list-table:: {key.replace('_', ' ').title()} Waterfall",
            "   :header-rows: 1",
            "   :widths: 12 12 12 12 12",
            "",
            "   * - Eb/N0 (dB)",
            "     - FER",
            "     - 95% CI",
            "     - BER",
            "     - Frames",
        ]
        for p in points:
            fer_str = f"{p['fer']:.6f}" if p["fer"] > 0 else "**0**"
            ber_str = f"{p['ber']:.2e}" if p["ber"] > 0 else "**0**"
            lines.append(f"   * - {p['eb_n0_db']:.1f}")
            lines.append(f"     - {fer_str}")
            lines.append(f"     - {p['ci_fer']:.6f}")
            lines.append(f"     - {ber_str}")
            lines.append(f"     - {p['frames']}")

        (outdir / f"{key}_table.rst.inc").write_text("\n".join(lines) + "\n")

    # Error floor table
    lines = [
        ".. list-table:: Error Floor Probe",
        "   :header-rows: 1",
        "   :widths: 15 12 12 15 15",
        "",
        "   * - Code / SNR",
        "     - FER",
        "     - Frame Errors",
        "     - Frames",
        "     - FER Upper Bound (95%)",
    ]
    for label, result in data["error_floor"].items():
        fer_str = f"{result['fer']:.2e}" if result["fer"] > 0 else "**0**"
        lines.append(f"   * - {label.replace('_', ' ')}")
        lines.append(f"     - {fer_str}")
        lines.append(f"     - {result['frame_errors']}")
        lines.append(f"     - {result['frames']}")
        lines.append(f"     - {result['ci_fer_upper']:.2e}")

    (outdir / "error_floor_table.rst.inc").write_text("\n".join(lines) + "\n")

    # Symmetry table
    sym = data["symmetry"]
    lines = [
        ".. list-table:: Codeword Symmetry Validation",
        "   :header-rows: 1",
        "   :widths: 20 15 15 15",
        "",
        "   * - Configuration",
        "     - FER",
        "     - 95% CI",
        "     - Frames",
        "   * - Random messages",
        f"     - {sym['random']['fer']:.6f}",
        f"     - {sym['random']['ci_fer']:.6f}",
        f"     - {sym['random']['frames']}",
        "   * - All-zero message",
        f"     - {sym['allzero']['fer']:.6f}",
        f"     - {sym['allzero']['ci_fer']:.6f}",
        f"     - {sym['allzero']['frames']}",
    ]
    (outdir / "symmetry_table.rst.inc").write_text("\n".join(lines) + "\n")

    print("  RST table fragments generated")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    """Generate all algorithm card figures from JSON simulation results."""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <results.json> <output_dir>")
        sys.exit(1)

    json_path = Path(sys.argv[1])
    outdir = Path(sys.argv[2])
    outdir.mkdir(parents=True, exist_ok=True)

    with open(json_path) as f:
        data = json.load(f)

    setup_style()

    print(f"Generating figures from {json_path} → {outdir}/")
    plot_waterfall(
        data, "waterfall_sf2", "LDPC Waterfall — SF2 (n=2400, k=1200)", outdir
    )
    plot_waterfall(
        data, "waterfall_sf3", "LDPC Waterfall — SF3 (n=1740, k=870)", outdir
    )
    plot_waterfall_comparison(data, outdir)
    plot_convergence(data, outdir)
    plot_loss_budget(data, outdir)
    plot_error_patterns(data, outdir)
    plot_shannon_gap(data, outdir)
    plot_quantisation(data, outdir)
    generate_rst_tables(data, outdir)
    print("Done.")


if __name__ == "__main__":
    main()
