#!/usr/bin/env bash
#
# run_layer2_tests.sh
#
# Layer-2 geometry, source, and scoring tests for shieldSim.
#
# These tests are still software/verification tests rather than full physics
# validation.  They use new diagnostic CLI options implemented for testing:
#   --random-seed
#   --output-prefix
#   --dump-source-samples
#   --dump-exit-particles
#
# The diagnostics let the script verify source sampling and shield-exit scoring
# without parsing Geant4 internals or relying on visual inspection.

set -u
set -o pipefail

print_usage() {
  cat <<'USAGE'
Usage:
  tests/run_layer2_tests.sh
  tests/run_layer2_tests.sh --help
  tests/run_layer2_tests.sh -help
  tests/run_layer2_tests.sh -h

Purpose:
  Run Layer-2 geometry, source-sampling, and scoring tests for the shieldSim
  Geant4 shielding application.

What the script tests:
  1. Clean CMake configure/build, unless SKIP_BUILD=1 is set.
  2. Diagnostic CLI options are present in --help.
  3. Beam-source diagnostics:
       x = y = 0, direction = +z, species = proton, E = 100 MeV.
  4. Isotropic-source diagnostics:
       source positions stay within the upstream plane;
       direction cosine mu = uz is positive;
       <mu> is close to 2/3 and <mu^2> is close to 1/2 for p(mu)=2mu.
  5. Near-vacuum shield transmission and shield rear-face scoring:
       most 100 MeV protons leave the downstream shield face;
       accepted exit crossings have positive local z direction;
       local exit z is close to the shield downstream face.
  6. Detector/target ordering in the computed-quantity output metadata.
  7. Material alias smoke test for Al and G4_Al.

What this script does NOT test:
  It does not validate stopping powers against NIST tables.
  It does not validate TID, DDD, n_eq, LET, or neutron production physics.
  It does not test Monte Carlo convergence.

Environment variables:
  BUILD_DIR   Build directory. Default: build_layer2_tests
  EXE_NAME    Executable name. Default: shieldSim
  LOG_DIR     Log directory. Default: layer2_test_logs
  RUN_DIR     Per-case run directory. Default: layer2_test_runs
  JOBS        Parallel build jobs. Default: nproc or 2
  SKIP_BUILD  If set to 1, skip CMake configure/build and use EXE_PATH if set,
              otherwise search BUILD_DIR for EXE_NAME.
  EXE_PATH    Explicit executable path. Useful with SKIP_BUILD=1.

Examples:
  tests/run_layer2_tests.sh
  JOBS=8 tests/run_layer2_tests.sh
  SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer2_tests.sh

Exit status:
  0   all tests passed
  1   one or more tests failed, or the executable could not be found
  2   invalid script command-line option
USAGE
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

BUILD_DIR="${BUILD_DIR:-build_layer2_tests}"
EXE_NAME="${EXE_NAME:-shieldSim}"
LOG_DIR="${LOG_DIR:-layer2_test_logs}"
RUN_DIR="${RUN_DIR:-layer2_test_runs}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
SKIP_BUILD="${SKIP_BUILD:-0}"
DATA_DIR="${SCRIPT_DIR}/data"

mkdir -p "${LOG_DIR}" "${RUN_DIR}"

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
  local case_name="$1"
  shift
  local case_dir="${PROJECT_DIR}/${RUN_DIR}/${case_name}"
  local logfile="${PROJECT_DIR}/${LOG_DIR}/${case_name}.log"

  rm -rf "${case_dir}"
  mkdir -p "${case_dir}"

  echo "${YELLOW}[RUN ]${RESET} case ${case_name}"
  {
    echo "case_dir=${case_dir}"
    echo "\$ ${EXE_ABS} $*"
  } > "${logfile}"

  ( cd "${case_dir}" && "${EXE_ABS}" "$@" ) >> "${logfile}" 2>&1
  local status=$?
  if [[ ${status} -eq 0 ]]; then
    pass "Run case ${case_name}"
  else
    fail "Run case ${case_name}" "${logfile}"
  fi
  return ${status}
}

