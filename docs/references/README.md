# Reference documents

This directory holds the normative source material LunaLink implements
against. Nothing here is LunaLink's own work, and nothing here is covered
by LunaLink's Apache-2.0 licence. Each file is reproduced from a public
NASA release so that a clone of this repository can be built, tested and
independently audited offline.

## Provenance

All files originate from NASA's [LunaNet Interoperability Specification][lnis]
release, which publishes the Signal-In-Space Recommended Standard for the
Augmented Forward Signal (LSIS V1.0, 29 January 2025) together with its data
annexes. Works prepared by officers or employees of the United States
Government as part of their official duties are not subject to copyright
protection in the United States (17 U.S.C. § 105).

| File | Origin |
|------|--------|
| `lunanet-signal-in-space-recommended-standard-augmented-forward-signal-vol-a.pdf` | LSIS V1.0 Volume A, the specification itself |
| `002_LNIS-AD1-Vol-A-Annex2-LDPC-Submatrices-CSV-README.pdf` | Annex 2 explanatory README |
| `003*.csv`, `004*.csv` | Annex 2 LDPC submatrices, index and matrix forms |
| `005_LNIS-AD1-Vol-A-Annex3-PRN-Spreading-Codes-README.pdf` | Annex 3 explanatory README |
| `006_GoldCode2046hex210prns.txt` | Annex 3 Gold-2046 code set, PRN 1-210 |
| `007_l1cp_hex210prns.txt` | Annex 3 Weil-10230 primary code set, PRN 1-210 |
| `008_Weil1500hex210prns.txt` | Annex 3 Weil-1500 tertiary code set, PRN 1-210 |
| `README.pdf` | Release-package README accompanying the annexes |
| `overview.txt` | Non-normative summary of the standard's scope |

`spec_tables/` is different: those CSV files are **transcriptions** made by
this project of tables printed in the specification PDF, produced so the
tables can be diffed and tested against. They are derived work, they carry
transcription risk, and `spec_tables/README.md` records which table each one
came from.

## Verifying these copies

The specification PDF in this directory is byte-identical to the file NASA
publishes. `verify_references.sh` re-downloads it and compares:

```bash
bash docs/references/verify_references.sh
```

Expected SHA-256 of the specification:

```
986e07959f527d24280d87b5477298424ffdf754c4625f08041b5749592e61b6
```

If that check ever fails, NASA has reissued the document and this repository
is behind. Open an issue rather than silently replacing the file, because the
implementation is traced clause by clause against the version recorded in
`docs/governance/baseline_register.rst`.

## Certification standards are not vendored

The ECSS and NASA software assurance standards that
`docs/assurance/` cites are **not** stored here. They are freely downloadable
but their redistribution terms differ from the specification's, so
`docs/standards/download_standards.sh` fetches them on demand instead.
MISRA C++:2023 is a paid document and is never redistributed by this project;
`docs/assurance/c_plus_plus_guidelines.rst` cites rule numbers only.

[lnis]: https://www.nasa.gov/directorates/somd/space-communications-navigation-program/lunanet-interoperability-specification/
