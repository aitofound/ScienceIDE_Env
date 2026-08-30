#!/usr/bin/env bash
# sciaccel-canary GUID epoch-mdpc-1a7c9e42
set -u -o pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$ROOT/harness.py"
