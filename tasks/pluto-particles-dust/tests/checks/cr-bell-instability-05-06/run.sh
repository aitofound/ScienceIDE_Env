#!/bin/sh
# Grouped Bell runner: both exact official sub-runs are required; no arguments.
set -eu
[ "$#" -eq 0 ] || exit 2
mkdir -p /app/results/subrun-05 /app/results/subrun-06
for cfg in 05 06; do
 cp "/app/build/pluto_${cfg}.ini" "/app/results/subrun-${cfg}/pluto.ini"
 (cd "/app/results/subrun-${cfg}" && exec "/app/build/pluto-${cfg}")
done
