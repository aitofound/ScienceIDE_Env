#!/bin/sh
# Trusted CPU oracle preparation. This worker is called only inside the pinned
# Docker reference image; it uses only the vendored source and check manifests.
# It never downloads source or runs a host-native solver.
set -eu
LEAF=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ORACLE_DIR=${PLUTO_ORACLE_DIR:-"$LEAF/solution/oracle"}
SELF_TEST_CANDIDATE_DIR=${PLUTO_SELF_TEST_CANDIDATE_DIR:-"$LEAF/solution/self-test-candidate"}
JOBS=${PLUTO_MAKE_JOBS:-2}

# Verify the complete extracted source boundary before any build side effect.
python3 - "$LEAF" <<'PY'
import json
import pathlib
import re
import sys

leaf = pathlib.Path(sys.argv[1])
manifest_path = leaf / 'solution/source-manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
src = leaf / 'code/pluto'
if not src.is_dir() or src.is_symlink():
    raise SystemExit('source boundary missing or symlinked')
if manifest.get('source_root') != 'code/pluto':
    raise SystemExit('source manifest names an unexpected source root')
archive_sha = manifest.get('archive_sha256')
if not isinstance(archive_sha, str) or not re.fullmatch(r'[0-9a-f]{64}', archive_sha):
    raise SystemExit('source manifest archive SHA-256 is invalid')
metadata = (leaf / 'task.toml').read_text(encoding='utf-8')
match = re.search(r'^repo_commit\s*=\s*"sha256:([0-9a-f]{64})"\s*$', metadata, re.M)
if match is None or match.group(1) != archive_sha:
    raise SystemExit('source manifest hash disagrees with task metadata')
if manifest.get('verification', {}).get('source_download_at_runtime') is not False:
    raise SystemExit('source manifest permits runtime source download')
count = total = 0
for path in src.rglob('*'):
    if path.is_symlink():
        raise SystemExit('source tree contains symlink: ' + str(path))
    if path.is_file():
        count += 1
        total += path.stat().st_size
if count != manifest['extracted_file_count'] or total != manifest['extracted_total_bytes']:
    raise SystemExit('source manifest count/bytes mismatch: %d/%d' % (count, total))
for rel in manifest['critical_present']:
    p = src / rel
    if not p.is_file():
        raise SystemExit('critical source missing: ' + rel)
for rel in manifest['critical_absent_expected']:
    if (src / rel).exists():
        raise SystemExit('expected source-closure boundary changed: ' + rel)
PY

# A completed reference/candidate pair is immutable evidence. On a later
# canonical invocation, validate its required manifests and reuse it rather
# than overwriting outputs or rebuilding a multi-minute oracle.
if [ "${PLUTO_REUSE_ORACLE:-0}" = 1 ]; then
  complete=1
  for row in \
    cr-gyration-01 \
    cr-gyration-04 \
    cr-relative-drift-01 \
    cr-xpoint-01; do
    if [ ! -f "$ORACLE_DIR/$row/oracle-manifest.json" ] || \
       [ ! -d "$SELF_TEST_CANDIDATE_DIR/$row" ]; then
      complete=0
    fi
  done
  for bell in 05 06; do
    if [ ! -f "$ORACLE_DIR/cr-bell-instability-05-06/subrun-$bell/oracle-manifest.json" ] || \
       [ ! -d "$SELF_TEST_CANDIDATE_DIR/cr-bell-instability-05-06/subrun-$bell" ]; then
      complete=0
    fi
  done
  if [ "$complete" = 1 ]; then
    printf '%s\n' "CPU oracle already prepared under $ORACLE_DIR; preserving existing outputs"
    printf '%s\n' "Self-test candidate already prepared under $SELF_TEST_CANDIDATE_DIR; preserving existing outputs"
    exit 0
  fi
fi

mkdir -p "$ORACLE_DIR"
SOURCE_SHA=$(python3 - "$LEAF/solution/source-manifest.json" <<'PY'
import json, sys
print(json.load(open(sys.argv[1], encoding='utf-8'))['archive_sha256'])
PY
)

read_build_flags() {
  python3 - "$1" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
lines = []
for line in path.read_text(encoding='utf-8').splitlines():
    stripped = line.strip()
    if stripped.startswith('CFLAGS') and '=' in stripped:
        name, value = stripped.split('=', 1)
        if name.strip() == 'CFLAGS':
            lines.append(value.strip())
if len(lines) != 1 or not lines[0]:
    raise SystemExit('expected exactly one non-empty CFLAGS assignment: ' + str(path))
print(lines[0])
PY
}

