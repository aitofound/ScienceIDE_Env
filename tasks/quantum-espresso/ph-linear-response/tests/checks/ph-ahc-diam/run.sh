#!/usr/bin/env bash
# Check runner for a Quantum ESPRESSO PHonon chain.
#   run.sh nominal | run.sh variant     one initial condition (see ic/)
#   run.sh altbuild                     nominal inputs, make.inc FFLAGS/CFLAGS -O3 -> -O0 -g -ffp-contract=off
#   run.sh --help                       knobs and the altbuild line
# Reads only CHECK_DIR and SOURCE_DIR; never modifies SOURCE_DIR; no network.

cpus_allowed() {
  local q p
  if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then
    echo $(( (q + p - 1) / p ))
  else
    nproc 2>/dev/null || getconf _NPROCESSORS_ONLN
  fi
}
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_OMP_THREADS "1" "OpenMP threads for QE; default 1 is the graded value"
knob SAB_ECUT_SCALE "1.0" "multiplies ecutwfc and ecutrho in every scf deck; default 1.0 is the graded value"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel make jobs for configure+make pw ph"
ALTBUILD="same source and the same ./configure --disable-parallel --enable-openmp git=true as nominal, then make.inc FFLAGS/CFLAGS -O3 replaced by -O0 -g -ffp-contract=off keeping -fallow-argument-mismatch and -fopenmp: a flags-preserving -O0 build a correct candidate could plausibly be"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
FLAVOR=default
if [ "$IC" = altbuild ]; then
  INPUTS=nominal
  FLAVOR=altbuild
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
CHAIN_JSON="$CHECK_DIR/chain.json"
[ -f "$CHAIN_JSON" ] || { echo "run.sh: missing chain.json" >&2; exit 2; }

export OMP_NUM_THREADS="${SAB_OMP_THREADS}"
export OPENBLAS_NUM_THREADS=1
export ESPRESSO_PSEUDO="$SOURCE_DIR/pseudo"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

TESTDIR="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["testdir"])' "$CHAIN_JSON")"

# Recorded configure line, identical in both Dockerfiles' comments and every
# check README. git=true is a dummy override of install/m4/x_ac_qe_git.m4:
# the image has no git (the apt line is curator-fixed) and the vendored tree
# has no .git, so the submodule block never runs and git is never invoked.
# This is not a network dependency.
CONFIG_ARGS=(--disable-parallel --enable-openmp git=true)
CONFIG_LINE="./configure ${CONFIG_ARGS[*]}"
# Cache lives in this container's /tmp only. Each solve.sh docker run is a new
# container, so nominal/variant/altbuild cannot see each other's binaries.
# Within one solve, later checks reuse the first check's build. The stamp is
# the configure line plus the exact FFLAGS/CFLAGS, so a default -O3 tree can
# never be reused as altbuild.
CACHE="/tmp/sciaccel-qe-ph/${FLAVOR}"
STAMP="$CACHE/configure.line"

expected_stamp() {
  if [ "$FLAVOR" = altbuild ]; then
    printf '%s\n' \
      "$CONFIG_LINE" \
      "flavor=altbuild" \
      "FFLAGS=-O0 -ffp-contract=off -g -fallow-argument-mismatch -fopenmp" \
      "CFLAGS=-O0 -g -ffp-contract=off"
  else
    printf '%s\n' \
      "$CONFIG_LINE" \
      "flavor=default" \
      "FFLAGS=-O3 -g -fallow-argument-mismatch -fopenmp" \
      "CFLAGS=-O3"
  fi
}

