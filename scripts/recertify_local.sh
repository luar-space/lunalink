#!/usr/bin/env bash
# =============================================================================
# recertify_local.sh — Local Recertification Runner
# =============================================================================
#
# Runs all assurance gates sequentially, collects PASS/FAIL verdicts for each,
# calls gen_recertification_record.py, and writes
# docs/assurance/recertification_record.rst.
#
# Usage:
#   bash scripts/recertify_local.sh [--milestone PDR] [--reason "..."] [--authority "..."]
#
# Environment overrides (alternative to flags):
#   MILESTONE, REASON, AUTHORITY, PROJECT
#
# The MC/DC coverage gate is auto-detected — it runs if LLVM is installed
# locally, otherwise SKIPPED.  All gates run regardless of each other's outcome.
# Exit code is 1 if any gate FAILs; 0 if all PASS or only SKIP.
#
# Reuse:
#   Copy this script and gen_recertification_record.py to any project.
#   Set GATE_COMMANDS and GATE_LABELS to match your gate set.
# =============================================================================

set -uo pipefail

# ---------------------------------------------------------------------------
# Repo root — works regardless of caller's working directory
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ---------------------------------------------------------------------------
# Colour output (degrades gracefully in non-TTY / CI environments)
# ---------------------------------------------------------------------------
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    RED="$(tput setaf 1)"
    GREEN="$(tput setaf 2)"
    YELLOW="$(tput setaf 3)"
    CYAN="$(tput setaf 6)"
    BOLD="$(tput bold)"
    RESET="$(tput sgr0)"
else
    RED="" GREEN="" YELLOW="" CYAN="" BOLD="" RESET=""
fi

# ---------------------------------------------------------------------------
# Argument parsing (flags override env-var defaults)
# ---------------------------------------------------------------------------
MILESTONE="${MILESTONE:-Periodic}"
REASON="${REASON:-Local recertification run}"
AUTHORITY="${AUTHORITY:-}"
PROJECT="${PROJECT:-LunaLink}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --milestone)  MILESTONE="$2";  shift 2 ;;
        --reason)     REASON="$2";     shift 2 ;;
        --authority)  AUTHORITY="$2";  shift 2 ;;
        --project)    PROJECT="$2";    shift 2 ;;
        *)
            echo "Unknown argument: $1" >&2
            echo "Usage: $0 [--milestone M] [--reason R] [--authority A] [--project P]" >&2
            exit 1
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Gate definitions
# GATE_KEYS  — canonical IDs (must match --gate args passed to generator)
# GATE_LABELS — human labels including spec refs in parentheses
# GATE_SKIP   — space-separated keys to always mark SKIPPED (no command run)
#
# To adapt to another project: update these three variables and GATE_COMMANDS.
# ---------------------------------------------------------------------------
GATE_KEYS=(
    lint
    test-cpp
    swifi
    sanitize
    coverage-cpp
    coverage-mcdc
    tidy
    wcet
    test-python
    docs
)

GATE_LABELS=(
    "Lint / Static Analysis (SWE-108, SWE-117)"
    "C++ Unit Tests (SWE-127)"
    "SWIFI Fault Injection (SW-SAF-06)"
    "Sanitizers ASan+UBSan (SWE-128)"
    "C++ Line Coverage (SWE-127)"
    "MC/DC Coverage (SWE-127)"
    "clang-tidy Analysis (SWE-108)"
    "WCET Benchmark Profile (Safety Plan §4)"
    "Python Tests (SWE-128)"
    "Documentation Build (SWE-104)"
)

# Gates that are structurally skipped in local runs.
# coverage-mcdc requires LLVM — auto-detect; skip only if missing.
if command -v llvm-profdata >/dev/null 2>&1 || \
   [ -x /opt/homebrew/opt/llvm/bin/llvm-profdata ] || \
   [ -x /usr/local/opt/llvm/bin/llvm-profdata ]; then
    GATE_SKIP=""
    GATE_COMMANDS[coverage-mcdc]="task coverage-cpp"  # reuses LLVM coverage task
else
    GATE_SKIP="coverage-mcdc"
