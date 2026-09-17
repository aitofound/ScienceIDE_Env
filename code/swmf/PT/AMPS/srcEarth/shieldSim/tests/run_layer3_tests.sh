#!/usr/bin/env bash
#
# run_layer3_tests.sh
#
# Layer-3 physics and numerical sanity tests for shieldSim.
#
# These tests exercise the physics-facing outputs of the code without trying to
# replace a full validation campaign against NIST, SR-NIEL, or experimental
# data.  They are designed to catch implementation regressions in source
# normalization, computed-quantity output, LET folding, H100/10, physics-list
# selection, sweep output, and optional numerical controls.

set -u
set -o pipefail

print_usage() {
  cat <<'USAGE'
Usage:
  tests/run_layer3_tests.sh
  tests/run_layer3_tests.sh --help
  tests/run_layer3_tests.sh -help
  tests/run_layer3_tests.sh -h

Purpose:
  Run Layer-3 physics-output and numerical-sanity tests for the shieldSim
  Geant4 shielding application.

What the script tests:
  1. Clean CMake configure/build, unless SKIP_BUILD=1 is set.
  2. CLI help includes Layer-3 diagnostic/numerical options:
       --dump-run-summary, --production-cut, --max-step.
  3. A monoenergetic 100 MeV proton run writes all computed output files:
       *_spectra.dat, *_quantities.dat, *_let_spectrum.dat, and run summary.
  4. Source-intensity scaling:
       multiplying the input spectrum by 10 leaves per-primary TID unchanged
       but multiplies source-normalized TID rate by 10.
  5. H100/10 hardness sanity checks:
       50 MeV transmitted spectrum gives H100/10 approximately 0;
       150 MeV transmitted spectrum gives H100/10 approximately 1.
  6. LET sanity check:
       100 MeV total-energy alpha particles in Si have a larger folded mean
       LET than 100 MeV protons.
  7. Supported physics lists run successfully in a short smoke test:
       FTFP_BERT, FTFP_BERT_HP, Shielding, QGSP_BIC_HP.
  8. Sweep-mode scalar output is produced and shield areal density increases
       monotonically with shield thickness.

What this script does NOT test:
  It does not prove that the physics model is quantitatively validated.
  It does not compare stopping powers against NIST PSTAR/ASTAR tables.
  It does not compare NIEL or n_eq against SR-NIEL/device-response tables.
  It does not provide high-statistics Monte Carlo convergence.

Environment variables:
  BUILD_DIR       Build directory. Default: build_layer3_tests
  EXE_NAME        Executable name. Default: shieldSim
  LOG_DIR         Log directory. Default: layer3_test_logs
  RUN_DIR         Per-case run directory. Default: layer3_test_runs
  JOBS            Parallel build jobs. Default: nproc or 2
  SKIP_BUILD      If set to 1, skip CMake configure/build and use EXE_PATH if
                  set; otherwise search BUILD_DIR for EXE_NAME.
  EXE_PATH        Explicit executable path. Useful with SKIP_BUILD=1.
  EVENTS_SHORT    Events for ordinary Layer-3 cases. Default: 2000
  EVENTS_SMOKE    Events for quick physics-list/sweep smoke cases. Default: 300

Examples:
  tests/run_layer3_tests.sh
  JOBS=8 tests/run_layer3_tests.sh
  SKIP_BUILD=1 EXE_PATH=build/shieldSim tests/run_layer3_tests.sh
  EVENTS_SHORT=10000 EVENTS_SMOKE=1000 tests/run_layer3_tests.sh

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

BUILD_DIR="${BUILD_DIR:-build_layer3_tests}"
EXE_NAME="${EXE_NAME:-shieldSim}"
LOG_DIR="${LOG_DIR:-layer3_test_logs}"
RUN_DIR="${RUN_DIR:-layer3_test_runs}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
SKIP_BUILD="${SKIP_BUILD:-0}"
DATA_DIR="${SCRIPT_DIR}/data"
EVENTS_SHORT="${EVENTS_SHORT:-2000}"
EVENTS_SMOKE="${EVENTS_SMOKE:-300}"

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
    if grep -Eiq -- "${pattern}" "${logfile}"; then
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
  local name="$1"
  local case_dir="$2"
  local logfile="$3"
  shift 3

  rm -rf "${case_dir}"
  mkdir -p "${case_dir}"

  echo "${YELLOW}[RUN ]${RESET} ${name}"
  {
    echo "case_dir=${case_dir}"
    echo "\$ $*"
  } > "${logfile}"

  if (cd "${case_dir}" && "$@") >> "${logfile}" 2>&1; then
    pass "${name}"
    return 0
  else
    fail "${name}" "${logfile}"
    return 1
  fi
}

