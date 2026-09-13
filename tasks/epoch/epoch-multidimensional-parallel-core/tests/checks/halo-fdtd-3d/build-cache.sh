#!/usr/bin/env bash
# Shared mechanical build helper for the epoch checks.
# A produce invocation supplies SAB_BUILD_CACHE as a per-run sibling of its
# output root. Direct run.sh calls leave it unset and therefore remain
# independently cold-buildable in their private WORK directory.

sab_source_fingerprint() {
  python3 - "$1" <<'PY'
import hashlib
import os
import sys

root = os.path.realpath(sys.argv[1])
if not os.path.isdir(root):
    raise SystemExit("sab_source_fingerprint: source directory is not a directory: " + root)
hash_ = hashlib.sha256()
for directory, dirnames, filenames in os.walk(root, followlinks=False):
    dirnames.sort()
    filenames.sort()
    for name in filenames:
        path = os.path.join(directory, name)
        relative = os.path.relpath(path, root).replace(os.sep, "/")
        hash_.update(relative.encode("utf-8", "surrogateescape"))
        hash_.update(b"\0")
        if os.path.islink(path):
            hash_.update(b"SYMLINK\0")
            hash_.update(os.readlink(path).encode("utf-8", "surrogateescape"))
            hash_.update(b"\0")
            continue
        with open(path, "rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                hash_.update(chunk)
        hash_.update(b"\0")
print(hash_.hexdigest())
PY
}

sab_build_key() {
  local source_fingerprint=$1 dimension=$2 compiler=$3 precision=$4 altbuild=$5 build_config=$6
  printf 'source=%s\ndimension=%s\ncompiler=%s\nprecision=%s\nbuild_config=%s\naltbuild=%s\n' \
    "$source_fingerprint" "$dimension" "$compiler" "$precision" "$build_config" "$altbuild" \
    | python3 -c 'import hashlib, sys; print(hashlib.sha256(sys.stdin.buffer.read()).hexdigest())'
}

sab_apply_altbuild() {
  local makefile=$1 before after
  before=$(grep -c '^  FFLAGS = -O3 -g -std=f2003$' "$makefile" || true)
  sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$makefile"
  after=$(grep -c '^  FFLAGS = -O0 -g -std=f2003$' "$makefile" || true)
  [ "$before" = 1 ] && [ "$after" = 1 ] || {
    echo "build-cache.sh: expected exactly one gfortran FFLAGS line to change (-O3 -> -O0), before=$before after=$after" >&2
    return 1
  }
}

sab_prepare_build() {
  local source_dir=$1 dimension=$2 compiler=$3 precision=$4 altbuild=$5 make_jobs=$6 work_dir=$7
  local source_fingerprint="${SAB_SOURCE_FINGERPRINT:-}"
  local build_config="${SAB_BUILD_CONFIG:-epoch-make-default}"
  local cache_root="${SAB_BUILD_CACHE:-}"
  local key marker candidate build_dir build_source start end elapsed
  [ -n "$source_fingerprint" ] || source_fingerprint=$(sab_source_fingerprint "$source_dir")
  key=$(sab_build_key "$source_fingerprint" "$dimension" "$compiler" "$precision" "$altbuild" "$build_config")

  SAB_BUILD_REUSED=0
  SAB_BUILD_SECONDS=0
  if [ -n "$cache_root" ]; then
    mkdir -p "$cache_root"
    cache_root=$(cd "$cache_root" && pwd -P)
    for marker in "$cache_root/${key}.ready."*; do
      [ -f "$marker" ] || continue
      candidate=$(cat "$marker")
      if [ -n "$candidate" ] && [ -x "$candidate/$dimension/bin/$dimension" ]; then
        SAB_BUILD_SOURCE=$candidate
        SAB_BUILD_REUSED=1
        export SAB_BUILD_SOURCE SAB_BUILD_REUSED SAB_BUILD_SECONDS
        return 0
      fi
    done
    while :; do
      build_dir="$cache_root/${key}.build.$$.$RANDOM"
      if mkdir "$build_dir" 2>/dev/null; then break; fi
    done
    build_source="$build_dir/src"
    mkdir "$build_source"
  else
    build_dir="$work_dir"
    build_source="$build_dir/src"
    mkdir "$build_source"
  fi

  cp -R "$source_dir/." "$build_source"
  if [ "$altbuild" = 1 ]; then
    sab_apply_altbuild "$build_source/$dimension/Makefile"
  fi
  start=$(date +%s)
  make -C "$build_source/$dimension" COMPILER="$compiler" -j"$make_jobs" > "$build_dir/make.log" 2>&1
  end=$(date +%s)
  elapsed=$((end - start))
  [ "$elapsed" -gt 0 ] || elapsed=1
  SAB_BUILD_SOURCE=$build_source
  SAB_BUILD_SECONDS=$elapsed
  if [ -n "$cache_root" ]; then
    marker="$cache_root/${key}.ready.$$.$RANDOM"
    printf '%s\n' "$build_source" > "$marker"
  fi
  export SAB_BUILD_SOURCE SAB_BUILD_REUSED SAB_BUILD_SECONDS
}
