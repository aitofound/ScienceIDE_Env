#!/usr/bin/env bash
# Reproduce one official Gkeyll PKPM regression (1D and 2D configuration-space drivers): run.sh nominal | variant (two-binary64-ULP input perturbation) | altbuild (nominal inputs, strict-IEEE build).
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q+p-1)/p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEPS upstream "number of update steps; the graded window runs to its physical end time"
knob SAB_XCELLS upstream "override the upstream configuration-space resolution (first direction)"
knob SAB_YCELLS upstream "override the upstream second configuration-space resolution when present"
knob SAB_VX_CELLS upstream "override the upstream parallel-velocity resolution"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs used only for excluded source-build time"
# Alternative build (run.sh altbuild): the nominal inputs on the same pinned source compiled strict-IEEE with the
# same gcc, -O2 without -ffast-math, -ffp-contract=off and no -march=native, into build-ieee/ (the oracle image
# carries that library tree next to the default -O3 -ffast-math -march=native build/). A correct port compiled
# without fast-math or FMA contraction is exactly such a build; selfcheck grades it against nominal as the floor.
ALTBUILD="same source, gcc -O2 strict IEEE: no -ffast-math, -ffp-contract=off, no -march=native (build-ieee/)"
if [ "${1:-}" = --help ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi
set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; BUILD_DIR=build; MAKE_OVERRIDES=()
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal; BUILD_DIR=build-ieee
  alt_arch=""; case "$(uname -m)" in aarch64|arm64) alt_arch="-D__arm64__" ;; esac
  export CFLAGS='-O2 -g -fPIC -MMD -MP -ffp-contract=off -DGIT_COMMIT_ID=\"sab-altbuild\" -DGKYL_BUILD_DATE=\"sab-altbuild\" -DGKYL_GIT_CHANGESET=\"sab-altbuild\"'
  MAKE_OVERRIDES=("BUILD_DIR=$BUILD_DIR" "ARCH_FLAGS=$alt_arch")