print_header "Layer 3: build"

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
  echo "${RED}Cannot continue Layer-3 tests because the executable was not found.${RESET}"
  exit 1
fi

print_header "Layer 3: CLI support for physics/numerics diagnostics"

run_success_with_grep \
  "Help includes Layer-3 diagnostic/numerical options" \
  "${LOG_DIR}/10_help_layer3_options.log" \
  "dump-run-summary|production-cut|max-step" \
  "${EXE_ABS}" --help

print_header "Layer 3: computed-quantity output smoke test"

CASE_ALL="${PROJECT_DIR}/${RUN_DIR}/all_quantities"
run_case \
  "Monoenergetic 100 MeV proton run with all quantities" \
  "${CASE_ALL}" \
  "${LOG_DIR}/20_all_quantities_run.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --shield=Al:1 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=31001 \
    --output-prefix=all_quantities \
    --dump-run-summary=all_quantities_summary.dat \
    --quantities=all \
    --production-cut=1.0 \
    --max-step=0.1

run_check \
  "All-quantities run wrote expected files and positive scalar outputs" \
  "${LOG_DIR}/21_all_quantities_check.log" \
  python3 - "${CASE_ALL}" <<'PY'
import pathlib, sys, math
case = pathlib.Path(sys.argv[1])
required = [
    case/'all_quantities_spectra.dat',
    case/'all_quantities_quantities.dat',
    case/'all_quantities_let_spectrum.dat',
    case/'all_quantities_summary.dat',
]
missing = [str(p) for p in required if not p.exists() or p.stat().st_size == 0]
if missing:
    raise SystemExit('missing/empty files: ' + ', '.join(missing))
scalars = {}
counts = {}
targets = []
for line in (case/'all_quantities_summary.dat').read_text().splitlines():
    f = line.split()
    if not f or f[0].startswith('#'):
        continue
    if f[0] == 'scalar':
        scalars[f[1]] = float(f[2])
    elif f[0] == 'count':
        counts[f[1]] = float(f[2])
    elif f[0] == 'target':
        targets.append(f)
if counts.get('input_proton',0) <= 0 or counts.get('output_proton',0) <= 0:
    raise SystemExit('expected positive input/output proton counts')
if not targets:
    raise SystemExit('no target rows in summary')
tid = float(targets[0][4])
tid_rate = float(targets[0][5])
if not (tid > 0 and tid_rate > 0):
    raise SystemExit(f'expected positive TID and TIDRate, got {tid}, {tid_rate}')
if abs(scalars.get('production_cut_mm',0)-1.0) > 1e-9:
    raise SystemExit('production_cut_mm was not recorded as 1.0')
if abs(scalars.get('max_step_mm',0)-0.1) > 1e-9:
    raise SystemExit('max_step_mm was not recorded as 0.1')
print('TID_Gy_perPrimary=', tid)
print('TIDRate_Gy_s=', tid_rate)
PY

print_header "Layer 3: source-normalization scaling"

CASE_RATE1="${PROJECT_DIR}/${RUN_DIR}/rate_scale_1"
CASE_RATE10="${PROJECT_DIR}/${RUN_DIR}/rate_scale_10"
run_case \
  "Source scaling baseline, rate x1" \
  "${CASE_RATE1}" \
  "${LOG_DIR}/30_rate_scale_1.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --shield=Al:1 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=32001 \
    --output-prefix=rate1 \
    --dump-run-summary=summary.dat \
    --quantities=TID

run_case \
  "Source scaling comparison, rate x10" \
  "${CASE_RATE10}" \
  "${LOG_DIR}/31_rate_scale_10.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton_rate10.dat" \
    --shield=Al:1 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=32001 \
    --output-prefix=rate10 \
    --dump-run-summary=summary.dat \
    --quantities=TID

run_check \
  "Source scaling: TID unchanged and TIDRate scales by 10" \
  "${LOG_DIR}/32_rate_scaling_check.log" \
  python3 - "${CASE_RATE1}/summary.dat" "${CASE_RATE10}/summary.dat" <<'PY'
import sys, math

def read_target(path):
    source_norm = None
    for line in open(path):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        if f[0] == 'scalar' and f[1] == 'source_norm':
            source_norm = float(f[2])
        if f[0] == 'target':
            tid = float(f[4])
            rate = float(f[5])
            return source_norm, tid, rate
    raise SystemExit('no target row in ' + path)

n1, tid1, rate1 = read_target(sys.argv[1])
n10, tid10, rate10 = read_target(sys.argv[2])
if not (tid1 > 0 and tid10 > 0 and rate1 > 0 and rate10 > 0):
    raise SystemExit('expected positive TID and rates')
