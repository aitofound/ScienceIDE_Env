#!/usr/bin/env bash
# Per-produce SWMF build-family cache.  The produce driver gives each solve a
# fresh, private SAB_SHARED_BUILD_ROOT and invokes checks serially.  Configured
# trees are built at their final paths and are never copied or relocated.

sab_tree_fingerprint() {
  python3 - "$1" <<'PY'
from pathlib import Path
import hashlib, os, sys
root = Path(sys.argv[1])
if not root.is_dir():
    raise SystemExit(f"sab_tree_fingerprint: not a directory: {root}")
h = hashlib.sha256()
for path in sorted(root.rglob("*"), key=lambda p: p.relative_to(root).as_posix()):
    rel = path.relative_to(root).as_posix().encode()
    if path.is_symlink():
        kind, body = b"L", os.readlink(path).encode()
    elif path.is_dir():
        kind, body = b"D", b""
    elif path.is_file():
        kind, body = b"F", path.read_bytes()
    else:
        kind, body = b"O", b""
    h.update(kind + b"\0" + rel + b"\0" + str(len(body)).encode() + b"\0" + body + b"\0")
print(h.hexdigest())
PY
}

sab_prepare_build() {
  : "${SOURCE_DIR:?}" "${WORK:?}" "${IC:?}" "${BUILD_FAMILY:?}" "${BUILD_SPEC:?}" "${BUILD_INPUT_KEY:?}"
  case "$BUILD_FAMILY:$BUILD_INPUT_KEY:$IC" in
    *[!A-Za-z0-9._:-]*) echo "run.sh: unsafe build-cache key" >&2; exit 2 ;;
  esac
  if [ "$IC" = altbuild ]; then BUILD_MODE=o0; else BUILD_MODE=release; fi
  BUILD_CACHE_KEY="${BUILD_FAMILY}--${IC}--${BUILD_MODE}--${BUILD_INPUT_KEY}"
  SAB_BUILD_CACHE_HIT=0
  SAB_BUILD_CACHE_ENABLED=0
  if [ -n "${SAB_SHARED_BUILD_ROOT:-}" ]; then
    case "$SAB_SHARED_BUILD_ROOT" in /*) ;; *) echo "run.sh: SAB_SHARED_BUILD_ROOT must be absolute" >&2; exit 2 ;; esac
    [ -d "$SAB_SHARED_BUILD_ROOT" ] || { echo "run.sh: private build root is not a directory: $SAB_SHARED_BUILD_ROOT" >&2; exit 2; }
    SAB_BUILD_CACHE_ENABLED=1
    SRC="$SAB_SHARED_BUILD_ROOT/$BUILD_CACHE_KEY"
    SAB_BUILD_MARKER="$SRC/.sab-build-complete"
    SAB_EXPECTED_MARKER="$(printf '%s\n' \
      'format=sab-swmf-build-cache-v1' \
      "family=$BUILD_FAMILY" \
      "solve_ic=$IC" \
      "mode=$BUILD_MODE" \
      "input_key=$BUILD_INPUT_KEY" \
      "source_dir=$SOURCE_DIR" \
      "spec=$BUILD_SPEC")"
    if [ -e "$SRC" ] || [ -L "$SRC" ]; then
      [ -d "$SRC" ] && [ ! -L "$SRC" ] || { echo "run.sh: unsafe build-cache entry: $SRC" >&2; exit 1; }
      [ -f "$SAB_BUILD_MARKER" ] || { echo "run.sh: refusing incomplete build-cache entry: $SRC" >&2; exit 1; }
      [ "$(cat "$SAB_BUILD_MARKER")" = "$SAB_EXPECTED_MARKER" ] || { echo "run.sh: refusing mismatched build-cache entry: $SRC" >&2; exit 1; }
      SAB_BUILD_CACHE_HIT=1
    else
      mkdir "$SRC"
      cp -R "$SOURCE_DIR/." "$SRC/"
    fi
  else
    SRC="$WORK/src"
    mkdir "$SRC"
    cp -R "$SOURCE_DIR/." "$SRC/"
    SAB_BUILD_MARKER=""
    SAB_EXPECTED_MARKER=""
  fi
  export SRC BUILD_MODE BUILD_CACHE_KEY SAB_BUILD_CACHE_HIT SAB_BUILD_CACHE_ENABLED
}

sab_report_build_reuse() {
  [ "$SAB_BUILD_CACHE_HIT" -eq 1 ] || { echo "run.sh: internal cache-hit error" >&2; exit 1; }
  echo "SAB_BUILD_CACHE_STATUS=hit"
  echo "SAB_BUILD_CACHE_KEY=$BUILD_CACHE_KEY"
  echo "SAB_BUILD_SECONDS=0"
}

sab_finish_build() {
  local seconds="${1:?}"
  case "$seconds" in ''|*[!0-9]*) echo "run.sh: invalid build duration: $seconds" >&2; exit 1 ;; esac
  [ "$SAB_BUILD_CACHE_HIT" -eq 0 ] || { echo "run.sh: cannot finish an existing cache entry" >&2; exit 1; }
  if [ "$SAB_BUILD_CACHE_ENABLED" -eq 1 ]; then
    printf '%s\n' "$SAB_EXPECTED_MARKER" > "$SAB_BUILD_MARKER"
    echo "SAB_BUILD_CACHE_STATUS=miss"
  else
    echo "SAB_BUILD_CACHE_STATUS=disabled"
  fi
  echo "SAB_BUILD_CACHE_KEY=$BUILD_CACHE_KEY"
  echo "SAB_BUILD_SECONDS=$seconds"
}
