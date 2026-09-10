#!/usr/bin/env bash
#
# run_layer4_tests.sh
#
# Layer-4 regression tests for shieldSim.
#
# These tests run a small suite of deterministic end-to-end cases, collect
# integrated quantities into a compact JSON file, and compare that JSON against
# a previously accepted baseline.  The purpose is to detect unintended changes
# in the software behavior after edits to the transport setup, scoring,
# material catalog, computed quantities, or output logic.
#
# Important distinction:
#   Layer-4 tests are regression tests, not physics validation tests.  Passing
#   them means "the result did not change beyond tolerance relative to the
#   accepted baseline."  It does not mean the model has been validated against
#   NIST stopping-power tables, device-response data, or measurements.

set -u
set -o pipefail

print_usage() {
  cat <<'USAGE'
Usage:
  tests/run_layer4_tests.sh
  tests/run_layer4_tests.sh --check
  tests/run_layer4_tests.sh --update-baseline
  tests/run_layer4_tests.sh --actual-only
  tests/run_layer4_tests.sh --help
  tests/run_layer4_tests.sh -help
  tests/run_layer4_tests.sh -h

Purpose:
  Run Layer-4 regression tests for the shieldSim Geant4 shielding application.
  The script executes deterministic end-to-end cases, collects compact summary
  quantities into JSON, and compares the actual JSON against an accepted
  baseline JSON.

Modes:
  --check
      Default mode. Run the cases, collect actual results, and compare them
      against the accepted baseline.

  --update-baseline
      Run the cases and replace the baseline JSON with the newly collected
      results. Use this only after reviewing the outputs and deciding that the
      new behavior is correct.

  --actual-only
      Run the cases and write the actual-results JSON, but do not compare and
      do not update the baseline. This is useful for inspecting changes before
      accepting them.

What the script tests:
  1. Clean CMake configure/build, unless SKIP_BUILD=1 is set.
  2. Deterministic monoenergetic beam through Al into Si.
  3. Deterministic monoenergetic alpha beam through Al into Si.
  4. Isotropic SEP-like spectrum through Al into BFO and Si targets.
  5. Built-in broad spectrum through HDPE into BFO and Si targets.
  6. A high-Z shielding smoke case with the Shielding physics list.
  7. A short sweep-mode case with multiple shield thicknesses.
  8. Regression comparison of integrated quantities, transmitted-particle
     counts, H100/10, target TID/DDD/n_eq rates, LET moments, and compact output
     file signatures.

What this script does NOT test:
  It does not independently validate Geant4 physics.
  It does not compare against NIST PSTAR/ASTAR or NIEL tables.
  It does not prove Monte Carlo convergence.
  It does not replace Layer-1/2/3 tests.

Environment variables:
  BUILD_DIR          Build directory. Default: build_layer4_tests
  EXE_NAME           Executable name. Default: shieldSim
  LOG_DIR            Log directory. Default: layer4_test_logs
  RUN_DIR            Per-case run directory. Default: layer4_test_runs
  JOBS               Parallel build jobs. Default: nproc or 2
  SKIP_BUILD         If set to 1, skip CMake configure/build.
  EXE_PATH           Explicit executable path. Useful with SKIP_BUILD=1.
  EVENTS_REGRESSION  Events for ordinary regression cases. Default: 4000
  EVENTS_SMOKE       Events for the high-Z/sweep smoke cases. Default: 1000
  REL_TOL            Relative tolerance for numerical JSON comparison.
                     Default: 0.15
  ABS_TOL            Absolute tolerance for numerical JSON comparison.
                     Default: 1e-30
  BASELINE_FILE      Accepted baseline JSON.
                     Default: tests/expected/layer4/layer4_reference.json
  ACTUAL_FILE        Actual JSON written by this run.
                     Default: layer4_test_runs/layer4_actual.json

Examples:
  # First trusted run: create/replace the baseline after manual review.
  tests/run_layer4_tests.sh --update-baseline

  # Normal regression check after code changes.
  tests/run_layer4_tests.sh

  # Inspect an actual result without comparing or updating the baseline.
  tests/run_layer4_tests.sh --actual-only

  # Use a previously built executable.
  SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer4_tests.sh --check

  # Run with more statistics and tighter tolerance.
  EVENTS_REGRESSION=20000 REL_TOL=0.05 tests/run_layer4_tests.sh --check

Exit status:
  0   requested operation succeeded
  1   one or more tests/comparisons failed, or the executable was not found
  2   invalid script command-line option
USAGE
}