rel_tid = abs(tid10 - tid1) / max(abs(tid1), 1e-300)
ratio_rate = rate10 / rate1
ratio_norm = n10 / n1
if rel_tid > 0.02:
    raise SystemExit(f'per-primary TID changed by {rel_tid:.3g}; expected unchanged within 2%')
if abs(ratio_rate - 10.0) > 0.2:
    raise SystemExit(f'TIDRate ratio {ratio_rate:.6g}; expected 10 within 2%')
if abs(ratio_norm - 10.0) > 1e-9:
    raise SystemExit(f'source normalization ratio {ratio_norm:.12g}; expected 10')
print('TID ratio=', tid10/tid1)
print('TIDRate ratio=', ratio_rate)
PY

print_header "Layer 3: H100/10 hardness sanity checks"

CASE_H_LOW="${PROJECT_DIR}/${RUN_DIR}/hardness_50MeV"
CASE_H_HIGH="${PROJECT_DIR}/${RUN_DIR}/hardness_150MeV"
run_case \
  "H100/10 low-energy case, 50 MeV protons" \
  "${CASE_H_LOW}" \
  "${LOG_DIR}/40_hardness_low.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_50MeV_proton.dat" \
    --shield=G4_Galactic:0.001 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=33001 \
    --output-prefix=h50 \
    --dump-run-summary=summary.dat \
    --quantities=H100/10

run_case \
  "H100/10 high-energy case, 150 MeV protons" \
  "${CASE_H_HIGH}" \
  "${LOG_DIR}/41_hardness_high.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_150MeV_proton.dat" \
    --shield=G4_Galactic:0.001 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=33002 \
    --output-prefix=h150 \
    --dump-run-summary=summary.dat \
    --quantities=H100/10

run_check \
  "H100/10 is near 0 below 100 MeV and near 1 above 100 MeV" \
  "${LOG_DIR}/42_hardness_check.log" \
  python3 - "${CASE_H_LOW}/summary.dat" "${CASE_H_HIGH}/summary.dat" <<'PY'
import sys

def read_h(path):
    for line in open(path):
        f=line.split()
        if len(f)>=3 and f[0]=='scalar' and f[1]=='H100_10':
            return float(f[2])
    raise SystemExit('missing H100_10 in ' + path)

h_low = read_h(sys.argv[1])
h_high = read_h(sys.argv[2])
if h_low > 0.05:
    raise SystemExit(f'50 MeV case H100/10={h_low}; expected <=0.05')
if h_high < 0.95:
    raise SystemExit(f'150 MeV case H100/10={h_high}; expected >=0.95')
print('H50=', h_low)
print('H150=', h_high)
PY

print_header "Layer 3: LET folded-spectrum sanity check"

CASE_LET_P="${PROJECT_DIR}/${RUN_DIR}/let_proton_100MeV"
CASE_LET_A="${PROJECT_DIR}/${RUN_DIR}/let_alpha_100MeV"
run_case \
  "LET case, 100 MeV protons in Si" \
  "${CASE_LET_P}" \
  "${LOG_DIR}/50_let_proton.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --shield=G4_Galactic:0.001 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=34001 \
    --output-prefix=letp \
    --dump-run-summary=summary.dat \
    --quantities=LET

run_case \
  "LET case, 100 MeV total-energy alpha particles in Si" \
  "${CASE_LET_A}" \
  "${LOG_DIR}/51_let_alpha.log" \
  "${EXE_ABS}" \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_alpha.dat" \
    --shield=G4_Galactic:0.001 \
    --target=Si:1 \
    --events="${EVENTS_SHORT}" \
    --random-seed=34002 \
    --output-prefix=leta \
    --dump-run-summary=summary.dat \
    --quantities=LET

run_check \
  "Alpha LET mean is larger than proton LET mean" \
  "${LOG_DIR}/52_let_check.log" \
  python3 - "${CASE_LET_P}/letp_let_spectrum.dat" "${CASE_LET_A}/leta_let_spectrum.dat" <<'PY'
import sys

def mean_let(path):
    num = den = 0.0
    for line in open(path):
        line=line.strip()
        if not line or line.startswith('#') or line.startswith('TITLE') or line.startswith('VARIABLES') or line.startswith('ZONE'):
            continue
        f=line.split()
        if len(f) < 4:
            continue
        L = float(f[0])
        charged = float(f[3])
        num += L*charged
        den += charged
    if den <= 0:
        raise SystemExit('no charged LET fluence in ' + path)
    return num/den