fi
# Each check patches the first declaration of one driver parameter (param) whose right-hand side is a numeric
# literal, with ic/<inputs>/value.txt (a later "double x = app->x;" local copy is never matched).
# Each check patches the first declaration of one driver parameter (param) whose right-hand side is a numeric
# literal, with ic/<inputs>/value.txt (a later "double x = app->x;" local copy is never matched).
case "$(basename "$CHECK_DIR")" in
  pkpm-landau-damping)
    stem=rt_pkpm_landau_damping_p1; param=alpha
    files=('elc-imom.gkyl:rt_pkpm_landau_damping_p1-elc-imom.gkyl' 'elc-L2.gkyl:rt_pkpm_landau_damping_p1-elc-L2.gkyl' 'field-energy.gkyl:rt_pkpm_landau_damping_p1-field-energy.gkyl') ;;
  pkpm-em-advection)
    stem=rt_pkpm_em_advect_p1; param=omega
    files=('elc-imom.gkyl:rt_pkpm_em_advect_p1-elc-imom.gkyl' 'elc-L2.gkyl:rt_pkpm_em_advect_p1-elc-L2.gkyl' 'field-energy.gkyl:rt_pkpm_em_advect_p1-field-energy.gkyl') ;;
  pkpm-neutral-sodshock)
    stem=rt_pkpm_neut_sodshock_p1; param=nu
    files=('neut-imom.gkyl:rt_pkpm_neut_sodshock_p1-neut-imom.gkyl' 'neut-L2.gkyl:rt_pkpm_neut_sodshock_p1-neut-L2.gkyl' 'field-energy.gkyl:rt_pkpm_neut_sodshock_p1-field-energy.gkyl') ;;
  pkpm-traveling-pulse)
    stem=rt_pkpm_travel_pulse_p1; param=alpha
    files=('neut-imom.gkyl:rt_pkpm_travel_pulse_p1-neut-imom.gkyl' 'neut-L2.gkyl:rt_pkpm_travel_pulse_p1-neut-L2.gkyl' 'field-energy.gkyl:rt_pkpm_travel_pulse_p1-field-energy.gkyl') ;;
  pkpm-wall-p2)
    stem=rt_pkpm_wall_p2; param=u0
    files=('neut-imom.gkyl:rt_pkpm_wall_p2-neut-imom.gkyl' 'neut-L2.gkyl:rt_pkpm_wall_p2-neut-L2.gkyl' 'field-energy.gkyl:rt_pkpm_wall_p2-field-energy.gkyl') ;;
  pkpm-reflecting-electrostatic-shock-p2)
    stem=rt_pkpm_es_shock_reflect_p2; param=n0
    files=('elc-imom.gkyl:rt_pkpm_es_shock_reflect_p2-elc-imom.gkyl' 'elc-L2.gkyl:rt_pkpm_es_shock_reflect_p2-elc-L2.gkyl' 'ion-imom.gkyl:rt_pkpm_es_shock_reflect_p2-ion-imom.gkyl' 'ion-L2.gkyl:rt_pkpm_es_shock_reflect_p2-ion-L2.gkyl' 'field-energy.gkyl:rt_pkpm_es_shock_reflect_p2-field-energy.gkyl') ;;
  pkpm-sheath-p1)
    stem=rt_pkpm_sheath_p1; param=n0
    files=('elc-imom.gkyl:rt_pkpm_sheath_p1-elc-imom.gkyl' 'elc-L2.gkyl:rt_pkpm_sheath_p1-elc-L2.gkyl' 'ion-imom.gkyl:rt_pkpm_sheath_p1-ion-imom.gkyl' 'ion-L2.gkyl:rt_pkpm_sheath_p1-ion-L2.gkyl' 'field-energy.gkyl:rt_pkpm_sheath_p1-field-energy.gkyl') ;;
  pkpm-alfven-soliton-p2)
    stem=rt_pkpm_alf_soliton_1x_p2; param=a
    files=('elc.gkyl:rt_pkpm_alf_soliton_1x_p2-elc_1.gkyl' 'ion.gkyl:rt_pkpm_alf_soliton_1x_p2-ion_1.gkyl' 'elc-pkpm-fluid.gkyl:rt_pkpm_alf_soliton_1x_p2-elc_pkpm_fluid_1.gkyl' 'ion-pkpm-fluid.gkyl:rt_pkpm_alf_soliton_1x_p2-ion_pkpm_fluid_1.gkyl' 'field.gkyl:rt_pkpm_alf_soliton_1x_p2-field_1.gkyl') ;;
  pkpm-es-potential-well-p1)
    stem=rt_pkpm_es_pot_well_1x_p1; param=vt
    files=('elc-imom.gkyl:rt_pkpm_es_pot_well_1x_p1-elc-imom.gkyl' 'elc-L2.gkyl:rt_pkpm_es_pot_well_1x_p1-elc-L2.gkyl' 'field-energy.gkyl:rt_pkpm_es_pot_well_1x_p1-field-energy.gkyl') ;;
  pkpm-moment-beach-p2)
    stem=rt_pkpm_mom_beach_p2; param=J0
    files=('elc.gkyl:rt_pkpm_mom_beach_p2-elc_1.gkyl' 'elc-pkpm-fluid.gkyl:rt_pkpm_mom_beach_p2-elc_pkpm_fluid_1.gkyl' 'field.gkyl:rt_pkpm_mom_beach_p2-field_1.gkyl' 'field-app-current.gkyl:rt_pkpm_mom_beach_p2-field_app_current_1.gkyl') ;;
  pkpm-square-relaxation-p1)
    stem=rt_pkpm_square_relax_1x_p1; param=n0
    files=('neut-imom.gkyl:rt_pkpm_square_relax_1x_p1-neut-imom.gkyl' 'neut-L2.gkyl:rt_pkpm_square_relax_1x_p1-neut-L2.gkyl' 'field-energy.gkyl:rt_pkpm_square_relax_1x_p1-field-energy.gkyl') ;;
  pkpm-traveling-pulse-2d-p1)
    stem=rt_pkpm_2d_travel_pulse_p1; param=alpha
    files=('neut-imom.gkyl:rt_pkpm_2d_travel_pulse_p1-neut-imom.gkyl' 'neut-L2.gkyl:rt_pkpm_2d_travel_pulse_p1-neut-L2.gkyl' 'field-energy.gkyl:rt_pkpm_2d_travel_pulse_p1-field-energy.gkyl') ;;
  *) echo "unknown check directory" >&2; exit 2 ;;
