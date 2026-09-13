#!/usr/bin/env bash
set -euo pipefail

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MAX_SPACE "12" "Davidson subspace cap used to obtain the CI vector before RDM construction"
ALTBUILD=""
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
case "$IC" in nominal|variant) ;; *) echo "invalid initial condition: $IC" >&2; exit 2 ;; esac
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUT="$CHECK_DIR/ic/$IC/input.json"
[ -f "$INPUT" ] || { echo "missing $INPUT" >&2; exit 2; }
echo "SAB_BUILD_SECONDS=0"
PYTHONPATH="$SOURCE_DIR${PYTHONPATH:+:$PYTHONPATH}" python3 -m pytest "$SOURCE_DIR/pyscf/fci/test/test_rdm.py" -q --disable-warnings --maxfail=1
PYTHONPATH="$SOURCE_DIR${PYTHONPATH:+:$PYTHONPATH}" python3 - "$INPUT" "$OUT_DIR/observable.npy" "$SAB_MAX_SPACE" <<'PY'
import json, sys
import numpy as np
from pyscf.fci import direct_spin1, rdm

inp = json.load(open(sys.argv[1], encoding="utf-8"))
h1 = np.asarray(inp["h1e"], dtype=np.float64)
eri = np.asarray(inp["eri"], dtype=np.float64)
if inp["variant"]:
    h1[0, 0] = np.nextafter(np.nextafter(h1[0, 0], -np.inf), -np.inf)
norb, nelec = int(inp["norb"]), tuple(int(x) for x in inp["nelec"])
e, ci = direct_spin1.kernel(h1, eri, norb, nelec, max_space=int(sys.argv[3]))
dm1, dm2 = rdm.make_rdm12_spin1('FCIrdm12kern_sf', ci, ci, norb, nelec)
obs = np.array([float(e), float(np.trace(dm1)), float(np.linalg.norm(dm1)), float(dm1[0, 0]), float(dm1[0, 1]), float(np.linalg.norm(dm2)), float(dm2[0, 0, 0, 0])], dtype=np.float64)
np.save(sys.argv[2], obs)
PY
