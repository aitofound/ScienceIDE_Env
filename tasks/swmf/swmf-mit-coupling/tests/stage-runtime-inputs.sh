#!/usr/bin/env bash
# Stage only pinned, pre-existing runtime sidecars required by the upstream recipes.
set -euo pipefail

[ "$#" -eq 3 ] || {
  echo 'usage: stage-runtime-inputs.sh <swpc|rbe> <source_root> <destination_root>' >&2
  exit 2
}
mode="$1"; source_root="$2"; destination_root="$3"
[ -d "$source_root" ] || { echo "stage-runtime-inputs: source root is not a directory: $source_root" >&2; exit 2; }
[ -d "$destination_root" ] || { echo "stage-runtime-inputs: destination root is not a directory: $destination_root" >&2; exit 2; }

copy_exact() {
  src="$1"; dst="$2"
  [ -f "$src" ] || { echo "stage-runtime-inputs: required pinned file is missing: $src" >&2; exit 2; }
  [ ! -L "$dst" ] || { echo "stage-runtime-inputs: refusing symlink destination: $dst" >&2; exit 2; }
  if [ -e "$dst" ]; then
    cmp -s "$src" "$dst" || { echo "stage-runtime-inputs: destination already differs: $dst" >&2; exit 2; }
  else
    cp "$src" "$dst"
  fi
}

case "$mode" in
  swpc)
    # This is the pinned Makefile.test contract: the selected PARAM plus *.in
    # and *.dat. PARAM.in itself is copied by run.sh after these sidecars.
    sidecars="$source_root/Param/SWPC"
    for name in IMF.dat INTERPOLATE.in SATELLITES.in magin_GEM.dat sat01.dat; do
      copy_exact "$sidecars/$name" "$destination_root/$name"
    done
    ;;
  rbe)
    # RB/RBE/Makefile's custom-rundir recipe resolves ../data/input from the
    # requested RUNDIR, so reproduce that layout using the verified SWMF_data.
    data="$source_root/SWMF_data/RB/RBE/data/input"
    mkdir -p "$destination_root/data/input"
    copy_exact "$data/2002_296.SWIMF" "$destination_root/data/input/2002_296.SWIMF"
    copy_exact "$data/2002_296.symHKp" "$destination_root/data/input/2002_296.symHKp"
    ;;
  *)
    echo "stage-runtime-inputs: unsupported mode: $mode" >&2
    exit 2
    ;;
esac
