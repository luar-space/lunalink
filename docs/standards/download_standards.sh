#!/usr/bin/env bash
# Download ECSS and NASA software certification standards (PDFs).
# Run from repo root:  bash docs/standards/download_standards.sh

set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ESA_DIR="$DIR/esa"
NASA_DIR="$DIR/nasa"
mkdir -p "$ESA_DIR" "$NASA_DIR"

# ── ECSS standards ──────────────────────────────────────────────────
ecss_urls=(
  "https://ecss.nl/wp-content/uploads/2025/05/ECSS-E-ST-40C-Rev.1(30April2025).pdf"
  "https://ecss.nl/wp-content/uploads/2025/05/ECSS-Q-ST-80C-Rev.2(30April2025).pdf"
  "https://ecss.nl/wp-content/uploads/handbooks/ecss-e-hb/ECSS-E-HB-40A11December2013.pdf"
  "https://ecss.nl/wp-content/uploads/2020/04/ECSS-E-HB-40-01A(7April2020).pdf"
  "https://ecss.nl/wp-content/uploads/2017/11/ECSS-Q-HB-80-03A-Rev.1(20November2017).pdf"
  "https://ecss.nl/wp-content/uploads/handbooks/ecss-q-hb/ECSS-Q-HB-80-04A30March2011.pdf"
)

# ── NASA standards ──────────────────────────────────────────────────
nasa_urls=(
  "https://nodis3.gsfc.nasa.gov/npg_img/N_PR_7150_002D_/N_PR_7150_002D_.pdf"
  "https://standards.nasa.gov/sites/default/files/standards/NASA/B/0/NASA-STD-87398-Revision-B.pdf"
  "https://standards.nasa.gov/sites/default/files/standards/NASA/B/1/NASA-STD-7009B-Final-3-5-2024.pdf"
  "https://standards.nasa.gov/sites/default/files/standards/NASA/A/0/nasa-hdbk-7009a.pdf"
)

# Files whose server name differs from the desired local name
declare -A renames=(
  ["ECSS-E-HB-40A11December2013.pdf"]="ECSS-E-HB-40A(11December2013).pdf"
  ["ECSS-Q-HB-80-04A30March2011.pdf"]="ECSS-Q-HB-80-04A(30March2011).pdf"
  ["N_PR_7150_002D_.pdf"]="NPR-7150.2D.pdf"
  ["NASA-STD-87398-Revision-B.pdf"]="NASA-STD-8739.8B.pdf"
  ["NASA-STD-7009B-Final-3-5-2024.pdf"]="NASA-STD-7009B.pdf"
  ["nasa-hdbk-7009a.pdf"]="NASA-HDBK-7009A.pdf"
)

fetch() {
  local url="$1"
  local out_dir="$2"
  local server_name
  server_name="$(basename "$url")"
  local local_name="${renames[$server_name]:-$server_name}"
  local dest="$out_dir/$local_name"
  if [ -f "$dest" ]; then
    echo "  exists: $local_name"
  else
    echo "  fetch:  $local_name"
    curl -fSL -o "$dest" "$url"
  fi
}

echo "=== ESA / ECSS Standards ==="
for url in "${ecss_urls[@]}"; do fetch "$url" "$ESA_DIR"; done

echo ""
echo "=== NASA Standards ==="
for url in "${nasa_urls[@]}"; do fetch "$url" "$NASA_DIR"; done

esa_count=$(ls "$ESA_DIR"/*.pdf 2>/dev/null | wc -l | tr -d ' ')
nasa_count=$(ls "$NASA_DIR"/*.pdf 2>/dev/null | wc -l | tr -d ' ')
echo ""
echo "Done. ${esa_count} ECSS PDFs in esa/, ${nasa_count} NASA PDFs in nasa/"
