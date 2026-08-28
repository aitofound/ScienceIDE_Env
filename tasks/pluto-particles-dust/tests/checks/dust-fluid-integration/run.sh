#!/bin/sh
set -eu
[ "$#" -eq 0 ] || exit 2
printf '%s\n' '{"check":"dust-fluid-integration","status":"blocked","passed":false,"outcome":"owner_inputs_missing"}'
exit 78
