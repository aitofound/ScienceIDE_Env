#!/usr/bin/env bash
# Check mflampa-poisson: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STOP_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "1" "multiplies the nIterMax and tSimulationMax of every #STOP block of the deck (upstream: the graded window of the SP/MFLAMPA test); run time scales with it"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs on a build-cache miss or standalone fallback (default: the CPUs allowed to this container); ignored on cache reuse; changes build time only, never the graded run"
# No runnable alternative build: the x86_64 -O0 probe reaches MFLAMPA show_progress
# and raises SIGFPE because nProgress1=0 is used as a modulo divisor (see rubric.json).
ALTBUILD=""
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
exec < /dev/null                 # mpiexec must not read the produce driver's stdin
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
BUILD_MODE=o3
[ "$IC" != altbuild ] || BUILD_MODE=o0
BUILD_FAMILY="mflampa-g20000"
EXPECTED_REL="SP/MFLAMPA/bin/MFLAMPA.exe"
BUILD_RECIPE="format=sab-build-recipe-v1\n./Config.pl -install=BATSRUS -compiler=gfortran
optimization_mode=${BUILD_MODE}; o3=no additional command; o0=./Config.pl -O0
(cd SP/MFLAMPA && ./Config.pl -g=20000)
(cd SP/MFLAMPA && make -j${SAB_MAKE_JOBS})"

# Immutable task-local cache contract. The configured source tree is created at
# its final path and never relocated. A full source digest and build-platform
# digest are supplied once per solve by tests/test.sh; standalone invocation
# computes them here. The recipe digest below is check-family specific.
cache_sha_stdin() {
  python3 -c 'import hashlib,sys; print(hashlib.sha256(sys.stdin.buffer.read()).hexdigest())'
}

cache_tree() {
  # cache_tree digest ROOT | cache_tree freeze ROOT | cache_tree verify ROOT
  python3 - "$1" "$2" <<'PYCACHE_TREE'
import hashlib, os, pathlib, stat, sys
mode, root_arg = sys.argv[1:]
root = pathlib.Path(root_arg).resolve(strict=True)
if not root.is_dir():
    raise SystemExit(f"cache tree is not a directory: {root}")

def entries(path):
    with os.scandir(path) as scan:
        children = sorted(scan, key=lambda e: os.fsencode(e.name))
    for entry in children:
        p = pathlib.Path(entry.path)
        st = p.lstat()
        yield p, st
        if stat.S_ISDIR(st.st_mode):
            yield from entries(p)

rows = list(entries(root))
if mode == "freeze":
    for p, st in rows:
        if stat.S_ISREG(st.st_mode):
            os.chmod(p, 0o555 if st.st_mode & 0o111 else 0o444)
        elif not (stat.S_ISDIR(st.st_mode) or stat.S_ISLNK(st.st_mode)):
            raise SystemExit(f"special file cannot enter cache: {p}")
    for p, st in reversed(rows):
        if stat.S_ISDIR(st.st_mode):
            os.chmod(p, 0o555)
    os.chmod(root, 0o555)
    rows = list(entries(root))
elif mode not in ("digest", "verify"):
    raise SystemExit(f"unknown cache_tree mode: {mode}")

h = hashlib.sha256()
h.update(b"sab-immutable-tree-v1\0")
all_rows = [(root, root.lstat())] + rows
for p, st in all_rows:
    rel = b"." if p == root else os.fsencode(str(p.relative_to(root)))
    kind = (b"d" if stat.S_ISDIR(st.st_mode) else b"f" if stat.S_ISREG(st.st_mode)
            else b"l" if stat.S_ISLNK(st.st_mode) else b"x")
    if kind == b"x":
        raise SystemExit(f"special file cannot enter cache: {p}")
    perms = stat.S_IMODE(st.st_mode)
    if mode == "verify" and kind in (b"d", b"f") and perms & 0o222:
        raise SystemExit(f"writable cache path: {p}")
    h.update(kind + b"\0" + rel + b"\0" + f"{perms:04o}".encode() + b"\0")
    if kind == b"f":
        h.update(str(st.st_size).encode() + b"\0")
        with p.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                h.update(chunk)
    elif kind == b"l":
        target_text = os.readlink(p)
        h.update(os.fsencode(target_text))
        target = pathlib.Path(target_text)
        if not target.is_absolute():
            target = p.parent / target
        resolved = target.resolve(strict=False)
        try:
            resolved.relative_to(root)
        except ValueError:
            if mode in ("freeze", "verify"):
                raise SystemExit(f"external cache symlink: {p} -> {target_text}")
    h.update(b"\0")
print(h.hexdigest())
PYCACHE_TREE
}

