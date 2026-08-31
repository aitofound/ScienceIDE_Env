#!/usr/bin/env bash
set -u -o pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 -B "$ROOT/harness.py"
