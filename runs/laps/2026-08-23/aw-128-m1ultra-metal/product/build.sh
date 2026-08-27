#!/usr/bin/env bash
# Build the LAPS port for cell aw-128-m1ultra-metal on the m1ultra-metal target.
#
#     bash product/build.sh
#
# No arguments, no network, nothing installed.  Everything it needs is in
# manifest.json and is part of Command Line Tools plus two system frameworks.
#
# The Metal shaders are NOT compiled here.  The target has Command Line Tools
# and no Xcode, so `xcrun metal` does not exist and there is no offline
# .metal -> .air -> .metallib path.  The shader source travels inside the
# executable as a string and Metal.framework compiles it at run time via
# newLibraryWithSource:, which needs no Xcode.  A shader error therefore shows
# up on the first run, not here - run.sh surfaces it as a non-zero exit.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$DIR/bin"

# -fno-fast-math is belt to the braces in backend_metal.h: the double-single
# arithmetic that stands in for the fp64 Metal does not have is built from
# error-free transformations, and fast math is entitled to fold them away.  The
# HOST side of that arithmetic is only the double-to-pair split, but the flag
# costs nothing and says what the code needs.
clang++ -O2 -std=c++17 -fobjc-arc -fno-fast-math \
    -x objective-c++ \
    -DLAPS_VARIANT='"3d"' \
    -I "$DIR/src" \
    "$DIR/src/main_metal.mm" \
    -o "$DIR/bin/laps" \
    -framework Foundation -framework Metal

test -x "$DIR/bin/laps"
echo "built $DIR/bin/laps"