cache_local_context() {
  python3 - <<'PYCACHE_CONTEXT'
import hashlib, json, os, pathlib, platform, shutil, subprocess
names = ["bash", "sh", "make", "perl", "python3", "git", "gfortran", "gcc", "g++",
         "mpifort", "mpif90", "mpicc", "mpicxx", "ar", "ranlib", "ld", "cpp"]
rows = {"format": "sab-build-context-v1", "platform": platform.uname()._asdict(),
        "environment": {k: os.environ.get(k, "") for k in
                        ("PATH", "HOME", "LANG", "LC_ALL", "CC", "CXX", "FC", "F77", "F90", "MPIFC")},
        "tools": {}}
for name in names:
    path = shutil.which(name)
    row = {"resolved": path or "<missing>"}
    if path:
        real = os.path.realpath(path)
        row["realpath"] = real
        try:
            row["sha256"] = hashlib.sha256(pathlib.Path(real).read_bytes()).hexdigest()
        except OSError as exc:
            row["sha256_error"] = type(exc).__name__
        try:
            p = subprocess.run([path, "--version"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=3, env={"PATH": os.environ.get("PATH", ""), "LANG": "C"})
            row["version_sha256"] = hashlib.sha256(p.stdout).hexdigest()
            row["version_status"] = p.returncode
        except (OSError, subprocess.TimeoutExpired) as exc:
            row["version_error"] = type(exc).__name__
    rows["tools"][name] = row
for label, filename in (("os_release", "/etc/os-release"), ("cpuinfo", "/proc/cpuinfo")):
    try:
        rows[label] = hashlib.sha256(pathlib.Path(filename).read_bytes()).hexdigest()
    except OSError:
        rows[label] = "<unavailable>"
try:
    p = subprocess.run(["dpkg-query", "-W", "-f=${Package}=${Version}\\n"], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=10,
                       env={"PATH": os.environ.get("PATH", ""), "LANG": "C"})
    rows["packages_sha256"] = hashlib.sha256(p.stdout).hexdigest()
    rows["packages_status"] = p.returncode
except (OSError, subprocess.TimeoutExpired) as exc:
    rows["packages_error"] = type(exc).__name__
blob = json.dumps(rows, sort_keys=True, separators=(",", ":")).encode()
print(hashlib.sha256(blob).hexdigest())
PYCACHE_CONTEXT
}

cache_manifest_content() {
  python3 - "$1" "$BUILD_KEY" "$SAB_BUILD_SOURCE_SHA256" "$SAB_BUILD_CONTEXT_SHA256" \
    "$BUILD_RECIPE_SHA256" "$EXPECTED_REL" "$BUILD_SRC" <<'PYCACHE_MANIFEST'
import json, os, sys
content, key, source, context, recipe, expected, artifact = sys.argv[1:]
print(json.dumps({"artifact_realpath": os.path.realpath(artifact), "content_sha256": content,
                  "context_sha256": context, "expected_executable": expected,
                  "format": "sab-immutable-build-v1", "key": key,
                  "recipe_sha256": recipe, "source_sha256": source},
                 sort_keys=True, separators=(",", ":")))
PYCACHE_MANIFEST
}

cache_verify() {
  local recorded actual
  [ -f "$CACHE_READY" ] || { echo "run.sh: cache has no atomic ready manifest: $CACHE_READY" >&2; return 1; }
  recorded="$(python3 - "$CACHE_READY" "$BUILD_KEY" "$SAB_BUILD_SOURCE_SHA256" \
    "$SAB_BUILD_CONTEXT_SHA256" "$BUILD_RECIPE_SHA256" "$EXPECTED_REL" "$BUILD_SRC" <<'PYCACHE_VERIFY'
import json, os, pathlib, sys
ready, key, source, context, recipe, expected, artifact = sys.argv[1:]
try:
    doc = json.loads(pathlib.Path(ready).read_text(encoding="utf-8"))
except (OSError, ValueError) as exc:
    raise SystemExit(f"unreadable ready manifest: {exc}")
want = {"artifact_realpath": os.path.realpath(artifact), "context_sha256": context,
        "expected_executable": expected, "format": "sab-immutable-build-v1", "key": key,
        "recipe_sha256": recipe, "source_sha256": source}
if set(doc) != set(want) | {"content_sha256"}:
    raise SystemExit("ready manifest has unexpected fields")
for field, value in want.items():
    if doc.get(field) != value:
        raise SystemExit(f"ready manifest mismatch: {field}")
content = doc.get("content_sha256")
if not isinstance(content, str) or len(content) != 64 or any(c not in "0123456789abcdef" for c in content):
    raise SystemExit("ready manifest has invalid content_sha256")
print(content)
PYCACHE_VERIFY
  )" || return 1
  [ -f "$BUILD_SRC/$EXPECTED_REL" ] && [ -x "$BUILD_SRC/$EXPECTED_REL" ] || {
    echo "run.sh: immutable cache lacks expected executable: $BUILD_SRC/$EXPECTED_REL" >&2; return 1;
  }
  actual="$(cache_tree verify "$BUILD_SRC")" || return 1
  [ "$actual" = "$recorded" ] || {
    echo "run.sh: immutable cache content mismatch: $BUILD_SRC" >&2; return 1;
  }
}

