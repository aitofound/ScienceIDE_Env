#!/usr/bin/env bash
# Build one isolated Phantom test-suite executable for every unique active SETUP.
set -euo pipefail
PHANTOM_DIR="${PHANTOM_DIR:-/opt/phantom}"
BUILD_JOBS="${PHANTOM_BUILD_JOBS:-1}"
case "$BUILD_JOBS" in ''|*[!0-9]*|0) BUILD_JOBS=1 ;; esac
setups=(raddisc radstar radiativebox testkd test)
mkdir -p /opt/phantom-tests
cd "$PHANTOM_DIR"
for setup in "${setups[@]}"; do
  make SYSTEM=gfortran SETUP="$setup" clean
  FFLAGS='-ffp-contract=off' \
    make -j"$BUILD_JOBS" SYSTEM=gfortran SETUP="$setup" OPENMP=yes phantomtest
  test -x "$PHANTOM_DIR/bin/phantomtest"
  cp "$PHANTOM_DIR/bin/phantomtest" "/opt/phantom-tests/$setup"
done
printf 'built %d setup-specific Phantom suites\n' "${#setups[@]}"
