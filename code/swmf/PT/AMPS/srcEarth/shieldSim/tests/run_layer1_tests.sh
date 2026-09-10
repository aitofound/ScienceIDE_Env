#!/usr/bin/env bash
#
# run_layer1_tests.sh
#
# Layer-1 build and smoke tests for shieldSim.
#
# This script tests:
#   1. Clean CMake configuration
#   2. Clean build
#   3. Basic CLI/help commands
#   4. Material/target/quantity list commands
#   5. Rejection of invalid CLI input
#
# It does not validate physics results. It only verifies that the executable
# builds, basic commands run, and invalid inputs fail cleanly.

set -u
set -o pipefail

print_usage() {
  cat <<'USAGE_EOF'
Usage:
  tests/run_layer1_tests.sh
  tests/run_layer1_tests.sh --help
  tests/run_layer1_tests.sh -help
  tests/run_layer1_tests.sh -h

Purpose:
  Run the Layer-1 build and smoke-test sequence for the shieldSim Geant4
  shielding application.

What the script does:
  1. Removes and recreates a clean build directory.
  2. Runs CMake configuration.
  3. Builds the shieldSim executable.
  4. Finds the executable in the build directory.
  5. Runs basic CLI commands:
       --help
       --list-materials
       --list-target-materials
       --list-detector-materials
       --list-quantities
  6. Runs invalid-input tests and verifies that the program fails cleanly:
       bad shielding material
       bad physics list
       bad source mode
       negative target thickness
       bad computed quantity
       malformed shield specification
       negative number of events
  7. Prints a PASS/FAIL summary.

What this script does NOT test:
  It does not validate the physics accuracy of the model.
  It does not compare Geant4 output against stopping-power tables.
  It does not test Monte Carlo convergence.
  It does not test TID, DDD, LET, or neutron-equivalent physics correctness.

Environment variables:
  BUILD_DIR   Build directory to use.
              Default: build_layer1_tests

  EXE_NAME    Name of the executable to search for.
              Default: shieldSim

  LOG_DIR     Directory where test logs are written.
              Default: layer1_test_logs

  JOBS        Number of parallel build jobs.
              Default: output of nproc, or 2 if nproc is unavailable.

Examples:
  tests/run_layer1_tests.sh

  BUILD_DIR=build_test tests/run_layer1_tests.sh

  BUILD_DIR=build_test JOBS=8 tests/run_layer1_tests.sh

Exit status:
  0   all tests passed
  1   one or more tests failed, or the executable could not be found
  2   invalid script command-line option
USAGE_EOF
}

for arg in "$@"; do
  case "${arg}" in
    --help|-help|-h)
      print_usage
      exit 0
      ;;
    *)
      echo "Error: unknown option '${arg}'"
      echo
      print_usage
      exit 2
      ;;
  esac
done

BUILD_DIR="${BUILD_DIR:-build_layer1_tests}"
EXE_NAME="${EXE_NAME:-shieldSim}"
LOG_DIR="${LOG_DIR:-layer1_test_logs}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

mkdir -p "${LOG_DIR}"

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
  local logfile="$2"
  ((N_FAIL++))
  ((N_TOTAL++))
  echo "${RED}[FAIL]${RESET} ${name}"
  echo "       log: ${logfile}"
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

run_expect_failure() {
  local name="$1"
  local logfile="$2"
  shift 2

  echo "${YELLOW}[RUN ]${RESET} ${name}"
  echo "\$ $*" > "${logfile}"

  if "$@" >> "${logfile}" 2>&1; then
    fail "${name}" "${logfile}"
    return 1
  else
    pass "${name}"
    return 0
  fi
}