fi

# ---------------------------------------------------------------------------
# Gate commands (associative array: key → bash command)
# All commands are run from REPO_ROOT.
# ---------------------------------------------------------------------------
declare -A GATE_COMMANDS
GATE_COMMANDS[lint]="task lint-check && task check-flags"
GATE_COMMANDS[test-cpp]="task test-cpp"
GATE_COMMANDS[swifi]="task swifi"
GATE_COMMANDS[sanitize]="task sanitize"
GATE_COMMANDS[coverage-cpp]="task coverage-cpp"
GATE_COMMANDS[tidy]="task tidy"
GATE_COMMANDS[wcet]="task wcet-profile"
GATE_COMMANDS[test-python]="task test-py"
GATE_COMMANDS[docs]="task docs-build"
# coverage-mcdc: intentionally absent — handled by GATE_SKIP logic

# ---------------------------------------------------------------------------
# Capture timestamp and ref metadata
# ---------------------------------------------------------------------------
TIMESTAMP="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
REF="$(git -C "${REPO_ROOT}" rev-parse HEAD 2>/dev/null || echo "unknown")"
REF_NAME="$(git -C "${REPO_ROOT}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "detached")"
if [[ "${REF_NAME}" == "HEAD" ]]; then
    # Detached HEAD — try to find a tag name
    REF_NAME="$(git -C "${REPO_ROOT}" describe --tags --exact-match HEAD 2>/dev/null || echo "detached@${REF:0:7}")"
fi

# ---------------------------------------------------------------------------
# Print run header
# ---------------------------------------------------------------------------
echo ""
echo "${BOLD}${CYAN}======================================================${RESET}"
echo "${BOLD}${CYAN}  LunaLink Recertification Runner${RESET}"
echo "${BOLD}${CYAN}  Project  : ${PROJECT}${RESET}"
echo "${BOLD}${CYAN}  Ref      : ${REF_NAME} @ ${REF:0:12}${RESET}"
echo "${BOLD}${CYAN}  Milestone: ${MILESTONE}${RESET}"
echo "${BOLD}${CYAN}  Reason   : ${REASON}${RESET}"
echo "${BOLD}${CYAN}  Time     : ${TIMESTAMP}${RESET}"
echo "${BOLD}${CYAN}======================================================${RESET}"
echo ""

# ---------------------------------------------------------------------------
# Run gate loop
# ---------------------------------------------------------------------------
declare -A VERDICTS
LOG_DIR="${REPO_ROOT}/build/recertify-logs"
mkdir -p "${LOG_DIR}"

ANY_FAIL=0
NUM_KEYS="${#GATE_KEYS[@]}"

for (( i=0; i<NUM_KEYS; i++ )); do
    key="${GATE_KEYS[$i]}"
    label="${GATE_LABELS[$i]}"

    # Print running banner
    printf "${BOLD}[%d/%d] %s${RESET}\\n" "$((i+1))" "${NUM_KEYS}" "${label}"

    # Check if this gate is always skipped
    if [[ " ${GATE_SKIP} " == *" ${key} "* ]]; then
        VERDICTS["${key}"]="SKIPPED"
        echo "${YELLOW}  SKIPPED${RESET} — not available in local runs (requires dedicated CI toolchain)"
        echo ""
        continue
    fi

    # Check if a command is defined (belt-and-suspenders for GATE_SKIP mismatches)
    if [[ -z "${GATE_COMMANDS[$key]+x}" ]]; then
        VERDICTS["${key}"]="SKIPPED"
        echo "${YELLOW}  SKIPPED${RESET} — no command defined for this gate"
        echo ""
        continue
    fi

    # Run the gate in a subshell; capture exit code without aborting the script
    log_file="${LOG_DIR}/${key}.log"
    cmd="${GATE_COMMANDS[$key]}"
    (
        cd "${REPO_ROOT}"
        eval "${cmd}"
    ) > "${log_file}" 2>&1
    gate_exit=$?

    if [[ "${gate_exit}" -eq 0 ]]; then
        VERDICTS["${key}"]="PASS"
        echo "${GREEN}  PASS${RESET}"
    else
        VERDICTS["${key}"]="FAIL"
        echo "${RED}  FAIL${RESET} (exit ${gate_exit}) — log: ${log_file}"
        # Print last 20 lines of log to help diagnose quickly
        echo "${RED}  --- last 20 lines of output ---${RESET}"
        tail -20 "${log_file}" | sed 's/^/  /'
        echo "${RED}  --------------------------------${RESET}"
        ANY_FAIL=1
    fi
    echo ""
