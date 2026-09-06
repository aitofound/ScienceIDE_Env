#!/usr/bin/env bash
# Build the product image for cell aw-256-rtx4070.
#
#     bash skill/scripts/build.sh [image-tag]
#
# Network is on during the build (the base image is pulled) and off at run
# time.  Nothing is fetched at run time by design.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TAG="${1:-sciaccel/laps-aw-256-rtx4070-submission}"
docker build --tag "$TAG" "$DIR/product"
echo "built $TAG"
