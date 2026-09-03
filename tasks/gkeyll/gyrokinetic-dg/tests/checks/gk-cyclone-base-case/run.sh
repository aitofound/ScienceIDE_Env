#!/usr/bin/env bash
# Run one official Gkeyll gyrokinetic regression with a two-binary64-ULP input perturbation.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q+p-1)/p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEPS 100000 "number of update steps; bounded before the upstream small-step abort near step 200100"
knob SAB_XCELLS upstream "override first configuration-space resolution"
knob SAB_YCELLS upstream "override second configuration-space resolution when present"
knob SAB_VPAR_CELLS upstream "override parallel-velocity resolution"
knob SAB_MU_CELLS upstream "override magnetic-moment resolution"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs used only for excluded source-build time"
if [ "${1:-}" = --help ]; then printf '%s' "$KNOB_HELP"; exit 0; fi
set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
case "$(basename "$CHECK_DIR")" in
  gk-ion-sound)
    stem=rt_gk_ion_sound_1x2v_p1; param=alpha
    files=('elc-integrated-moms.gkyl:rt_gk_ion_sound_1x2v_p1-elc_integrated_moms.gkyl' 'ion-integrated-moms.gkyl:rt_gk_ion_sound_1x2v_p1-ion_integrated_moms.gkyl' 'elc-L2norm.gkyl:rt_gk_ion_sound_1x2v_p1-elc_L2norm.gkyl' 'ion-L2norm.gkyl:rt_gk_ion_sound_1x2v_p1-ion_L2norm.gkyl' 'field-energy.gkyl:rt_gk_ion_sound_1x2v_p1-field_energy.gkyl') ;;
  gk-lbo-relaxation)
    stem=rt_gk_lbo_relax_1x2v_p1; param=nu
    files=('square-integrated-moms.gkyl:rt_gk_lbo_relax_1x2v_p1-square_integrated_moms.gkyl' 'bump-integrated-moms.gkyl:rt_gk_lbo_relax_1x2v_p1-bump_integrated_moms.gkyl' 'square-L2norm.gkyl:rt_gk_lbo_relax_1x2v_p1-square_L2norm.gkyl' 'bump-L2norm.gkyl:rt_gk_lbo_relax_1x2v_p1-bump_L2norm.gkyl') ;;
  gk-sheath-bgk)
    stem=rt_gk_sheath_bgk_1x2v_p1; param=nu_frac
    files=('elc-integrated-moms.gkyl:rt_gk_sheath_bgk_1x2v_p1-elc_integrated_moms.gkyl' 'ion-integrated-moms.gkyl:rt_gk_sheath_bgk_1x2v_p1-ion_integrated_moms.gkyl' 'elc-L2norm.gkyl:rt_gk_sheath_bgk_1x2v_p1-elc_L2norm.gkyl' 'ion-L2norm.gkyl:rt_gk_sheath_bgk_1x2v_p1-ion_L2norm.gkyl' 'field-energy.gkyl:rt_gk_sheath_bgk_1x2v_p1-field_energy.gkyl') ;;
  gk-cyclone-base-case)
    stem=rt_gk_cbc_2x2v_p1; param=n0
    files=('elc-integrated-moms.gkyl:rt_gk_cbc_2x2v_p1-elc_integrated_moms.gkyl' 'ion-integrated-moms.gkyl:rt_gk_cbc_2x2v_p1-ion_integrated_moms.gkyl' 'field-energy.gkyl:rt_gk_cbc_2x2v_p1-field_energy.gkyl') ;;
  *) echo "unknown check directory" >&2; exit 2 ;;
esac
[ -f "$CHECK_DIR/ic/$IC/value.txt" ] || { echo "missing ic/$IC/value.txt" >&2; exit 2; }
VALUE="$(tr -d '[:space:]' < "$CHECK_DIR/ic/$IC/value.txt")"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
python3 - "$WORK/src/gyrokinetic/creg/$stem.c" "$param" "$VALUE" <<'PY'
import re, sys
path, param, value = sys.argv[1:]
text = open(path, encoding="utf-8").read()
pattern = rf"(^[ \t]*double[ \t]+{re.escape(param)}[ \t]*=[ \t]*)([^;]+)(;[^\n]*$)"
text, count = re.subn(pattern, lambda m: m.group(1)+value+m.group(3), text, count=1, flags=re.M)
if count != 1: raise SystemExit(f"could not replace first declaration of {param}")
open(path, "w", encoding="utf-8").write(text)
PY
cd "$WORK/src"
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
arch_flags="-march=native"; case "$(uname -m)" in aarch64|arm64) arch_flags="$arch_flags -D__arm64__" ;; esac
BUILD_START=$(date +%s)
./configure --prefix=/usr --app=gyrokinetic "ARCH_FLAGS=$arch_flags" --lapack-inc=/usr/include --lapack-lib="/usr/lib/$multiarch" --lapack-lib-name="openblas -llapacke" --superlu-inc=/usr/include/superlu --superlu-lib="/usr/lib/$multiarch" >/dev/null
make -j"$SAB_MAKE_JOBS" gyrokinetic >/dev/null
make -j"$SAB_MAKE_JOBS" "build/gyrokinetic/creg/$stem" >/dev/null
echo "SAB_BUILD_SECONDS=$(( $(date +%s)-BUILD_START ))"
args=()
[ "$SAB_STEPS" = upstream ] || args+=("-s$SAB_STEPS")
[ "$SAB_XCELLS" = upstream ] || args+=("-x$SAB_XCELLS")
[ "$SAB_YCELLS" = upstream ] || args+=("-y$SAB_YCELLS")
[ "$SAB_VPAR_CELLS" = upstream ] || args+=("-u$SAB_VPAR_CELLS")
[ "$SAB_MU_CELLS" = upstream ] || args+=("-v$SAB_MU_CELLS")
export LD_LIBRARY_PATH="$WORK/src/build/gyrokinetic:$WORK/src/build/core${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir "$WORK/run"; (cd "$WORK/run" && "$WORK/src/build/gyrokinetic/creg/$stem" "${args[@]}")
for spec in "${files[@]}"; do out=${spec%%:*}; src=${spec#*:}; [ -f "$WORK/run/$src" ] || { echo "missing $src" >&2; exit 1; }; cp "$WORK/run/$src" "$OUT_DIR/$out"; done
