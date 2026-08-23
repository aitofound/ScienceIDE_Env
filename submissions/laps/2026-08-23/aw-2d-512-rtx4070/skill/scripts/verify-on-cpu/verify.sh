#!/usr/bin/env bash
# Build and run the CUDA product's own kernels on the CPU, and grade the
# result against the incumbent.
#
#     bash verify.sh <package-dir> [output-dir]
#
# WHAT THIS IS FOR.  This cell's product is CUDA and it was written on a
# machine with no NVIDIA device.  cuda_shim.hpp supplies the slice of the CUDA
# and cuFFT API that ../../../product/src/backend_cuda.cuh uses - cudaMalloc,
# the launch geometry, cufftExecD2Z - over FFTW, so that the SAME backend file,
# the SAME kernel bodies and the SAME host orchestration can be run and
# compared against the reference the check image produces.
#
# WHAT IT ESTABLISHES.  Every index calculation, every launch geometry, the
# flux batching, the scatter table, the dealiasing, the RK update, the CFL
# reduction and the output writer.
#
# WHAT IT DOES NOT.  Real device scheduling, and cuFFT's arithmetic as against
# FFTW's.  Both are stated in ../../references/diary.md rather than glossed.
#
# Needs: a C++17 compiler, FFTW3 (headers and library), python3 with numpy,
# and docker to produce the reference.
set -euo pipefail
PKG="${1:?usage: verify.sh <package-dir> [output-dir]}"
PKG="$(cd "$PKG" && pwd)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROD="$(cd "$HERE/../../../product" && pwd)"
OUT="${2:-$(mktemp -d)}"
mkdir -p "$OUT"

CXXFLAGS="${CXXFLAGS:-}"
LDFLAGS="${LDFLAGS:-}"
for pre in /opt/homebrew /usr/local /usr; do
  if [ -f "$pre/include/fftw3.h" ]; then
    CXXFLAGS="$CXXFLAGS -I$pre/include"; LDFLAGS="$LDFLAGS -L$pre/lib"; break
  fi
done

PROD_DECK="$PROD/config/mhd.input"
# If the deck inside the product has been patched, the CHECK directory has to
# be patched to match: the reference below is produced from checks/aw-2d-512's
# own deck, and grading a 192-cubed candidate against a 256-cubed reference
# reports "wrong_shape", which sends you looking for the wrong bug.  Measured,
# not imagined - this is what happened the first time.
if ! cmp -s "$PROD_DECK" "$PKG/checks/aw-2d-512/config/mhd.input"; then
  echo "NOTE: the product's deck and checks/aw-2d-512/config/mhd.input differ." >&2
  echo "      The reference is produced from the CHECK's deck. Patch both, or" >&2
  echo "      point this script at a package whose check carries the same deck." >&2
fi

echo "== building the CUDA backend against the CPU stand-in"
c++ -O2 -std=c++17 -I "$PROD/src" -I "$HERE" $CXXFLAGS \
    -DLAPS_VARIANT='"2d"' \
    "$HERE/main_cuda_emu.cpp" -o "$OUT/laps-cuda-emu" $LDFLAGS -lfftw3 -lm

echo "== running it on this cell's deck"
LAPS_CONFIG="$PROD/config/mhd.input" LAPS_OUTDIR="$OUT/CAND" "$OUT/laps-cuda-emu"

echo "== building and running the check image - this is the reference"
docker build -q -t "sciaccel/laps-aw-2d-512" "$PKG/checks/aw-2d-512" >/dev/null
docker rm -f "laps-emu-ref-$$" >/dev/null 2>&1 || true
docker run --name "laps-emu-ref-$$" --network=none "sciaccel/laps-aw-2d-512" >/dev/null
docker cp "laps-emu-ref-$$:/app/results" "$OUT/REF" >/dev/null
docker rm -f "laps-emu-ref-$$" >/dev/null

echo "== the verdict is the check's own validate.py"
( cd "$PKG/checks/aw-2d-512" && python3 validate.py "$OUT/REF" "$OUT/CAND" )