cache_mark_failed() {
  local status="$1" pending
  [ "$status" -ne 0 ] || return 0
  [ "${CACHE_ACTIVE:-0}" -eq 1 ] && [ "${CACHE_OWNER:-0}" -eq 1 ] || return 0
  [ ! -e "${CACHE_READY:-}" ] && [ ! -e "${CACHE_FAILED:-}" ] || return 0
  pending="$CACHE_STATE/failed.pending.$$"
  [ ! -e "$pending" ] || return 0
  printf '{"format":"sab-immutable-build-failure-v1","key":"%s","owner_status":%s}\n' \
    "$BUILD_KEY" "$status" > "$pending" || return 0
  chmod 0444 "$pending" || return 0
  # Like ready publication, a hard link is atomic and cannot overwrite.
  ln "$pending" "$CACHE_FAILED" 2>/dev/null || true
}

cache_exit() {
  local status="$1"
  trap - EXIT
  cache_mark_failed "$status"
  rm -rf "$WORK"
  exit "$status"
}

cache_prepare() {
  local identity claim_index
  : "${BUILD_FAMILY:?}" "${BUILD_MODE:?}" "${BUILD_RECIPE:?}" "${EXPECTED_REL:?}"
  if [ -z "${SAB_BUILD_SOURCE_SHA256:-}" ]; then
    SAB_BUILD_SOURCE_SHA256="$(cache_tree digest "$SOURCE_DIR")"
  fi
  if [ -z "${SAB_BUILD_CONTEXT_SHA256:-}" ]; then
    SAB_BUILD_CONTEXT_SHA256="$(cache_local_context)"
  fi
  BUILD_RECIPE_SHA256="$(printf '%s' "$BUILD_RECIPE" | cache_sha_stdin)"
  BUILD_REUSED=0; CACHE_ACTIVE=0; CACHE_OWNER=0
  trap 'cache_exit $?' EXIT
  if [ -z "${SAB_SHARED_BUILD_ROOT:-}" ]; then
    BUILD_KEY="$BUILD_FAMILY-$BUILD_MODE-standalone"
    BUILD_SRC="$WORK/src"
    cp -R "$SOURCE_DIR/." "$BUILD_SRC"
    return
  fi
  [ -d "$SAB_SHARED_BUILD_ROOT" ] || {
    echo "run.sh: shared build root is not a directory: $SAB_SHARED_BUILD_ROOT" >&2; exit 1;
  }
  SAB_SHARED_BUILD_ROOT="$(cd "$SAB_SHARED_BUILD_ROOT" && pwd -P)"
  identity="$(printf '%s\0' "sab-immutable-build-key-v1" "$BUILD_FAMILY" "$BUILD_MODE" \
    "$SAB_BUILD_SOURCE_SHA256" "$SAB_BUILD_CONTEXT_SHA256" "$BUILD_RECIPE_SHA256" \
    "$SAB_MAKE_JOBS" "$SAB_SHARED_BUILD_ROOT" | cache_sha_stdin)"
  BUILD_KEY="$BUILD_FAMILY-$BUILD_MODE-$identity"
  BUILD_SRC="$SAB_SHARED_BUILD_ROOT/artifacts/$BUILD_KEY"
  CACHE_STATE="$SAB_SHARED_BUILD_ROOT/state/$BUILD_KEY"
  CACHE_READY="$CACHE_STATE/ready.json"
  CACHE_FAILED="$CACHE_STATE/failed.json"
  mkdir -p "$SAB_SHARED_BUILD_ROOT/artifacts" "$CACHE_STATE"
  CACHE_ACTIVE=1
  if [ -f "$CACHE_READY" ]; then
    cache_verify
    BUILD_REUSED=1
    return
  fi
  [ ! -f "$CACHE_FAILED" ] || {
    echo "run.sh: refusing cache whose owner recorded failure: $BUILD_KEY" >&2; exit 1;
  }
  if mkdir "$CACHE_STATE/claim" 2>/dev/null; then
    [ ! -e "$BUILD_SRC" ] && [ ! -e "$CACHE_READY" ] && [ ! -e "$CACHE_FAILED" ] || {
      echo "run.sh: owner refuses pre-existing incomplete cache state: $BUILD_KEY" >&2; exit 1;
    }
    CACHE_OWNER=1
    mkdir "$BUILD_SRC"
    cp -R "$SOURCE_DIR/." "$BUILD_SRC/"
    return
  fi
  claim_index=0
  while [ ! -f "$CACHE_READY" ]; do
    [ ! -f "$CACHE_FAILED" ] || {
      echo "run.sh: cache owner failed; incomplete artifact is not reusable: $BUILD_KEY" >&2; exit 1;
    }
    [ "$claim_index" -lt 3600 ] || {
      echo "run.sh: timed out waiting for cache owner; incomplete state is not reusable: $BUILD_KEY" >&2; exit 1;
    }
    sleep 0.5
    claim_index=$((claim_index + 1))
  done
  cache_verify
  BUILD_REUSED=1
}