esac
[ -f "$CHECK_DIR/ic/$INPUTS/value.txt" ] || { echo "missing ic/$INPUTS/value.txt" >&2; exit 2; }
VALUE="$(tr -d '[:space:]' < "$CHECK_DIR/ic/$INPUTS/value.txt")"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -a "$SOURCE_DIR/." "$WORK/src"   # -a keeps the mtimes of the prebuilt library trees, so make relinks only the driver
python3 - "$WORK/src/pkpm/creg/$stem.c" "$param" "$VALUE" <<'PY'
import re, sys
path, param, value = sys.argv[1:]
text = open(path, encoding="utf-8").read()
# The first declaration of the parameter whose right-hand side is a numeric literal: the driver's context value,
# not a later local copy such as "double a = app->a;" inside a per-species eval callback.
number = r"[-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?"
pattern = rf"(^[ \t]*double[ \t]+{re.escape(param)}[ \t]*=[ \t]*)({number})([ \t]*;[^\n]*$)"
text, count = re.subn(pattern, lambda m: m.group(1)+value+m.group(3), text, count=1, flags=re.M)
if count != 1:
    raise SystemExit(f"could not replace the literal declaration of {param}")
open(path, "w", encoding="utf-8").write(text)
PY
cd "$WORK/src"
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
arch_flags="-march=native"; case "$(uname -m)" in aarch64|arm64) arch_flags="$arch_flags -D__arm64__" ;; esac
BUILD_START=$(date +%s)
./configure --prefix=/usr --app=pkpm "ARCH_FLAGS=$arch_flags" --lapack-inc=/usr/include --lapack-lib="/usr/lib/$multiarch" --lapack-lib-name="openblas -llapacke" --superlu-inc=/usr/include/superlu --superlu-lib="/usr/lib/$multiarch" >/dev/null
make -j"$SAB_MAKE_JOBS" ${MAKE_OVERRIDES[@]+"${MAKE_OVERRIDES[@]}"} pkpm >/dev/null
make -j"$SAB_MAKE_JOBS" ${MAKE_OVERRIDES[@]+"${MAKE_OVERRIDES[@]}"} "$BUILD_DIR/pkpm/creg/$stem" >/dev/null
echo "SAB_BUILD_SECONDS=$(( $(date +%s)-BUILD_START ))"
args=()
[ "$SAB_STEPS" = upstream ] || args+=("-s$SAB_STEPS")
[ "$SAB_XCELLS" = upstream ] || args+=("-x$SAB_XCELLS")
[ "$SAB_YCELLS" = upstream ] || args+=("-y$SAB_YCELLS")
[ "$SAB_VX_CELLS" = upstream ] || args+=("-u$SAB_VX_CELLS")
export LD_LIBRARY_PATH="$WORK/src/$BUILD_DIR/pkpm:$WORK/src/$BUILD_DIR/gyrokinetic:$WORK/src/$BUILD_DIR/vlasov:$WORK/src/$BUILD_DIR/moments:$WORK/src/$BUILD_DIR/core${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir "$WORK/run"; (cd "$WORK/run" && "$WORK/src/$BUILD_DIR/pkpm/creg/$stem" ${args[@]+"${args[@]}"})
for spec in "${files[@]}"; do
  out=${spec%%:*}; src=${spec#*:}
  [ -f "$WORK/run/$src" ] || { echo "missing $src" >&2; exit 1; }
  cp "$WORK/run/$src" "$OUT_DIR/$out"
done