MODE="check"
for arg in "$@"; do
  case "${arg}" in
    --help|-help|-h)
      print_usage
      exit 0
      ;;
    --check)
      MODE="check"
      ;;
    --update-baseline)
      MODE="update-baseline"
      ;;
    --actual-only)
      MODE="actual-only"
      ;;
    *)
      echo "Error: unknown option '${arg}'"
      echo
      print_usage
      exit 2
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

BUILD_DIR="${BUILD_DIR:-build_layer4_tests}"
EXE_NAME="${EXE_NAME:-shieldSim}"
LOG_DIR="${LOG_DIR:-layer4_test_logs}"
RUN_DIR="${RUN_DIR:-layer4_test_runs}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
SKIP_BUILD="${SKIP_BUILD:-0}"
DATA_DIR="${SCRIPT_DIR}/data"
EXAMPLE_DIR="${PROJECT_DIR}/examples"
EXPECTED_DIR="${SCRIPT_DIR}/expected/layer4"
EVENTS_REGRESSION="${EVENTS_REGRESSION:-4000}"
EVENTS_SMOKE="${EVENTS_SMOKE:-1000}"
REL_TOL="${REL_TOL:-0.15}"
ABS_TOL="${ABS_TOL:-1.0e-30}"
BASELINE_FILE="${BASELINE_FILE:-${EXPECTED_DIR}/layer4_reference.json}"
ACTUAL_FILE="${ACTUAL_FILE:-${PROJECT_DIR}/${RUN_DIR}/layer4_actual.json}"
MANIFEST_FILE="${PROJECT_DIR}/${RUN_DIR}/layer4_manifest.tsv"
TOOLS_PY="${SCRIPT_DIR}/regression_tools.py"

mkdir -p "${LOG_DIR}" "${RUN_DIR}" "${EXPECTED_DIR}"
: > "${MANIFEST_FILE}"

N_PASS=0
N_FAIL=0
N_TOTAL=0

if [[ -t 1 ]]; then
  RED=$'\033[31m'
  GREEN=$'\033[32m'
  YELLOW=$'\033[33m'
  BLUE=$'\033[34m'
  RESET=$'\033[0m'
else
  RED=""
  GREEN=""
  YELLOW=""
  BLUE=""
  RESET=""
fi

print_header() {
  echo
  echo "${BLUE}============================================================${RESET}"
  echo "${BLUE}$1${RESET}"
  echo "${BLUE}============================================================${RESET}"
}

pass() {
  local name="$1"
  ((N_PASS++))
  ((N_TOTAL++))
  echo "${GREEN}[PASS]${RESET} ${name}"
}

fail() {
  local name="$1"
  local logfile="${2:-}"
  ((N_FAIL++))
  ((N_TOTAL++))
  echo "${RED}[FAIL]${RESET} ${name}"
  if [[ -n "${logfile}" ]]; then
    echo "       log: ${logfile}"
  fi
}