cache_publish() {
  local content pending
  [ "$CACHE_ACTIVE" -eq 1 ] && [ "$CACHE_OWNER" -eq 1 ] || return 0
  [ -f "$BUILD_SRC/$EXPECTED_REL" ] && [ -x "$BUILD_SRC/$EXPECTED_REL" ] || {
    echo "run.sh: successful build did not create expected executable: $BUILD_SRC/$EXPECTED_REL" >&2; exit 1;
  }
  content="$(cache_tree freeze "$BUILD_SRC")"
  pending="$CACHE_STATE/ready.pending.$$"
  [ ! -e "$pending" ] && [ ! -e "$CACHE_READY" ] || {
    echo "run.sh: refusing conflicting ready publication state: $BUILD_KEY" >&2; exit 1;
  }
  cache_manifest_content "$content" > "$pending"
  chmod 0444 "$pending"
  # Hard-link creation is atomic and cannot overwrite a competing ready path.
  ln "$pending" "$CACHE_READY" || {
    echo "run.sh: atomic ready publication failed: $BUILD_KEY" >&2; exit 1;
  }
  cache_verify
}

cache_prepare
# MFLAMPA's rundir recipe writes temporary JobScripts through ${DIR}. Keep the
# configured cache at its original absolute path, but override DIR to a private
# support tree; SPDIR/BINDIR continue to read the immutable configured artifact.
private_mflampa_rundir() {
  local run_dir="$1" log_mode="$2"
  RUNDIR_SUPPORT="$WORK/rundir-support-${run_dir##*/}"
  mkdir -p "$RUNDIR_SUPPORT/share/JobScripts" "$RUNDIR_SUPPORT/GM/BATSRUS/data/TRAJECTORY"
  cp -R "$BUILD_SRC/share/JobScripts/." "$RUNDIR_SUPPORT/share/JobScripts/"
  if [ "$log_mode" = append ]; then
    make -C "$BUILD_SRC/SP/MFLAMPA" rundir DIR="$RUNDIR_SUPPORT" RUNDIR="$run_dir" \
      STANDALONE=YES SPDIR="$BUILD_SRC/SP/MFLAMPA" >> "$WORK/rundir.log" 2>&1
  else
    make -C "$BUILD_SRC/SP/MFLAMPA" rundir DIR="$RUNDIR_SUPPORT" RUNDIR="$run_dir" \
      STANDALONE=YES SPDIR="$BUILD_SRC/SP/MFLAMPA" > "$WORK/rundir.log" 2>&1
  fi
}

