#!/usr/bin/env bash
# Build isolated official Phantom setup binaries without changing shared source.
set -euo pipefail
[ "$#" -eq 2 ] || { echo "usage: build.sh SOURCE OUT" >&2; exit 2; }
SOURCE="$1"
OUT="$2"
[ -f "$SOURCE/build/Makefile_setups" ] || { echo "build.sh: incomplete Phantom source" >&2; exit 2; }
mkdir -p "$OUT" /opt/phantom-builds
SETUPS=(sedov kh taylorgreen wave)
for setup in "${SETUPS[@]}"; do
  build="/opt/phantom-builds/$setup"
  dest="$OUT/$setup"
  [ ! -e "$build" ] && [ ! -e "$dest" ] || { echo "build.sh: refusing existing build/destination for $setup" >&2; exit 2; }
  mkdir -p "$build" "$dest"
  cp -a "$SOURCE/." "$build/"
  FFLAGS="-ffp-contract=off" make -C "$build" SETUP="$setup" SYSTEM=gfortran OPENMP=yes DOUBLEPRECISION=yes phantomsetup
  FFLAGS="-ffp-contract=off" make -C "$build" SETUP="$setup" SYSTEM=gfortran OPENMP=yes DOUBLEPRECISION=yes phantom
  [ -x "$build/bin/phantomsetup" ] && [ -x "$build/bin/phantom" ] || { echo "build.sh: missing binaries for $setup" >&2; exit 1; }
  cp "$build/bin/phantomsetup" "$dest/phantomsetup"
  cp "$build/bin/phantom" "$dest/phantom"
done

build=/opt/phantom-builds/test2
dest="$OUT/test2"
[ ! -e "$build" ] && [ ! -e "$dest" ] || { echo "build.sh: refusing existing test2 paths" >&2; exit 2; }
mkdir -p "$build" "$dest"
cp -a "$SOURCE/." "$build/"
FFLAGS="-ffp-contract=off" make -C "$build" SETUP=test2 SYSTEM=gfortran OPENMP=yes phantomtest
[ -x "$build/bin/phantomtest" ] || { echo "build.sh: missing phantomtest" >&2; exit 1; }
cp "$build/bin/phantomtest" "$dest/phantomtest"
printf 'built %d official production setups and one official test executable\n' "${#SETUPS[@]}"