build_row() {
  row=$1
  family=$2
  cfg=$3
  tstop=$4
  interval=$5
  shift 5
  cells="$*"
  row_dir="$LEAF/tests/checks/$row"
  out="$ORACLE_DIR/$row"
  flags=$(read_build_flags "$row_dir/build/sciaccel.defs")
  compiler_version=$(gcc -dumpfullversion -dumpversion)
  [ -n "$compiler_version" ] || { printf '%s\n' 'gcc did not report a version' >&2; return 1; }
  if [ -e "$out/oracle-manifest.json" ]; then
    printf '%s\n' "$row: oracle manifest exists; refusing to overwrite" >&2
    return 3
  fi
  work=$(mktemp -d "$ORACLE_DIR/build.${row}.XXXXXX")
  cp -a "$LEAF/code/pluto" "$work/pluto-src"
  cp "$row_dir/build/sciaccel.defs" "$work/pluto-src/Config/sciaccel.defs"
  cp "$work/pluto-src/Test_Problems/Particles/CR/$family/init.c" "$work/init.c"
  cp "$work/pluto-src/Test_Problems/Particles/CR/$family/definitions_${cfg}.h" "$work/definitions.h"
  cp "$work/pluto-src/Test_Problems/Particles/CR/$family/pluto_${cfg}.ini" "$work/pluto.ini"
  cp "$work/pluto-src/Test_Problems/Particles/CR/$family/particles_init.c" "$work/particles_init.c"
  if [ -f "$work/pluto-src/Test_Problems/Particles/CR/$family/userdef_output.c" ]; then
    cp "$work/pluto-src/Test_Problems/Particles/CR/$family/userdef_output.c" "$work/userdef_output.c"
  fi
  # Deck edits are declared in each case manifest and are not applied to code/pluto.
  # shellcheck disable=SC2086
  python3 "$row_dir/build/deck.py" "$work/pluto.ini" "$tstop" "$interval" $cells --particles
  (cd "$work" && printf 'ARCH         = sciaccel.defs\n' > makefile && \
      PLUTO_DIR="$work/pluto-src" python3 "$work/pluto-src/setup.py" --auto-update && make -j"$JOBS")
  mkdir -p "$out"
  # The incumbent runner accepts no scientific arguments and writes declared outputs only.
  (cd "$work" && mkdir -p results && cp pluto.ini results/pluto.ini && cd results && exec "$work/pluto")
  cp -a "$work/results/." "$out/"
  # coreutils is part of the Bookworm base image. Keep this out of a pipeline:
  # set -e cannot detect a missing producer when awk is the last command.
  deck_sha=$(sha256sum "$work/pluto.ini")
  deck_sha=${deck_sha%% *}
  if [ "${#deck_sha}" -ne 64 ]; then
    printf '%s\n' "$row: sha256sum returned an invalid digest" >&2
    return 1
  fi
  case "$deck_sha" in
    *[!0123456789abcdefABCDEF]*)
      printf '%s\n' "$row: sha256sum returned a non-hex digest" >&2
      return 1
      ;;
  esac
  compiler_version=$(gcc -dumpfullversion -dumpversion)
  [ -n "$compiler_version" ] || { printf '%s\n' "$row: gcc did not report a version" >&2; return 1; }
  cat > "$out/oracle-manifest.json" <<EOF
{
  "status": "measured-local-oracle",
  "row": "$row",
  "source_archive_sha256": "$SOURCE_SHA",
  "compiler": "gcc $compiler_version",
  "flags": "$flags",
  "case_family": "$family",
  "config": "$cfg",
  "deck_sha256": "$deck_sha",
  "command": "local isolated setup.py --auto-update && make -j$JOBS && pluto",
  "outputs": "declared data/dbl/grid/particles files under this row",
  "warnings": []
}
EOF
}

