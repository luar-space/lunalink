#!/usr/bin/env python3
"""V1 demo: Gold-2046 chip sequence and power spectrum for AFS-I.

Generates the AFS-I BPSK(1) signal for PRN 1, plots the chip sequence
(first 100 chips) and the power spectral density showing the expected
1.023 MHz null-to-null bandwidth.
"""

from __future__ import annotations

import numpy as np

import lunalink.afs as afs


def main() -> None:
    """Generate and plot the AFS-I signal for PRN 1."""
    import matplotlib.pyplot as plt

    chips = afs.prn_code(1)
    samples = afs.modulate_i(chips, 1)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))

    ax1.plot(samples[:100])
    ax1.set_title("AFS-I chip sequence (first 100 chips)")
    ax1.set_xlabel("Chip index")
    ax1.set_ylabel("Amplitude")
    ax1.set_ylim(-1.5, 1.5)

    freqs = np.fft.fftshift(np.fft.fftfreq(len(samples), 1 / 1.023e6))
    psd = np.abs(np.fft.fftshift(np.fft.fft(samples.astype(float)))) ** 2
    ax2.plot(freqs / 1e6, 10 * np.log10(psd + 1e-12))
    ax2.set_xlabel("Frequency (MHz)")
    ax2.set_ylabel("Power (dB)")
    ax2.set_title("Power spectrum — null-to-null 1.023 MHz")

    plt.tight_layout()
    plt.savefig("demo/v1_signal.png", dpi=150)
    print("Saved demo/v1_signal.png")
    plt.show()


if __name__ == "__main__":
    main()
