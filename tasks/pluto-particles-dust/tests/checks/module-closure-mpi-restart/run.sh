#!/bin/sh
set -eu
[ "$#" -eq 0 ] || exit 2
printf '%s\n' '{"check":"module-closure-mpi-restart","status":"blocked","passed":false,"outcome":"mpi_restart_not_measured"}'
exit 78