cache_hit() {
  [ -f "$CACHE/bin/pw.x" ] && [ ! -L "$CACHE/bin/pw.x" ] \
    && [ -f "$CACHE/bin/ph.x" ] && [ ! -L "$CACHE/bin/ph.x" ] \
    && [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$(expected_stamp)" ]
}

rewrite_altbuild_makeinc() {
  # Passing FFLAGS="-O0 ..." into configure replaces the whole flag set and
  # drops -fallow-argument-mismatch (gfortran >= 10, install/m4/x_ac_qe_f90.m4),
  # so LAXlib/ptoolkit.f90 dies with Type mismatch. Same configure as nominal,
  # then swap only the optimisation in make.inc.
  python3 - <<'PY'
from pathlib import Path
p = Path("make.inc")
lines = p.read_text(encoding="utf-8").splitlines(True)
out, changed = [], []
for line in lines:
    orig = line
    if line.startswith("FFLAGS         ="):
        line = line.replace("-O3", "-O0 -ffp-contract=off", 1)
    elif line.startswith("CFLAGS         ="):
        line = line.replace("-O3", "-O0 -g -ffp-contract=off", 1)
    if line != orig:
        changed.append(line.rstrip("\n"))
    out.append(line)
fflags = next((l for l in out if l.startswith("FFLAGS         =")), "")
cflags = next((l for l in out if l.startswith("CFLAGS         =")), "")
if len(changed) < 2:
    raise SystemExit("run.sh: altbuild expected to rewrite FFLAGS and CFLAGS -O3, got %r" % (changed,))
if "-fallow-argument-mismatch" not in fflags:
    raise SystemExit("run.sh: altbuild FFLAGS lost -fallow-argument-mismatch: %s" % fflags.rstrip())
if "-O3" in fflags or "-O3" in cflags:
    raise SystemExit("run.sh: altbuild still has -O3 in FFLAGS/CFLAGS")
if "-ffp-contract=off" not in fflags or "-ffp-contract=off" not in cflags:
    raise SystemExit("run.sh: altbuild missing -ffp-contract=off in FFLAGS/CFLAGS")
p.write_text("".join(out), encoding="utf-8")
print("run.sh: altbuild make.inc FFLAGS/CFLAGS:")
for c in changed:
    print(" ", c)
PY
}

build_tree() {
  local src="$1"
  cd "$src"
  # Offline-build trap 1: with every .git removed, install/install_utils tries
  # to git init/fetch external/devxlib and external/mbd. The two zero-byte
  # stamps are what QE itself writes after a successful update; create them in
  # THIS copy so a candidate's fresh tree never depends on them being vendored.
  mkdir -p install
  touch install/git_devx install/git_mbd
  if ! ./configure "${CONFIG_ARGS[@]}" > "$WORK/configure.log" 2>&1; then
    echo "run.sh: configure failed:" >&2
    tail -40 "$WORK/configure.log" >&2
    exit 1
  fi
  if [ "$FLAVOR" = altbuild ]; then
    rewrite_altbuild_makeinc
  fi
  if ! make -j"$SAB_MAKE_JOBS" pw ph > "$WORK/make.log" 2>&1; then
    echo "run.sh: make pw ph failed:" >&2
    grep -E 'Error [0-9]|Error:|error:|fatal error|Killed' "$WORK/make.log" | head -30 >&2 || true
    echo "run.sh: last 20 lines of make.log:" >&2
    tail -20 "$WORK/make.log" >&2
    exit 1
  fi
}

PREFIX=""
BUILD_START=$(date +%s)
if cache_hit; then
  PREFIX="$CACHE"
  echo "SAB_BUILD_SECONDS=0"
  echo "run.sh: reused $CACHE (stamp matches configure line + FFLAGS/CFLAGS; earlier check of this solve built it)" >&2
else
  mkdir -p "$WORK/src"
  cp -a "$SOURCE_DIR/." "$WORK/src/"
  build_tree "$WORK/src"
  echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
  # bin/*.x are symlinks into the build tree; copy the real binaries so a
  # later check of THIS container can reuse them after WORK is gone.
  mkdir -p "$CACHE/bin"
  for f in "$WORK/src/bin/"*.x; do
    [ -e "$f" ] || continue
    cp -L "$f" "$CACHE/bin/$(basename "$f")"
    chmod +x "$CACHE/bin/$(basename "$f")"
  done
  expected_stamp > "$STAMP"
  PREFIX="$WORK/src"
fi
[ -x "$PREFIX/bin/pw.x" ] && [ -x "$PREFIX/bin/ph.x" ] || { echo "run.sh: missing pw.x/ph.x under $PREFIX/bin" >&2; exit 1; }

RUN="$WORK/run/test-suite/$TESTDIR"
mkdir -p "$RUN" "$WORK/run"
ln -sfn "$SOURCE_DIR/pseudo" "$WORK/run/pseudo"
cp -a "$CHECK_DIR/ic/$INPUTS/." "$RUN/"

python3 - "$RUN" "$SAB_ECUT_SCALE" <<'PY'
import re, sys
from pathlib import Path
run, scale = Path(sys.argv[1]), float(sys.argv[2])
if abs(scale - 1.0) < 1e-15:
    raise SystemExit(0)
pat = re.compile(r"(?i)(ecut(?:wfc|rho)\s*=\s*)([0-9.eEdD+-]+)")
def mul(m):
    raw = m.group(2).replace("D", "e").replace("d", "e")
    return m.group(1) + format(float(raw) * scale, ".10g")
for p in run.iterdir():
    if not p.is_file():
        continue
    txt = p.read_text(encoding="utf-8", errors="replace")
    new = pat.sub(mul, txt)
    if new != txt:
        p.write_text(new, encoding="utf-8")
PY

cd "$RUN"
python3 - "$CHAIN_JSON" "$PREFIX" "$RUN" <<'PY'
import json, os, subprocess, sys
from pathlib import Path
chain = json.load(open(sys.argv[1]))
prefix, rundir = Path(sys.argv[2]), Path(sys.argv[3])
os.chdir(rundir)
for step in chain["steps"]:
    exe = step["exe"]
    inp = step["in"]
    binp = prefix / "bin" / exe
    if not binp.is_file():
        sys.stderr.write(f"run.sh: missing {binp}\n")
        raise SystemExit(1)
    if exe == "ph.x" and step.get("wipe_ph0", True):
        subprocess.run(["rm", "-rf", "_ph0"], check=True)
    out_name = step.get("stdout", Path(inp).name + ".out")
    with open(inp) as fin, open(out_name, "w") as fout:
        proc = subprocess.run([str(binp)], stdin=fin, stdout=fout, stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        sys.stderr.write(f"run.sh: {exe} < {inp} exited {proc.returncode}\n")
        try:
            sys.stderr.write(Path(out_name).read_text(encoding="utf-8", errors="replace")[-4000:])
        except OSError:
            pass
        raise SystemExit(1)
    if exe == "dynmat.x":
        Path("dynmat.out").write_bytes(Path(out_name).read_bytes())
    if exe == "lambda.x":
        Path("lambda.out").write_bytes(Path(out_name).read_bytes())
    # diam.nscf.in / diam.nscf.nosym.in share prefix='diam' and outdir='.'
    # with the scf step and overwrite diam.save/data-file-schema.xml. Snapshot
    # the scf XML immediately after that step's JOB DONE, before any nscf.
    if exe == "pw.x" and inp == "diam.scf.in":
        import shutil
        out_text = Path(out_name).read_text(encoding="utf-8", errors="replace")
        if "JOB DONE." not in out_text:
            sys.stderr.write("run.sh: pw.x < diam.scf.in did not print JOB DONE.\n")
            sys.stderr.write(out_text[-4000:])
            raise SystemExit(1)
        schema = Path("diam.save") / "data-file-schema.xml"
        if not schema.is_file():
            hits = list(Path(".").glob("**/data-file-schema.xml"))
            sys.stderr.write(
                f"run.sh: missing {schema} after the scf step JOB DONE; glob hits {hits}\n"
            )
            raise SystemExit(1)
        shutil.copy(schema, Path("scf-data-file-schema.xml"))
PY

python3 "$CHECK_DIR/extract.py" --run-dir "$RUN" --chain "$CHAIN_JSON" --out "$OUT_DIR/metrics.json"
