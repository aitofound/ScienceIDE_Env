#!/usr/bin/env bash
# Check pkp-1d-displacement-waveform: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STEPS=20 sab.py task selfcheck ... Declare every setting that
# scales this check's runtime, one knob per line.
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NFREQ "64" "number of angular frequencies solved (dsm_model field 2 and Par_file_freq2sac line 2); wall time and peak memory scale linearly; also sets fmax = SAB_NFREQ / 2401 Hz, so changing it changes the graded waveform and invalidates the reference"
knob SAB_RANKS "4" "MPI ranks the solver is launched with; frequencies are distributed over ranks, so wall time scales roughly as 1/SAB_RANKS while peak memory scales as SAB_RANKS * ~1.9 GB (measured by sampling RSS every 2 s: 4 ranks peak at ~1.88 GB each, ~7.56 GB summed)"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same sources rebuilt with FMA contraction enabled (-O2 -mfma for both Makefiles, keeping the SAC-side -fallow-argument-mismatch -std=legacy), which a correct candidate could plausibly ship on any FMA-capable x86-64 host: contracting a*b+c into a single rounding genuinely changes the arithmetic, so this build is not vacuous. Measured here: 309 s vs 314 s nominal, and every graded value moves. Plain -O0 was tried first and is NOT usable as an altbuild for this code: gfortran preserves IEEE semantics without -ffast-math, so -O and -O0 produce bit-identical output."
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/sem-dsm-hybrid/example/PKP_precursor_ULVZ_demo/Explosion_demo/OUTPUT_FILES_0.5Hz_DSM1D/1D_DSM
SOLVER_DIR="$WORK/src/src/DSM/src/DSM_Solver"
SAC_DIR="$WORK/src/src/DSM/src/DSM_FreqToTimeSac"
# The two Makefiles do NOT carry the same flags, and overriding FFLAGS wholesale breaks the
# second one: DSM_Solver ships `FFLAGS = -O`, while DSM_FreqToTimeSac ships
# `FFLAGS = -O3 -fallow-argument-mismatch -std=legacy` because sacconv.f passes a COMPLEX(8)
# array to a REAL(8) dummy argument -- modern gfortran rejects that as an error unless those
# legacy flags survive. So vary ONLY the optimisation level and keep each Makefile's
# remaining flags intact; the defaults below are exactly the upstream ones.
SOLVER_OPT="-O"
SAC_OPT="-O3"
SAC_COMPAT="-fallow-argument-mismatch -std=legacy"
if [ "$IC" = altbuild ]; then SOLVER_OPT="-O2 -mfma"; SAC_OPT="-O2 -mfma"; fi

BUILD_START=$(date +%s)
# The upstream tree ships stale .o/.mod files and prebuilt executables next to the sources.
# git does not preserve mtimes, so make would consider them up to date and either relink the
# stale objects or skip the link entirely -- FFLAGS would then have no effect at all. Remove
# them first so both builds are honest, and so the altbuild flags really produce a different binary.
for d in "$SOLVER_DIR" "$SAC_DIR"; do
  [ -d "$d" ] || { echo "run.sh: missing source directory $d" >&2; exit 2; }
  rm -f "$d"/*.o "$d"/*.mod
done
rm -f "$SOLVER_DIR/dsmti" "$SAC_DIR/spectotime"
make -C "$SOLVER_DIR" FFLAGS="$SOLVER_OPT" >&2
make -C "$SAC_DIR"    FFLAGS="$SAC_OPT $SAC_COMPAT" >&2
[ -x "$SOLVER_DIR/dsmti" ]   || { echo "run.sh: dsmti was not produced" >&2; exit 1; }
[ -x "$SAC_DIR/spectotime" ] || { echo "run.sh: spectotime was not produced" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run the configuration from ic/$INPUTS. Both executables resolve their inputs through
# hard-coded relative paths (DATA/dsm_model, DATA/Par_file_freq2sac), so they must be
# launched from the run directory, and spectotime does NOT create the SAC output
# directory named in Par_file_freq2sac -- it prints "Job completed!" and exits 0 while
# writing nothing. Both directories are created here on purpose.
RUN="$WORK/run"
# savespec.f opens every output file through a hard-coded relative path such as
# './OUTPUT_FILES/disp_solid/freq_00001' with status='unknown' and stops with
# "error saving GREENS" when the directory is absent, so every directory it can open
# must exist before the solver starts. spectotime likewise does not create the SAC
# directory named in Par_file_freq2sac.
mkdir -p "$RUN"
for d in disp_solid velo_solid disp_fluid stress pressure potential coef_cAnddcdr disp_solid_time_sac; do
  mkdir -p "$RUN/OUTPUT_FILES/$d"
done
cp -R "$CHECK_DIR/ic/$INPUTS/DATA" "$RUN/DATA"
cp "$SOLVER_DIR/dsmti" "$SAC_DIR/spectotime" "$RUN/"

# Apply the frequency knob to both files that must agree on it.
awk -v n="$SAB_NFREQ" 'NR==1{print $1, n; next} {print}' "$RUN/DATA/dsm_model" > "$RUN/DATA/dsm_model.tmp"
mv "$RUN/DATA/dsm_model.tmp" "$RUN/DATA/dsm_model"
awk -v n="$SAB_NFREQ" 'NR==2{print n " # Number of frequencies"; next} {print}' "$RUN/DATA/Par_file_freq2sac" > "$RUN/DATA/Par_file_freq2sac.tmp"
mv "$RUN/DATA/Par_file_freq2sac.tmp" "$RUN/DATA/Par_file_freq2sac"

( cd "$RUN" && mpirun --allow-run-as-root -np "$SAB_RANKS" ./dsmti < DATA/dsm_model >&2 )
( cd "$RUN" && ./spectotime >&2 )

# Copy the graded output files into OUT_DIR, named exactly as rubric.json lists them.
# Only the vertical and radial components are graded: for an isotropic explosive source
# with the receivers on the source azimuth the transverse component is identically zero,
# and a constant-zero array grades nothing.
SACOUT="$RUN/OUTPUT_FILES/disp_solid_time_sac"
for f in D129998_L12_dep0.00_dist130.00.bhz D129998_L12_dep0.00_dist130.00.bhr \
         D130000_L12_dep0.00_dist130.00.bhz D130000_L12_dep0.00_dist130.00.bhr; do
  [ -s "$SACOUT/$f" ] || { echo "run.sh: expected graded file missing or empty: $f" >&2; ls -la "$SACOUT" >&2; exit 1; }
  cp "$SACOUT/$f" "$OUT_DIR/$f"
done
