#!/usr/bin/env bash
# Check slab-pp-reflection: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime and resource knobs, and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# 1-D teleseismic synthetics (step 6) of the slab PP-reflection case (explosion): dsmti over SAB_NFREQ frequencies, then spectotime to SAC.

# Runtime and resource knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NFREQ=4 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NFREQ "16" "angular frequencies solved, i = 1..SAB_NFREQ at omega_i = 2 pi i / T with T = 3000 s (field 2 of line 1 of dsm_model, also the count spectotime reads); the graded band is f <= SAB_NFREQ/T = 0.005333 Hz; one radial solve per frequency and angular order, so run time scales linearly; must be a power of two (spectotime's FFT aborts with a corrupted heap at 24 or 6 frequencies, measured); changing it changes the graded output and invalidates the reference"
knob SAB_RANKS "8" "MPI ranks the solver runs on, the declared per-check cpus (never read from the host); frequencies are dealt to ranks round robin and each is solved by one rank alone with no reduction, so the rank count changes wall time only, never a graded value (freq_00001 measured bit-identical between 1 and 4 ranks)"
# Alternative build: the same sources rebuilt with FMA contraction enabled. gfortran keeps IEEE semantics
# at every -O level without -ffast-math, so -O0 against -O is bit-identical here (measured by the first
# packager); -O2 -mfma folds a*b+c into one rounding and really moves the arithmetic.
ALTBUILD="the same two Fortran sources rebuilt with FMA contraction enabled (-O2 -mfma on both Makefiles, the SAC side keeping -fallow-argument-mismatch -std=legacy) instead of the upstream -O and -O3; a correct candidate could plausibly ship an FMA-contracted build on any FMA-capable x86-64 host, and contracting a*b+c into one rounding really changes the arithmetic (the first packager measured -O against -O0 bit-identical, so that pair is not usable as an alternative build here)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS/DATA" ] || { echo "run.sh: no initial condition ic/$INPUTS/DATA" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Upstream test this check reproduces: code/sem-dsm-hybrid/example/Slab_PP_Reflection/Slab_PP_model1/OUTPUT_FILES_DSM3D_0.5Hz/1D_DSM

# ---- build: the two module directories only (dsmti, and spectotime for the seismograms) ----------------
SOLVER_SRC="$SOURCE_DIR/src/DSM/src/DSM_Solver"; SAC_SRC="$SOURCE_DIR/src/DSM/src/DSM_FreqToTimeSac"
[ -d "$SOLVER_SRC" ] && [ -d "$SAC_SRC" ] || { echo "run.sh: $SOURCE_DIR lacks src/DSM/src/DSM_Solver or src/DSM/src/DSM_FreqToTimeSac" >&2; exit 2; }
mkdir -p "$WORK/src"; cp -R "$SOLVER_SRC" "$WORK/src/DSM_Solver"; cp -R "$SAC_SRC" "$WORK/src/DSM_FreqToTimeSac"
# Upstream commits stale .o/.mod files and prebuilt executables next to the sources. git keeps no mtimes, so
# make would consider them current, relink them and ignore the flags; remove them so every build is honest.
rm -f "$WORK"/src/*/*.o "$WORK"/src/*/*.mod "$WORK/src/DSM_Solver/dsmti" "$WORK/src/DSM_FreqToTimeSac/spectotime"
# The two Makefiles carry different flags: DSM_Solver ships FFLAGS=-O; DSM_FreqToTimeSac ships
# -O3 -fallow-argument-mismatch -std=legacy because sacconv.f passes COMPLEX(8) arrays to REAL(8) dummies,
# which gfortran 13 rejects without the legacy flags. Only the optimisation level is varied.
SOLVER_OPT="-O"; SAC_OPT="-O3"; SAC_COMPAT="-fallow-argument-mismatch -std=legacy"
if [ "$IC" = altbuild ]; then SOLVER_OPT="-O2 -mfma"; SAC_OPT="-O2 -mfma"; fi
BUILD_START=$(date +%s)
make -C "$WORK/src/DSM_Solver" FFLAGS="$SOLVER_OPT" >&2
make -C "$WORK/src/DSM_FreqToTimeSac" FFLAGS="$SAC_OPT $SAC_COMPAT" >&2
[ -x "$WORK/src/DSM_FreqToTimeSac/spectotime" ] || { echo "run.sh: spectotime was not produced" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only
DSMTI="$WORK/src/DSM_Solver/dsmti"; SPECTOTIME="$WORK/src/DSM_FreqToTimeSac/spectotime"
[ -x "$DSMTI" ] || { echo "run.sh: dsmti was not produced" >&2; exit 1; }

# ---- MPI launcher --------------------------------------------------------------------------------------
# Every rank runs on this host. OpenMPI's default transport list includes TCP and, before the first message,
# it probes every interface it sees (docker bridges, veth pairs); on a host with such interfaces that is 150 s
# of dead time per launch (measured natively on the packaging host: 150-178 s before the first frequency with
# the defaults, 0.4 s with the on-node transports). Pinning the launch to self,vader changes no graded value.
MPIRUN=(mpirun --allow-run-as-root --bind-to none --oversubscribe --mca btl self,vader --mca btl_vader_single_copy_mechanism none)

# ---- helpers --------------------------------------------------------------------------------------------
prep_run() {  # $1 run directory, $2 DATA directory to copy; applies SAB_NFREQ to line 1 of dsm_model
  mkdir -p "$1"; cp -R "$2" "$1/DATA"
  # savespec.f opens every output file through a hard-coded relative path with status='unknown' and stops with
  # "error saving GREENS" when the directory is absent, so every directory it can open must exist first.
  for d in disp_solid velo_solid disp_fluid stress pressure potential coef_cAnddcdr; do mkdir -p "$1/OUTPUT_FILES/$d"; done
  awk -v n="$SAB_NFREQ" 'NR==1{print $1, n, " #time_series_length n_frequency"; next} {print}' "$1/DATA/dsm_model" > "$1/DATA/dsm_model.tmp"
  mv "$1/DATA/dsm_model.tmp" "$1/DATA/dsm_model"
}
run_dsmti() {  # $1 run directory, $2 ranks; dsmti reads the deck from stdin and its tables through DATA/ paths
  ( cd "$1" && "${MPIRUN[@]}" -np "$2" "$DSMTI" < DATA/dsm_model >&2 )
}
sac_convert() {  # $1 run directory: what run_freq_to_time_sac.sh writes (three components, solid displacement), then spectotime
  ( cd "$1"
    local ndep ndist tl oi sd
    ndep=$(awk 'NR==2 {print $1}' DATA/depth_solid_list); ndist=$(awk 'NR==1 {print $1}' DATA/dist_solid_list)
    tl=$(awk 'NR==1 {print $1}' DATA/dsm_model); oi=$(awk 'NR==2 {print $1}' DATA/dsm_model)
    sd=$(awk 'NR==4 {nz=$1; sl=NR+6*nz+1} sl>0 && NR==sl {print $1; exit}' DATA/dsm_model)
    [ -n "$sd" ] || { echo "run.sh: could not read the source depth from DATA/dsm_model" >&2; exit 1; }
    # spectotime does not create the SAC directory: it prints "Job completed!" and exits 0 having written nothing.
    mkdir -p OUTPUT_FILES/disp_solid_time_sac
    printf '%s # Number of stations (ndep * ndist)\n%s # Number of frequencies\n%s # Total time length in seconds\n%s 0.0 0.0 # Source depth (km), source latitude, source longitude\n3 # Number of components\n%s # Imaginary part of omega\n"OUTPUT_FILES/disp_solid" # Frequency-domain input directory\n"OUTPUT_FILES/disp_solid_time_sac" # SAC output directory\n' \
      "$((ndep * ndist))" "$SAB_NFREQ" "$tl" "$sd" "$oi" > DATA/Par_file_freq2sac
    # Station names as the upstream runner forms them (L<zone>_dep<depth> dist<distance>) but with five decimals
    # of distance: the upstream two decimals give two receivers 0.002 degrees apart the same SAC filename, and
    # spectotime silently overwrites the first with the second.
    awk 'NR==FNR { if (FNR>2) {ndep++; depth[ndep]=$1; zone[ndep]=$2}; next }
         FNR>1 { ndist++; dist[ndist]=$1 }
         END { for (i=1;i<=ndep;i++) for (j=1;j<=ndist;j++) printf "L%d_dep%.2f dist%.5f %.5f %.1f\n", zone[i], depth[i], dist[j], dist[j], 0.0 }' \
      DATA/depth_solid_list DATA/dist_solid_list > DATA/station_list
    "$SPECTOTIME" >&2 )
}
copy_spectra() {  # $1 run directory, $2 output subdirectory (disp_solid, velo_solid, ...), $3 destination directory
  local i f
  mkdir -p "$3"
  for i in $(seq 1 "$SAB_NFREQ"); do
    f=$(printf 'freq_%05d' "$i")
    [ -s "$1/OUTPUT_FILES/$2/$f" ] || { echo "run.sh: expected graded file missing or empty: $2/$f" >&2; ls -la "$1/OUTPUT_FILES/$2" >&2; exit 1; }
    cp "$1/OUTPUT_FILES/$2/$f" "$3/$f"
  done
}
copy_sac() {  # $1 run directory, $2 destination directory, $3... extensions graded (.bhz .bhr .bht)
  local run=$1 dest=$2 net sta rest ext; shift 2
  mkdir -p "$dest"
  while read -r net sta rest; do
    for ext in "$@"; do
      [ -s "$run/OUTPUT_FILES/disp_solid_time_sac/${net}_${sta}${ext}" ] || { echo "run.sh: expected SAC file missing or empty: ${net}_${sta}${ext}" >&2; ls -la "$run/OUTPUT_FILES/disp_solid_time_sac" >&2; exit 1; }
      cp "$run/OUTPUT_FILES/disp_solid_time_sac/${net}_${sta}${ext}" "$dest/${net}_${sta}${ext}"
    done
  done < "$run/DATA/station_list"
}

# ---- the run --------------------------------------------------------------------------------------------
RUN="$WORK/run"
prep_run "$RUN" "$CHECK_DIR/ic/$INPUTS/DATA"
run_dsmti "$RUN" "$SAB_RANKS"
sac_convert "$RUN"
copy_spectra "$RUN" disp_solid "$OUT_DIR/disp_solid"
copy_sac "$RUN" "$OUT_DIR/sac" .bhz .bhr