# Only the six recovered obligations are presently runnable. The grouped Bell row
# is run twice, one exact official sub-deck per obligation.
build_row cr-gyration-01 Gyration 01 30.0 3.0 128 128
build_row cr-gyration-04 Gyration 04 300.0 30.0 100 100
build_row cr-relative-drift-01 Relative_Drift 01 1.0 0.1 128 128 1
build_row cr-xpoint-01 Xpoint 01 100.0 10.0 512 512
# Bell 05 and Bell 06 keep separate sub-run outputs and manifests.
for bell in 05 06; do
  row_dir="$LEAF/tests/checks/cr-bell-instability-05-06"
  out="$ORACLE_DIR/cr-bell-instability-05-06/subrun-$bell"
  flags=$(read_build_flags "$row_dir/build/sciaccel.defs")
  compiler_version=$(gcc -dumpfullversion -dumpversion)
  [ -n "$compiler_version" ] || { printf '%s\n' "$bell: gcc did not report a version" >&2; exit 1; }
  if [ -e "$out/oracle-manifest.json" ]; then
    printf '%s\n' "$bell: oracle manifest exists; refusing to overwrite" >&2
    exit 3
  fi
  work=$(mktemp -d "$ORACLE_DIR/build.bell-${bell}.XXXXXX")
  cp -a "$LEAF/code/pluto" "$work/pluto-src"
  cp "$row_dir/build/sciaccel.defs" "$work/pluto-src/Config/sciaccel.defs"
  cp "$work/pluto-src/Test_Problems/Particles/CR/Bell_Instability/init.c" \
     "$work/pluto-src/Test_Problems/Particles/CR/Bell_Instability/particles_init.c" \
     "$work/pluto-src/Test_Problems/Particles/CR/Bell_Instability/userdef_output.c" \
     "$work/"
  cp "$work/pluto-src/Test_Problems/Particles/CR/Bell_Instability/definitions_${bell}.h" "$work/definitions.h"
  cp "$work/pluto-src/Test_Problems/Particles/CR/Bell_Instability/pluto_${bell}.ini" "$work/pluto.ini"
  if [ "$bell" = 05 ]; then tstop=1.26; interval=0.126; else tstop=2.1; interval=0.21; fi
  python3 "$row_dir/build/deck.py" "$work/pluto.ini" "$tstop" "$interval" --particles
  (cd "$work" && printf 'ARCH         = sciaccel.defs\n' > makefile && PLUTO_DIR="$work/pluto-src" python3 "$work/pluto-src/setup.py" --auto-update && make -j"$JOBS")
  mkdir -p "$out"
  (cd "$work" && mkdir -p results && cp pluto.ini results/pluto.ini && cd results && exec "$work/pluto")
  cp -a "$work/results/." "$out/"
  cat > "$out/oracle-manifest.json" <<EOF
{
  "status": "measured-local-oracle",
  "row": "cr-bell-instability-$bell",
  "grouped_check": "cr-bell-instability-05-06",
  "source_archive_sha256": "$SOURCE_SHA",
  "compiler": "gcc $compiler_version",
  "flags": "$flags",
  "case_family": "Bell_Instability",
  "config": "$bell",
  "declared_window": {"tstop": $tstop, "dbl_interval": $interval},
  "warnings": []
}
EOF
done

# The documented no-argument verifier sequence needs a candidate tree distinct
# from the trusted reference. Copy only declared output directories; temporary
# build trees left under the oracle are not verifier inputs.
python3 - "$ORACLE_DIR" "$SELF_TEST_CANDIDATE_DIR" <<'PY'
import pathlib
import sys

reference = pathlib.Path(sys.argv[1]).expanduser().resolve()
candidate = pathlib.Path(sys.argv[2]).expanduser().resolve()
if not reference.is_dir():
    raise SystemExit('oracle output tree is missing')
if reference == candidate:
    raise SystemExit('self-test candidate must be physically distinct from oracle')
for child, parent in ((candidate, reference), (reference, candidate)):
    try:
        child.relative_to(parent)
    except ValueError:
        continue
    raise SystemExit('oracle and self-test candidate paths may not contain one another')
destination = pathlib.Path(sys.argv[2])
if destination.exists() or destination.is_symlink():
    raise SystemExit('self-test candidate already exists; refusing to overwrite')
PY
mkdir -p "$SELF_TEST_CANDIDATE_DIR"
for row in \
  cr-gyration-01 \
  cr-gyration-04 \
  cr-relative-drift-01 \
  cr-xpoint-01 \
  cr-bell-instability-05-06; do
  cp -a "$ORACLE_DIR/$row" "$SELF_TEST_CANDIDATE_DIR/"
done
python3 - "$ORACLE_DIR" "$SELF_TEST_CANDIDATE_DIR" <<'PY'
import pathlib
import sys

reference = pathlib.Path(sys.argv[1]).expanduser().resolve()
candidate = pathlib.Path(sys.argv[2]).expanduser().resolve()
if not candidate.is_dir() or candidate.is_symlink() or reference == candidate:
    raise SystemExit('self-test candidate tree is missing or not physically distinct')
for row in ('cr-gyration-01', 'cr-gyration-04', 'cr-relative-drift-01',
            'cr-xpoint-01', 'cr-bell-instability-05-06'):
    if not (candidate / row).is_dir():
        raise SystemExit('self-test candidate output is missing: ' + row)
PY
printf '%s\n' "CPU oracle prepared under $ORACLE_DIR"
printf '%s\n' "Exact self-test candidate prepared under $SELF_TEST_CANDIDATE_DIR"