run_success_with_grep() {
  local name="$1"
  local logfile="$2"
  local pattern="$3"
  shift 3

  echo "${YELLOW}[RUN ]${RESET} ${name}"
  echo "\$ $*" > "${logfile}"

  if "$@" >> "${logfile}" 2>&1; then
    if grep -Eiq "${pattern}" "${logfile}"; then
      pass "${name}"
      return 0
    else
      fail "${name} -- expected pattern not found: ${pattern}" "${logfile}"
      return 1
    fi
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

find_executable() {
  if [[ -x "${BUILD_DIR}/${EXE_NAME}" ]]; then
    echo "${BUILD_DIR}/${EXE_NAME}"
    return 0
  fi

  local found
  found="$(find "${BUILD_DIR}" -type f -name "${EXE_NAME}" -perm -111 2>/dev/null | head -n 1 || true)"

  if [[ -n "${found}" ]]; then
    echo "${found}"
    return 0
  fi

  return 1
}

print_header "Layer 1: clean build"

rm -rf "${BUILD_DIR}"

run_expect_success \
  "Configure with CMake" \
  "${LOG_DIR}/01_cmake_configure.log" \
  cmake -S . -B "${BUILD_DIR}"

run_expect_success \
  "Build shieldSim" \
  "${LOG_DIR}/02_cmake_build.log" \
  cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

EXE_PATH="$(find_executable || true)"

if [[ -z "${EXE_PATH:-}" ]]; then
  fail "Find executable ${EXE_NAME}" "${LOG_DIR}/03_find_executable.log"
  echo "Executable ${EXE_NAME} was not found under ${BUILD_DIR}" > "${LOG_DIR}/03_find_executable.log"
else
  pass "Find executable ${EXE_PATH}"
fi

if [[ -z "${EXE_PATH:-}" ]]; then
  echo
  echo "${RED}Cannot continue smoke tests because the executable was not found.${RESET}"
  exit 1
fi

print_header "Layer 1: help and list commands"

run_success_with_grep \
  "Run --help" \
  "${LOG_DIR}/10_help.log" \
  "shield|source|physics|target|material|quantity|unit" \
  "${EXE_PATH}" --help

run_success_with_grep \
  "Run --list-materials" \
  "${LOG_DIR}/11_list_materials.log" \
  "Al|Aluminum|HDPE|BPE|Water|Regolith" \
  "${EXE_PATH}" --list-materials

run_success_with_grep \
  "Run --list-target-materials" \
  "${LOG_DIR}/12_list_target_materials.log" \
  "BFO|Skin|Eye|Si|Silicon|Water|GaAs|Ge" \
  "${EXE_PATH}" --list-target-materials

run_success_with_grep \
  "Run --list-detector-materials alias" \
  "${LOG_DIR}/13_list_detector_materials.log" \
  "BFO|Skin|Eye|Si|Silicon|Water|GaAs|Ge" \
  "${EXE_PATH}" --list-detector-materials

run_success_with_grep \
  "Run --list-quantities" \
  "${LOG_DIR}/14_list_quantities.log" \
  "TID|DDD|LET|n_eq|H100" \
  "${EXE_PATH}" --list-quantities

print_header "Layer 1: invalid input rejection"

run_expect_failure \
  "Reject invalid shielding material" \
  "${LOG_DIR}/20_invalid_shield_material.log" \
  "${EXE_PATH}" --shield=BadMaterial:2

run_expect_failure \
  "Reject invalid physics list" \
  "${LOG_DIR}/21_invalid_physics_list.log" \
  "${EXE_PATH}" --physics-list=BadPhysics

run_expect_failure \
  "Reject invalid source mode" \
  "${LOG_DIR}/22_invalid_source_mode.log" \
  "${EXE_PATH}" --source-mode=badmode

run_expect_failure \
  "Reject negative target thickness" \
  "${LOG_DIR}/23_invalid_target_thickness.log" \
  "${EXE_PATH}" --target=Si:-1

run_expect_failure \
  "Reject invalid computed quantity" \
  "${LOG_DIR}/24_invalid_quantity.log" \
  "${EXE_PATH}" --quantities=BadQuantity

run_expect_failure \
  "Reject malformed shield specification" \
  "${LOG_DIR}/25_malformed_shield.log" \
  "${EXE_PATH}" --shield=Al

run_expect_failure \
  "Reject negative number of events" \
  "${LOG_DIR}/26_invalid_events.log" \
  "${EXE_PATH}" --events=-10

print_header "Layer 1 test summary"

echo "Total tests: ${N_TOTAL}"
echo "Passed:      ${N_PASS}"
echo "Failed:      ${N_FAIL}"
echo "Logs:        ${LOG_DIR}/"

if [[ "${N_FAIL}" -eq 0 ]]; then
  echo "${GREEN}All Layer-1 tests passed.${RESET}"
  exit 0
else
  echo "${RED}Some Layer-1 tests failed.${RESET}"
  exit 1
fi