done

# ---------------------------------------------------------------------------
# Compute overall verdict
# ---------------------------------------------------------------------------
OVERALL="PASS"
for key in "${GATE_KEYS[@]}"; do
    v="${VERDICTS[$key]}"
    if [[ "${v}" == "FAIL" ]]; then
        OVERALL="FAIL"
        break
    fi
    if [[ "${v}" == "CANCELLED" || "${v}" == "SKIPPED" ]] && [[ "${OVERALL}" == "PASS" ]]; then
        OVERALL="WARN"
    fi
done

# ---------------------------------------------------------------------------
# Build generator arguments
# ---------------------------------------------------------------------------
GENERATOR_ARGS=(
    --project    "${PROJECT}"
    --doc-id     "LUNALINK-RCERT-001"
    --ref        "${REF}"
    --ref-name   "${REF_NAME}"
    --milestone  "${MILESTONE}"
    --reason     "${REASON}"
    --authority  "${AUTHORITY}"
    --timestamp  "${TIMESTAMP}"
    --out        "${REPO_ROOT}/docs/assurance/recertification_record.rst"
)

for (( i=0; i<NUM_KEYS; i++ )); do
    key="${GATE_KEYS[$i]}"
    label="${GATE_LABELS[$i]}"
    GENERATOR_ARGS+=(--gate "${key}=${VERDICTS[$key]}")
    GENERATOR_ARGS+=(--gate-label "${key}=${label}")
done

# ---------------------------------------------------------------------------
# Generate RST record
# ---------------------------------------------------------------------------
echo "${BOLD}Generating recertification record...${RESET}"
python3 "${SCRIPT_DIR}/gen_recertification_record.py" "${GENERATOR_ARGS[@]}"
echo ""

# ---------------------------------------------------------------------------
# Print summary table
# ---------------------------------------------------------------------------
echo "${BOLD}${CYAN}======================================================${RESET}"
printf "${BOLD}${CYAN}  %-40s  %s${RESET}\\n" "Gate" "Verdict"
echo "${BOLD}${CYAN}  ----------------------------------------  -------${RESET}"

for (( i=0; i<NUM_KEYS; i++ )); do
    key="${GATE_KEYS[$i]}"
    label="${GATE_LABELS[$i]}"
    # Strip spec ref from label for the summary (extract "Name (spec)" -> "Name")
    name="${label%% (*}"
    v="${VERDICTS[$key]}"
    case "${v}" in
        PASS)      colour="${GREEN}" ;;
        FAIL)      colour="${RED}" ;;
        SKIPPED)   colour="${YELLOW}" ;;
        CANCELLED) colour="${YELLOW}" ;;
        *)         colour="${RESET}" ;;
    esac
    printf "  ${BOLD}%-40s${RESET}  ${colour}%s${RESET}\\n" "${name}" "${v}"
done

echo "${BOLD}${CYAN}  ----------------------------------------  -------${RESET}"
case "${OVERALL}" in
    PASS) overall_colour="${GREEN}" ;;
    FAIL) overall_colour="${RED}" ;;
    *)    overall_colour="${YELLOW}" ;;
esac
printf "  ${BOLD}%-40s  ${overall_colour}%s${RESET}\\n" "Overall" "${OVERALL}"
echo "${BOLD}${CYAN}======================================================${RESET}"
echo ""
echo "Record: ${REPO_ROOT}/docs/assurance/recertification_record.rst"
echo ""

# ---------------------------------------------------------------------------
# Exit code
# ---------------------------------------------------------------------------
exit "${ANY_FAIL}"