p = mean_let(sys.argv[1])
a = mean_let(sys.argv[2])
if not (a > 2.0*p):
    raise SystemExit(f'alpha mean LET {a:.6g} is not at least twice proton mean LET {p:.6g}')
print('mean LET proton=', p)
print('mean LET alpha=', a)
PY

print_header "Layer 3: supported physics-list smoke tests"

for phys in FTFP_BERT FTFP_BERT_HP Shielding QGSP_BIC_HP; do
  safe="${phys//[^A-Za-z0-9_]/_}"
  case_dir="${PROJECT_DIR}/${RUN_DIR}/physics_${safe}"
  run_case \
    "Physics-list smoke: ${phys}" \
    "${case_dir}" \
    "${LOG_DIR}/60_physics_${safe}.log" \
    "${EXE_ABS}" \
      --physics-list="${phys}" \
      --source-mode=beam \
      --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
      --shield=Al:1 \
      --target=Si:1 \
      --events="${EVENTS_SMOKE}" \
      --random-seed=35001 \
      --output-prefix="phys_${safe}" \
      --dump-run-summary=summary.dat \
      --quantities=TID,DDD,n_eq,H100/10

  run_check \
    "Physics-list ${phys} produced positive or zero well-formed summary" \
    "${LOG_DIR}/61_physics_${safe}_check.log" \
    python3 - "${case_dir}/summary.dat" <<'PY'
import sys, math, pathlib
path = pathlib.Path(sys.argv[1])
if not path.exists() or path.stat().st_size == 0:
    raise SystemExit('missing summary')
seen_target = False
for line in path.read_text().splitlines():
    f=line.split()
    if f and f[0]=='target':
        vals=[float(x) for x in f[3:]]
        if any((not math.isfinite(v)) or v < 0 for v in vals):
            raise SystemExit('negative or non-finite target quantity: ' + line)
        seen_target=True
if not seen_target:
    raise SystemExit('no target row')
print('summary ok')
PY
done

print_header "Layer 3: sweep-mode scalar-output smoke test"

CASE_SWEEP="${PROJECT_DIR}/${RUN_DIR}/sweep_smoke"
run_case \
  "Sweep mode with TID/DDD/n_eq/H100 output" \
  "${CASE_SWEEP}" \
  "${LOG_DIR}/70_sweep_run.log" \
  "${EXE_ABS}" \
    --sweep \
    --sweep-material=Al \
    --sweep-tmin=0.5 \
    --sweep-tmax=2.0 \
    --sweep-n=3 \
    --source-mode=beam \
    --spectrum="${DATA_DIR}/mono_100MeV_proton.dat" \
    --target=Si:1 \
    --events="${EVENTS_SMOKE}" \
    --random-seed=36001 \
    --output-prefix=sweep \
    --dump-run-summary=sweep_summary.dat \
    --quantities=TID,DDD,n_eq,H100/10

run_check \
  "Sweep output has three runs and increasing areal density" \
  "${LOG_DIR}/71_sweep_check.log" \
  python3 - "${CASE_SWEEP}" <<'PY'
import pathlib, sys, math
case=pathlib.Path(sys.argv[1])
summary=case/'sweep_summary.dat'
dose=case/'sweep_dose_sweep.dat'
quant=case/'sweep_quantities.dat'
for p in (summary,dose,quant):
    if not p.exists() or p.stat().st_size == 0:
        raise SystemExit('missing/empty ' + str(p))
areal=[]
blocks=0
for line in summary.read_text().splitlines():
    f=line.split()
    if not f:
        continue
    if f[0]=='begin_run':
        blocks += 1
    if len(f)>=3 and f[0]=='scalar' and f[1]=='shield_areal_density_g_cm2':
        areal.append(float(f[2]))
if blocks != 3 or len(areal) != 3:
    raise SystemExit(f'expected 3 sweep blocks and 3 areal values; got blocks={blocks}, areal={len(areal)}')
if not all(areal[i] < areal[i+1] for i in range(len(areal)-1)):
    raise SystemExit('areal density is not strictly increasing: ' + repr(areal))
print('areal densities=', areal)
PY

print_header "Layer 3 test summary"

echo "Total tests: ${N_TOTAL}"
echo "Passed:      ${N_PASS}"
echo "Failed:      ${N_FAIL}"
echo "Logs:        ${LOG_DIR}/"
echo "Run data:    ${RUN_DIR}/"

if [[ "${N_FAIL}" -eq 0 ]]; then
  echo "${GREEN}All Layer-3 tests passed.${RESET}"
  exit 0
else
  echo "${RED}Some Layer-3 tests failed.${RESET}"
  exit 1
fi
