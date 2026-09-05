#!/usr/bin/env bash
# Run one EFTCAMB physics-family check from its self-contained public inputs.
# SOURCE_DIR is copied before the serial Fortran build; the source is never changed.

KNOB_HELP=""
knob() {
  local name=$1 default=$2 desc=$3
  [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"
  export "$name"
  KNOB_HELP+="$name=$default  $desc"$'\n'
}
knob SAB_MODELS "all models in models.txt" "space-separated model basenames; use a subset for iteration"
knob SAB_LMAX 1200 "graded angular-output window (ell <= SAB_LMAX); solver uses upstream l_max_scalar=3500"
knob SAB_KMAX 0.2 "graded k-output window (k/h <= SAB_KMAX); solver uses upstream transfer_kmax=2"
knob SAB_MAKE_JOBS 1 "build jobs; must remain 1 because EFTCAMB Fortran module builds race in parallel"
if [ "${1:-}" = "--help" ]; then
  printf '%s' "$KNOB_HELP"
  exit 0
fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
[ "$SAB_MAKE_JOBS" = 1 ] || { echo "run.sh: SAB_MAKE_JOBS must be 1; parallel make races on Fortran modules" >&2; exit 2; }
[[ "$SAB_LMAX" =~ ^[0-9]+$ ]] || { echo "run.sh: SAB_LMAX must be a positive integer" >&2; exit 2; }
[ "$SAB_LMAX" -gt 0 ] || { echo "run.sh: SAB_LMAX must be positive" >&2; exit 2; }
[[ "$SAB_KMAX" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "run.sh: SAB_KMAX must be a positive number" >&2; exit 2; }

if [ "$SAB_MODELS" = "all models in models.txt" ]; then
  mapfile -t MODELS < <(sed '/^[[:space:]]*$/d' "$CHECK_DIR/models.txt")
else
  read -r -a MODELS <<< "$SAB_MODELS"
fi
[ "${#MODELS[@]}" -gt 0 ] || { echo "run.sh: no models selected" >&2; exit 2; }
for model in "${MODELS[@]}"; do
  [[ "$model" =~ ^[A-Za-z0-9_]+$ ]] || { echo "run.sh: invalid model basename: $model" >&2; exit 2; }
  [ -f "$CHECK_DIR/ic/$IC/$model.ini" ] || { echo "run.sh: $model is not an input of this check" >&2; exit 2; }
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
FORTRAN="$WORK/src/fortran"
PARAMS="$FORTRAN/eftcamb_test/parameters"
[ -f "$FORTRAN/Makefile" ] || { echo "run.sh: source has no fortran/Makefile" >&2; exit 2; }
cp "$CHECK_DIR/ic/$IC/"*.ini "$PARAMS/"

mkdir -p "$FORTRAN/eftcamb_test/results/spectra_results"
make -C "$FORTRAN" clean
build_started=$SECONDS
make -C "$FORTRAN" -j "$SAB_MAKE_JOBS" camb CLUSTER_SAFE=1
printf 'SAB_BUILD_SECONDS=%s\n' "$((SECONDS - build_started))"
[ -x "$FORTRAN/camb" ] || { echo "run.sh: build left no fortran/camb" >&2; exit 1; }

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-8}"
for model in "${MODELS[@]}"; do
  (cd "$FORTRAN" && ./camb "eftcamb_test/parameters/$model.ini")
done

python3 - "$PARAMS" "$FORTRAN" "$OUT_DIR" "$SAB_LMAX" "$SAB_KMAX" "${MODELS[@]}" <<'PY'
import math
import re
import shutil
import sys
from pathlib import Path

parameter_dir, fortran_dir, output_dir = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
lmax = int(sys.argv[4])
kmax = float(sys.argv[5])
models = sys.argv[6:]
suffixes = (
    "scalCls.dat",
    "lensedCls.dat",
    "lensedtotCls.dat",
    "lenspotentialCls.dat",
    "totCls.dat",
    "tensCls.dat",
    "scalarCovCls.dat",
    "matterpower.dat",
    "transfer_out.dat",
)
angular_suffixes = {
    "scalCls.dat",
    "lensedCls.dat",
    "lensedtotCls.dat",
    "lenspotentialCls.dat",
    "totCls.dat",
    "tensCls.dat",
    "scalarCovCls.dat",
}
k_suffixes = {"matterpower.dat", "transfer_out.dat"}
output_dir.mkdir(parents=True, exist_ok=True)
file_count = 0
value_count = 0
for model in models:
    deck = (parameter_dir / f"{model}.ini").read_text(encoding="utf-8")
    match = re.search(r"^\s*output_root\s*=\s*(\S+)\s*$", deck, flags=re.M)
    if not match:
        raise SystemExit(f"run.sh: {model}.ini has no output_root")
    root = Path(match.group(1))
    prefix = root if root.is_absolute() else fortran_dir / root
    for suffix in suffixes:
        path = Path(f"{prefix}_{suffix}")
        if not path.is_file():
            raise SystemExit(f"run.sh: expected output missing: {path}")
        selected_lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        if suffix in angular_suffixes or suffix in k_suffixes:
            filtered = []
            for line_number, line in enumerate(selected_lines, 1):
                data = line.partition("#")[0].strip()
                if not data:
                    filtered.append(line)
                    continue
                try:
                    coordinate = float(data.split()[0].replace("D", "E").replace("d", "e"))
                except ValueError as exc:
                    raise SystemExit(f"run.sh: non-numeric coordinate in {path}:{line_number}") from exc
                limit = lmax if suffix in angular_suffixes else kmax
                if coordinate <= limit:
                    filtered.append(line)
            selected_lines = filtered
        values = []
        for line_number, line in enumerate(selected_lines, 1):
            data = line.partition("#")[0].strip()
            if not data:
                continue
            for token in data.split():
                try:
                    value = float(token.replace("D", "E").replace("d", "e"))
                except ValueError as exc:
                    raise SystemExit(f"run.sh: non-numeric token in {path}:{line_number}: {token}") from exc
                if not math.isfinite(value):
                    raise SystemExit(f"run.sh: non-finite value in {path}:{line_number}")
                values.append(value)
        if not values:
            raise SystemExit(f"run.sh: expected output empty: {path}")
        destination = output_dir / f"{model}_{suffix}"
        if destination.exists():
            raise SystemExit(f"run.sh: duplicate output destination: {destination}")
        if suffix in angular_suffixes or suffix in k_suffixes:
            destination.write_text("".join(selected_lines), encoding="utf-8")
        else:
            shutil.copyfile(path, destination)
        file_count += 1
        value_count += len(values)
print(f"SAB_OBSERVABLE_FILES={file_count}")
print(f"SAB_OBSERVABLE_VALUES={value_count}")
PY