require_file() {
  local name="$1"
  local path="$2"
  local logfile="$3"
  echo "Checking for ${path}" > "${logfile}"
  if [[ -s "${path}" ]]; then
    pass "${name}"
    return 0
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

print_header "Layer 2: build"

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
  echo "${YELLOW}[SKIP]${RESET} Build skipped because SKIP_BUILD=1"
fi

EXE_ABS="$(find_executable || true)"
if [[ -z "${EXE_ABS}" ]]; then
  fail "Find executable ${EXE_NAME}" "${LOG_DIR}/03_find_executable.log"
  echo "Executable ${EXE_NAME} was not found." > "${LOG_DIR}/03_find_executable.log"
else
  pass "Find executable ${EXE_ABS}"
fi

if [[ -z "${EXE_ABS}" ]]; then
  echo
  echo "${RED}Cannot continue Layer-2 tests because the executable was not found.${RESET}"
  exit 1
fi

print_header "Layer 2: diagnostic CLI availability"

run_success_with_grep \
  "Run --help and find Layer-2 diagnostic options" \
  "${LOG_DIR}/10_help_diagnostics.log" \
  "random-seed|dump-source-samples|dump-exit-particles|output-prefix|diagnostic-max-rows" \
  "${EXE_ABS}" --help

print_header "Layer 2: beam source diagnostic"

run_case "beam_source" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=Al:0.01 \
  --target=Si:1 \
  --events=1000 \
  --random-seed=12345 \
  --quantities=none \
  --output-prefix=beam \
  --dump-source-samples=source.dat \
  --diagnostic-max-rows=2000

require_file \
  "Beam source diagnostic file exists" \
  "${RUN_DIR}/beam_source/source.dat" \
  "${LOG_DIR}/21_beam_source_file.log"

run_check \
  "Beam source has x=y=0, direction +z, 100 MeV protons" \
  "${LOG_DIR}/22_beam_source_check.log" \
  awk '
    BEGIN { n=0; bad=0; }
    /^#/ { next; }
    {
      n++;
      species=$2; E=$3; x=$4; y=$5; ux=$7; uy=$8; uz=$9;
      if(species!="proton") bad++;
      if(E<99.999999 || E>100.000001) bad++;
      if(x*x>1.0e-18 || y*y>1.0e-18) bad++;
      if(ux*ux>1.0e-18 || uy*uy>1.0e-18) bad++;
      if(uz<0.999999999999 || uz>1.000000000001) bad++;
    }
    END {
      printf("rows=%d bad=%d\n",n,bad);
      exit !(n==1000 && bad==0);
    }' "${RUN_DIR}/beam_source/source.dat"

print_header "Layer 2: isotropic source diagnostic"

run_case "isotropic_source" \
  --source-mode=isotropic \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=Al:0.01 \
  --target=Si:1 \
  --events=20000 \
  --random-seed=24680 \
  --quantities=none \
  --output-prefix=isotropic \
  --dump-source-samples=source.dat \
  --diagnostic-max-rows=25000

require_file \
  "Isotropic source diagnostic file exists" \
  "${RUN_DIR}/isotropic_source/source.dat" \
  "${LOG_DIR}/31_isotropic_source_file.log"

run_check \
  "Isotropic source has p(mu)=2mu moments and bounded source plane" \
  "${LOG_DIR}/32_isotropic_source_check.log" \
  awk '
    function abs(x){ return x<0 ? -x : x; }
    BEGIN { n=0; bad=0; sum_mu=0; sum_mu2=0; sum_x=0; sum_y=0; max_abs_x=0; max_abs_y=0; }
    /^#/ { next; }
    {
      n++;
      x=$4; y=$5; ux=$7; uy=$8; uz=$9;
      mu=uz;
      sum_mu += mu;
      sum_mu2 += mu*mu;
      sum_x += x;
      sum_y += y;
      if(abs(x)>max_abs_x) max_abs_x=abs(x);
      if(abs(y)>max_abs_y) max_abs_y=abs(y);
      if(mu<=0.0 || mu>1.0000001) bad++;
      if(abs(x)>25.001 || abs(y)>25.001) bad++;
      if((ux*ux+uy*uy+uz*uz)<0.999999 || (ux*ux+uy*uy+uz*uz)>1.000001) bad++;
    }
    END {
      mean_mu=sum_mu/n;
      mean_mu2=sum_mu2/n;
      mean_x=sum_x/n;
      mean_y=sum_y/n;
      printf("rows=%d bad=%d mean_mu=%.6f mean_mu2=%.6f mean_x_mm=%.6f mean_y_mm=%.6f max_abs_x=%.6f max_abs_y=%.6f\n", n,bad,mean_mu,mean_mu2,mean_x,mean_y,max_abs_x,max_abs_y);
      ok = (n==20000 && bad==0 && mean_mu>0.64 && mean_mu<0.69 && mean_mu2>0.47 && mean_mu2<0.53 && abs(mean_x)<0.8 && abs(mean_y)<0.8);
      exit !ok;
    }' "${RUN_DIR}/isotropic_source/source.dat"

print_header "Layer 2: near-vacuum transmission and shield-exit scoring"

run_case "vacuum_transmission" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=G4_Galactic:0.001 \
  --target=Si:1 \
  --events=1000 \
  --random-seed=13579 \
  --quantities=none \
  --output-prefix=vacuum \
  --dump-exit-particles=exit.dat \
  --diagnostic-max-rows=2000

require_file \
  "Vacuum transmission exit diagnostic file exists" \
  "${RUN_DIR}/vacuum_transmission/exit.dat" \
  "${LOG_DIR}/41_vacuum_exit_file.log"

run_check \
  "Vacuum shield transmits most 100 MeV protons through rear face" \
  "${LOG_DIR}/42_vacuum_exit_check.log" \
  awk '
    function abs(x){ return x<0 ? -x : x; }
    BEGIN { n=0; bad=0; sumE=0; max_zerr=0; }
    /^#/ { next; }
    {
      n++;
      species=$2; E=$3; zl=$9; uzl=$12;
      sumE += E;
      zerr=abs(zl-0.0005);
      if(zerr>max_zerr) max_zerr=zerr;
      if(species!="proton") bad++;
      if(uzl<=0.0) bad++;
      if(zerr>0.002) bad++;
    }
    END {
      meanE = (n>0 ? sumE/n : 0.0);
      printf("rows=%d bad=%d meanE=%.9f max_local_z_error_mm=%.9g\n", n,bad,meanE,max_zerr);
      ok = (n>=950 && bad==0 && meanE>99.9 && meanE<100.1);
      exit !ok;
    }' "${RUN_DIR}/vacuum_transmission/exit.dat"

print_header "Layer 2: detector/target ordering"

run_case "target_order_water_si" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=Al:0.01 \
  --target=Water:1,Si:1 \
  --events=200 \
  --random-seed=10101 \
  --quantities=TID \
  --output-prefix=order_a

run_case "target_order_si_water" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=Al:0.01 \
  --target=Si:1,Water:1 \
  --events=200 \
  --random-seed=10101 \
  --quantities=TID \
  --output-prefix=order_b

run_check \
  "Target order Water,Si appears correctly in metadata" \
  "${LOG_DIR}/51_target_order_water_si.log" \
  bash -c 'grep -Eq "TargetIndex 0: .*Water" "${0}" && grep -Eq "TargetIndex 1: .*Si|TargetIndex 1: .*Silicon" "${0}"' \
  "${RUN_DIR}/target_order_water_si/order_a_quantities.dat"

run_check \
  "Target order Si,Water appears correctly in metadata" \
  "${LOG_DIR}/52_target_order_si_water.log" \
  bash -c 'grep -Eq "TargetIndex 0: .*Si|TargetIndex 0: .*Silicon" "${0}" && grep -Eq "TargetIndex 1: .*Water" "${0}"' \
  "${RUN_DIR}/target_order_si_water/order_b_quantities.dat"

print_header "Layer 2: material alias smoke test"

run_case "alias_al" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=Al:2 \
  --target=Si:1 \
  --events=100 \
  --random-seed=555 \
  --quantities=none \
  --output-prefix=alias_al

run_case "alias_g4al" \
  --source-mode=beam \
  --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
  --shield=G4_Al:2 \
  --target=Si:1 \
  --events=100 \
  --random-seed=555 \
  --quantities=none \
  --output-prefix=alias_g4al

run_check \
  "Al and G4_Al alias runs produced spectra files" \
  "${LOG_DIR}/61_alias_spectra_files.log" \
  bash -c '[[ -s "${0}" && -s "${1}" ]]' \
  "${RUN_DIR}/alias_al/alias_al_spectra.dat" \
  "${RUN_DIR}/alias_g4al/alias_g4al_spectra.dat"

print_header "Layer 2 test summary"

echo "Total tests: ${N_TOTAL}"
echo "Passed:      ${N_PASS}"
echo "Failed:      ${N_FAIL}"
echo "Logs:        ${LOG_DIR}/"
echo "Run outputs: ${RUN_DIR}/"

if [[ "${N_FAIL}" -eq 0 ]]; then
  echo "${GREEN}All Layer-2 tests passed.${RESET}"
  exit 0
else
  echo "${RED}Some Layer-2 tests failed.${RESET}"
  exit 1
fi
