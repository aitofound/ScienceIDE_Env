#!/bin/sh
set -eu
new_scratch() {
  base=${HARBOR_WRITABLE_DIR:-${TMPDIR:-/tmp}}
  mkdir -p "$base"
  mktemp -d "$base/pluto-hd-check.XXXXXX"
}
require_dir() {
  [ -d "$1" ] || { echo "missing directory: $1" >&2; return 1; }
}
