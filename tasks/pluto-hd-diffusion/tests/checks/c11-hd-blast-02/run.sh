#!/bin/sh
# C11: products are copied only after PLUTO exits; native output remains authoritative.
set -eu
mkdir -p /app/results
cp /app/build/pluto.ini /app/results/pluto.ini
cp /app/build/definitions.h /app/results/definitions.h
cp /app/build/deck_manifest.json /app/results/deck_manifest.json
cp /app/build/makefile /app/results/makefile
# C11's official InitDomain() consumes these external-input products.  They
# are generated during image preprocessing and copied before the solver starts,
# so this isolated Docker row has the same explicit input contract as the
# upstream two-stage campaign without relying on a host path.
cp /app/build/grid0.out /app/results/grid0.out
cp /app/build/rho0.dbl /app/results/rho0.dbl
cd /app/results
set +e
/app/build/pluto > solver.stdout 2> solver.stderr
status=$?
set -e
python3 /app/collect_observations.py /app/results /app/build /app/expected_observations.json || status=1
printf 'solver_exit_status=%s\n' "$status" > /app/results/solver_completion.txt
exit "$status"
