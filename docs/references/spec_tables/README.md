# LSIS-AFS Specification Tables

Machine-readable CSV files containing the parameter tables and constants
defined in the LSIS-AFS specification. These correspond to the appendix
tables and key parameter definitions from the specification document.

## Code Generation Parameters

| File | Spec Reference | Description |
|------|---------------|-------------|
| `appendix_c_gold_g2_delays.csv` | Appendix C, Tables C-1..C-5 | Gold code G2 LFSR delays for PRN 1-210 |
| `appendix_d_weil_primary_params.csv` | Appendix D, Tables D-2..D-6 | Weil primary code (k, p) for PRN 1-210 |
| `appendix_e_weil_tertiary_params.csv` | Appendix E, Tables E-2..E-6 | Weil tertiary code index k for PRN 1-210 |
| `table_10_secondary_codes.csv` | Table 10 | Secondary codes S0-S3 (4 chips each) |
| `table_11_code_assignments.csv` | Table 11 | Interim LNSP node-to-PRN assignments |

## Frame and Message Structure

| File | Spec Reference | Description |
|------|---------------|-------------|
| `table_12_sync_pattern.csv` | Table 12 | 68-symbol synchronization pattern |
| `table_14_frame_structure.csv` | Table 14 | Frame structure dimensions and constants |
| `sb2_bit_layout.csv` | Section 2.4.3.1.6 | Subframe 2 bit field layout (1176 bits) |

## Encoding and Signal Parameters

| File | Spec Reference | Description |
|------|---------------|-------------|
| `encoding_parameters.csv` | Sections 2.4.2, 2.4.3 | BCH, CRC-24, and LDPC parameters |
| `signal_parameters.csv` | Section 2.3 | Chip rates, modulation, code lengths |
| `time_system_parameters.csv` | Section 2.5.1 | Time system constants and ToT formula |
| `file_format_headers.csv` | — | Binary file format magic numbers and headers |

## Usage

Any LSIS-AFS implementation can parse these CSVs to obtain the specification
parameters needed for code generation, message encoding, and signal processing.
All comment lines begin with `#`.
