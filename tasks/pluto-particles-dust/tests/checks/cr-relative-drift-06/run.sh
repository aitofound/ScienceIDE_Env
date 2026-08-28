#!/bin/sh
set -eu
[ "$#" -eq 0 ] || { echo 'this runner accepts no arguments' >&2; exit 2; }
printf '%s\n' '{"check":"cr-relative-drift-06","status":"blocked","passed":false,"outcome":"stage_row_requires_oracle"}'
exit 78