run_expect_success() {
  local name="$1"
  local logfile="$2"
  shift 2

  echo "${YELLOW}[RUN ]${RESET} ${name}"
  echo "\$ $*" > "${logfile}"

  if "$@" >> "${logfile}" 2>&1; then
    pass "${name}"
    return 0
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

run_check() {
  local name="$1"
  local logfile="$2"
  shift 2

  echo "${YELLOW}[CHECK]${RESET} ${name}"
  echo "\$ $*" > "${logfile}"

  if "$@" >> "${logfile}" 2>&1; then
    pass "${name}"
    return 0
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

find_executable() {
  if [[ -n "${EXE_PATH:-}" && -x "${EXE_PATH}" ]]; then
    if [[ "${EXE_PATH}" = /* ]]; then
      echo "${EXE_PATH}"
    else
      echo "${PROJECT_DIR}/${EXE_PATH}"
    fi
    return 0
  fi

  if [[ -x "${BUILD_DIR}/${EXE_NAME}" ]]; then
    echo "${PROJECT_DIR}/${BUILD_DIR}/${EXE_NAME}"
    return 0
  fi

  local found
  found="$(find "${BUILD_DIR}" -type f -name "${EXE_NAME}" -perm -111 2>/dev/null | head -n 1 || true)"
  if [[ -n "${found}" ]]; then
    echo "${PROJECT_DIR}/${found}"
    return 0
  fi

  return 1
}

run_case() {
  local name="$1"
  local case_dir="$2"
  local logfile="$3"
  local prefix="$4"
  shift 4

  rm -rf "${case_dir}"
  mkdir -p "${case_dir}"

  echo "${YELLOW}[RUN ]${RESET} ${name}"
  {
    echo "case_dir=${case_dir}"
    echo "prefix=${prefix}"
    echo "\$ $*"
  } > "${logfile}"

  if (cd "${case_dir}" && "$@") >> "${logfile}" 2>&1; then
    pass "${name}"
    # Manifest columns:
    #   case_name, summary_file, LET_file, quantities_file, spectra_file
    # The helper script parses these files and converts them into a compact
    # JSON signature for baseline comparison.
    printf '%s\t%s\t%s\t%s\t%s\n' \
      "${name}" \
      "${case_dir}/summary.dat" \
      "${case_dir}/${prefix}_let_spectrum.dat" \
      "${case_dir}/${prefix}_quantities.dat" \
      "${case_dir}/${prefix}_spectra.dat" >> "${MANIFEST_FILE}"
    return 0
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

print_header "Layer 4: build"

if [[ "${SKIP_BUILD}" != "1" ]]; then
  rm -rf "${BUILD_DIR}"
  run_expect_success \
    "Configure with CMake" \
    "${LOG_DIR}/01_cmake_configure.log" \
    cmake -S . -B "${BUILD_DIR}"

  run_expect_success \
    "Build shieldSim" \
    "${LOG_DIR}/02_cmake_build.log" \
    cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
else
  pass "Skip build because SKIP_BUILD=1"
fi

EXE_ABS="$(find_executable || true)"
if [[ -z "${EXE_ABS:-}" ]]; then
  fail "Find executable ${EXE_NAME}" "${LOG_DIR}/03_find_executable.log"
  echo "Executable ${EXE_NAME} was not found under ${BUILD_DIR}" > "${LOG_DIR}/03_find_executable.log"
else
  pass "Find executable ${EXE_ABS}"
fi

if [[ -z "${EXE_ABS:-}" ]]; then
  echo
  echo "${RED}Cannot continue Layer-4 tests because the executable was not found.${RESET}"
  exit 1
fi

run_check \
  "Regression helper script is available" \
  "${LOG_DIR}/04_regression_helper.log" \
  python3 "${TOOLS_PY}" --help

print_header "Layer 4: deterministic end-to-end regression cases"

CASE1="${PROJECT_DIR}/${RUN_DIR}/regression_001_beam_100MeV_p_Al2mm_Si1mm"
run_case \
  "regression_001_beam_100MeV_p_Al2mm_Si1mm" \
  "${CASE1}" \
  "${LOG_DIR}/10_regression_001.log" \
  "reg001" \
  "${EXE_ABS}" \
    --physics-list=FTFP_BERT \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --shield=Al:2 \
    --target=Si:1 \
    --events="${EVENTS_REGRESSION}" \
    --random-seed=41001 \
    --output-prefix=reg001 \
    --dump-run-summary=summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=0.2

CASE2="${PROJECT_DIR}/${RUN_DIR}/regression_002_beam_100MeV_alpha_Al2mm_Si1mm"
run_case \
  "regression_002_beam_100MeV_alpha_Al2mm_Si1mm" \
  "${CASE2}" \
  "${LOG_DIR}/11_regression_002.log" \
  "reg002" \
  "${EXE_ABS}" \
    --physics-list=FTFP_BERT \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_alpha.dat" \
    --shield=Al:2 \
    --target=Si:1 \
    --events="${EVENTS_REGRESSION}" \
    --random-seed=41002 \
    --output-prefix=reg002 \
    --dump-run-summary=summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=0.2

CASE3="${PROJECT_DIR}/${RUN_DIR}/regression_003_isotropic_SEP_Al2mm_BFO50mm_Si1mm"
run_case \
  "regression_003_isotropic_SEP_Al2mm_BFO50mm_Si1mm" \
  "${CASE3}" \
  "${LOG_DIR}/12_regression_003.log" \
  "reg003" \
  "${EXE_ABS}" \
    --physics-list=FTFP_BERT_HP \
    --source-mode=isotropic \
    --spectrum="${EXAMPLE_DIR}/sep_spectrum.dat" \
    --shield=Al:2 \
    --target=BFO:50,Si:1 \
    --events="${EVENTS_REGRESSION}" \
    --random-seed=41003 \
    --output-prefix=reg003 \
    --dump-run-summary=summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=0.5

CASE4="${PROJECT_DIR}/${RUN_DIR}/regression_004_builtin_HDPE10mm_BFO50mm_Si1mm"
run_case \
  "regression_004_builtin_HDPE10mm_BFO50mm_Si1mm" \
  "${CASE4}" \
  "${LOG_DIR}/13_regression_004.log" \
  "reg004" \
  "${EXE_ABS}" \
    --physics-list=FTFP_BERT \
    --source-mode=isotropic \
    --shield=HDPE:10 \
    --target=BFO:50,Si:1 \
    --events="${EVENTS_REGRESSION}" \
    --random-seed=41004 \
    --output-prefix=reg004 \
    --dump-run-summary=summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=1.0

CASE5="${PROJECT_DIR}/${RUN_DIR}/regression_005_highZ_Shielding_W5mm_Si1mm"
run_case \
  "regression_005_highZ_Shielding_W5mm_Si1mm" \
  "${CASE5}" \
  "${LOG_DIR}/14_regression_005.log" \
  "reg005" \
  "${EXE_ABS}" \
    --physics-list=Shielding \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_150MeV_proton.dat" \
    --shield=W:5 \
    --target=Si:1 \
    --events="${EVENTS_SMOKE}" \
    --random-seed=41005 \
    --output-prefix=reg005 \
    --dump-run-summary=summary.dat \
    --quantities=TID,DDD,n_eq,LET,H100/10 \
    --production-cut=1.0 \
    --max-step=0.5

CASE6="${PROJECT_DIR}/${RUN_DIR}/regression_006_sweep_Al_0p5_to_2mm_Si1mm"
run_case \
  "regression_006_sweep_Al_0p5_to_2mm_Si1mm" \
  "${CASE6}" \
  "${LOG_DIR}/15_regression_006.log" \
  "reg006" \
  "${EXE_ABS}" \
    --sweep \
    --sweep-material=Al \
    --sweep-tmin=0.5 \
    --sweep-tmax=2.0 \
    --sweep-n=3 \
    --physics-list=FTFP_BERT \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --target=Si:1 \
    --events="${EVENTS_SMOKE}" \
    --random-seed=41006 \
    --output-prefix=reg006 \
    --dump-run-summary=summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=0.5

print_header "Layer 4: collect regression signature"

run_check \
  "Collect actual regression JSON" \
  "${LOG_DIR}/20_collect_actual.log" \
  python3 "${TOOLS_PY}" collect \
    --manifest "${MANIFEST_FILE}" \
    --output "${ACTUAL_FILE}"

print_header "Layer 4: baseline operation"

if [[ "${MODE}" == "update-baseline" ]]; then
  run_check \
    "Update accepted baseline JSON" \
    "${LOG_DIR}/30_update_baseline.log" \
    python3 - "${ACTUAL_FILE}" "${BASELINE_FILE}" <<'PY'
import pathlib, shutil, sys
actual = pathlib.Path(sys.argv[1])
baseline = pathlib.Path(sys.argv[2])
if not actual.exists() or actual.stat().st_size == 0:
    raise SystemExit('actual file is missing or empty: ' + str(actual))
baseline.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(actual, baseline)
print(f'updated baseline: {baseline}')
PY
elif [[ "${MODE}" == "actual-only" ]]; then
  pass "Actual-only mode: comparison and baseline update intentionally skipped"
else
  run_check \
    "Compare actual JSON against accepted baseline" \
    "${LOG_DIR}/31_compare_baseline.log" \
    python3 "${TOOLS_PY}" compare \
      --baseline "${BASELINE_FILE}" \
      --actual "${ACTUAL_FILE}" \
      --rel-tol "${REL_TOL}" \
      --abs-tol "${ABS_TOL}"
fi

print_header "Layer 4 test summary"

echo "Mode:        ${MODE}"
echo "Total tests: ${N_TOTAL}"
echo "Passed:      ${N_PASS}"
echo "Failed:      ${N_FAIL}"
echo "Logs:        ${LOG_DIR}/"
echo "Run data:    ${RUN_DIR}/"
echo "Actual JSON: ${ACTUAL_FILE}"
echo "Baseline:    ${BASELINE_FILE}"
echo "Rel tol:     ${REL_TOL}"
echo "Abs tol:     ${ABS_TOL}"

if [[ "${N_FAIL}" -eq 0 ]]; then
  echo "${GREEN}Layer-4 operation completed successfully.${RESET}"
  exit 0
else
  echo "${RED}Layer-4 operation failed.${RESET}"
  exit 1
fi