export LC_ALL=C OMP_NUM_THREADS=1
export GIT_TERMINAL_PROMPT=0     # Config.pl -install tries to clone srcUserExtra; it must fail fast, not prompt

# Rewrite one deck into the run directory, rescaling every #STOP window and the
# #ENDTIME of a deck that has one by SAB_STOP_SCALE. At the graded default of 1
# the deck is copied through unchanged.
deck() {
python3 - "$1" "$2" "$SAB_STOP_SCALE" <<'PY'
import datetime, sys
src, dst, scale = sys.argv[1], sys.argv[2], float(sys.argv[3])
lines = open(src, encoding="utf-8").read().split("\n")

def rewrite(index, value, integer):
    parts = lines[index].split(None, 1)
    tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
    lines[index] = (("%d" % value) if integer else ("%.10g" % value)) + tail

def clock(index):
    return [int(float(lines[index + k].split()[0])) for k in range(1, 7)]

if scale != 1.0:
    for i, line in enumerate(list(lines)):
        if line.strip() == "#STOP":
            for k, integer in ((i + 1, True), (i + 2, False)):
                if k >= len(lines) or not lines[k].split():
                    break
                try:
                    value = float(lines[k].split()[0])
                except ValueError:
                    break
                if value > 0:
                    rewrite(k, max(1, int(round(value * scale))) if integer else value * scale, integer)
        elif line.strip() == "#ENDTIME":
            start = max(j for j in range(i) if lines[j].strip() == "#STARTTIME")
            t0 = datetime.datetime(*clock(start))
            t1 = t0 + (datetime.datetime(*clock(i)) - t0) * scale
            for k, value in zip(range(i + 1, i + 7), (t1.year, t1.month, t1.day, t1.hour, t1.minute, t1.second)):
                rewrite(k, value, True)
open(dst, "w", encoding="utf-8").write("\n".join(lines))
PY
}

# The graded files, under the fixed names rubric.json lists.
grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  case "$last" in
    *.gz) gunzip -c "$last" > "$OUT_DIR/$dest" ;;
    *) cp "$last" "$OUT_DIR/$dest" ;;
  esac
}

# Upstream test this check reproduces: code/swmf/SP/MFLAMPA/Param/PARAM.in.test.poisson
# Build: the SWMF Config.pl installs the tree (it writes Makefile.conf and
# Makefile.def with this working copy's path and links SWMF_data), then
# SP/MFLAMPA's own Config.pl sets the vertex/line grid of the upstream
# `test_compile` target and `make` builds bin/MFLAMPA.exe standalone.
if [ "$BUILD_REUSED" -eq 0 ]; then
  cd "$BUILD_SRC"
  BUILD_START=$(date +%s)
  ./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/build.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
  fi
  cd SP/MFLAMPA
  ./Config.pl -g=20000 >> "$WORK/build.log" 2>&1
  make -j"$SAB_MAKE_JOBS" >> "$WORK/build.log" 2>&1
  cache_publish
  echo "SAB_BUILD_CACHE=miss key=$BUILD_KEY"
  echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
else
  echo "SAB_BUILD_CACHE=hit key=$BUILD_KEY"
  echo "SAB_BUILD_SECONDS=0"
fi
cd "$WORK"

# Run directory exactly as the upstream test builds it, with all writes private.
private_mflampa_rundir "$WORK/run" truncate
tar xzf "$CHECK_DIR/input/MH_data_e20120123.tgz" -C "$WORK/run"
deck "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$WORK/run/PARAM.in"
cd "$WORK/run"
if ! mpiexec -n 2 --oversubscribe --bind-to none ./MFLAMPA.exe > runlog 2>&1 < /dev/null; then
  echo "run.sh: MFLAMPA.exe failed; last lines of the run log follow" >&2
  tail -40 runlog >&2
  exit 1
fi
cat SP/IO2/MH_data_{*???_???,*n000006}.out > "$OUT_DIR/MH_data.outs"