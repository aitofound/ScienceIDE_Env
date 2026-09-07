#!/usr/bin/env bash
#
# Step 1: Prepare SPECFEM3D mesh/database input files and run the mesher/generator.
#
# This script reads parameters from DATA/Par_file_SEM_DSM, calculates mesh
# resolution, prepares the DSM model, and updates SPECFEM3D input files.
# It then runs xmeshfem3D and xgenerate_databases using MPI.

set -euo pipefail

# --- 1. Path Setup ---
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${ROOT_DIR}/WORK/SPECFEM3D"
DATA_DIR="${WORK_DIR}/DATA"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
TEMPLATE_DIR=$(cd "${ROOT_DIR}/../../Input_Example/WORK/SPECFEM3D" && pwd)
TEMPLATE_DATA="${TEMPLATE_DIR}/DATA"
BIN_DIR="$(cd "${ROOT_DIR}/../../../src/SPECFEM3D/bin" && pwd)"
MESHFEM_DIR="${DATA_DIR}/meshfem3D_files"
DSM_MODEL="${DATA_DIR}/dsm_model_input"
DSM_MODEL_BASE="${ROOT_DIR}/DATA/dsm_model_base"

echo "----------------------------------------------------------------------"
echo "Starting Step 1: SPECFEM3D Preparation and Execution"
echo "Root Directory:    ${ROOT_DIR}"
echo "Working Directory: ${WORK_DIR}"
echo "Template Data:     ${TEMPLATE_DATA}"
echo "----------------------------------------------------------------------"

# --- 2. Helper Functions ---

param() {
  local key=$1
  local default=${2:-}
  local value
  value=$(awk -F= -v key="$key" '
    $0 !~ /^[[:space:]]*#/ && $1 ~ "^[[:space:]]*" key "[[:space:]]*$" {
      v=$2
      sub(/#.*/, "", v)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
      print v
      found=1
      exit
    }
    END { if (!found) exit 1 }
  ' "${PARAM_FILE}" 2>/dev/null || true)
  if [[ -n "${value}" ]]; then
    printf '%s\n' "${value}"
  else
    printf '%s\n' "${default}"
  fi
}

has_param() {
  local key=$1
  awk -F= -v key="$key" '
    $0 !~ /^[[:space:]]*#/ && $1 ~ "^[[:space:]]*" key "[[:space:]]*$" { found=1; exit }
    END { exit found ? 0 : 1 }
  ' "${PARAM_FILE}"
}

resolve_case_path() {
  local path=$1
  if [[ "${path}" = /* ]]; then
    printf '%s\n' "${path}"
  else
    printf '%s\n' "${ROOT_DIR}/${path}"
  fi
}

to_float() {
  printf '%s\n' "$1" | sed 's/[dD]/e/g'
}

is_false() {
  case "$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')" in
    .false.|false|f|0|no|n) return 0 ;;
    *) return 1 ;;
  esac
}

replace_key() {
  local file=$1 key=$2 value=$3
  awk -v key="$key" -v value="$value" '
    BEGIN { done=0 }
    $0 ~ "^[[:space:]]*" key "[[:space:]]*=" {
      comment = ""
      if (index($0, "#") > 0) comment = substr($0, index($0, "#"))
      printf "%-32s= %s", key, value
      if (comment != "") printf " %s", comment
      printf "\n"
      done=1
      next
    }
    { print }
    END { if (!done) printf "%-32s= %s\n", key, value }
  ' "$file" > "${file}.tmp"
  mv "${file}.tmp" "$file"
}

summary_line() {
  echo "$1"
}

choose_proc_grid() {
  local nproc=$1
  local xi eta root
  root=$(awk -v n="$nproc" 'BEGIN { print int(sqrt(n)) }')
  xi=$root
  while [[ ${xi} -gt 1 ]]; do
    if (( nproc % xi == 0 )); then
      eta=$((nproc / xi))
      printf '%s %s\n' "$xi" "$eta"
      return
    fi
    xi=$((xi - 1))
  done
  printf '1 %s\n' "$nproc"
}

round_up_multiple() {
  awk -v value="$1" -v multiple="$2" 'BEGIN {
    if (multiple < 1) multiple=1
    print int((value + multiple - 1) / multiple) * multiple
  }'
}

copy_template_data() {
  echo "Copying template data from ${TEMPLATE_DATA} to ${DATA_DIR}..."
  mkdir -p "${DATA_DIR}"
  (cd "${TEMPLATE_DATA}" && find . -type f ! -name '.DS_Store' -print) | while IFS= read -r rel; do
    mkdir -p "${DATA_DIR}/$(dirname "$rel")"
    cp "${TEMPLATE_DATA}/${rel}" "${DATA_DIR}/${rel}"
  done
  # Copy local DATA files that are needed
  cp "${ROOT_DIR}/DATA/CMTSOLUTION" "${DATA_DIR}/CMTSOLUTION"
}

constant_topography() {
  local value=$1 src=$2 dst=$3
  local nlines
  nlines=$(wc -l < "$src")
  awk -v value="$value" -v nlines="$nlines" 'BEGIN { for (i=0; i<nlines; i++) print value }' > "$dst"
}

constant_topography_grid() {
  local value=$1 nxi=$2 neta=$3 dst=$4
  awk -v value="$value" -v nxi="$nxi" -v neta="$neta" 'BEGIN {
    for (i=0; i<nxi*neta; i++) print value
  }' > "$dst"
}

convert_interface_topography() {
  local interface_index=$1 src=$2 baseline=$3 dst=$4
  local margin
  margin=$(to_float "$(param CMB_TOPOGRAPHY_TRUNCATION_MARGIN_M 40000.0)")
  if [[ ! -x "${BIN_DIR}/xcubedsphere_topo" ]]; then
    echo "Error: ${BIN_DIR}/xcubedsphere_topo is required to convert ${src} to ${dst}." >&2
    echo "Build it with: make -C \"${BIN_DIR}/..\" xcubedsphere_topo" >&2
    exit 1
  fi
  mkdir -p "${WORK_DIR}/OUTPUT_FILES"
  (cd "${WORK_DIR}" && "${BIN_DIR}/xcubedsphere_topo" "${interface_index}" "${src}" "${baseline}" \
    "$(awk -v z="${baseline}" -v m="${margin}" 'BEGIN { printf "%.6f", z-m }')" \
    "$(awk -v z="${baseline}" -v m="${margin}" 'BEGIN { printf "%.6f", z+m }')")
  [[ -s "${dst}" ]] || {
    echo "Error: topography conversion did not create ${dst}" >&2
    exit 1
  }
}

min_wave_speed_in_box() {
  local model=$1 bottom=$2 top=$3 earth=$4
  awk -v bottom="$bottom" -v top="$top" -v earth="$earth" '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      return line
    }
    function read_coeff(line, arr,    n, i, fields) {
      line=clean(line)
      n=split(line, fields)
      for (i=1; i<=n; i++) arr[i]=fields[i]+0.0
      return n
    }
    function eval_poly(arr, n, r,    x, xp, i, v) {
      x=r/earth
      xp=1.0
      v=0.0
      for (i=1; i<=n; i++) {
        v += arr[i] * xp
        xp *= x
      }
      return v
    }
    function min_positive(a, b) {
      if (a > 0.0 && b > 0.0) return (a < b ? a : b)
      if (a > 0.0) return a
      if (b > 0.0) return b
      return 0.0
    }
    NR == 4 { nz=$1+0; read_zones=1; next }
    read_zones && zone_count < nz {
      zone_line=$0
      getline vph_line
      getline vpv_line
      getline vsh_line
      getline vsv_line
      getline eta_line

      zone_line=clean(zone_line)
      split(zone_line, z)
      rmin=z[1]+0.0
      rmax=z[2]+0.0
      zone_count++
      if (rmax <= bottom || rmin >= top) next

      n_vph=read_coeff(vph_line, vph)
      n_vpv=read_coeff(vpv_line, vpv)
      n_vsh=read_coeff(vsh_line, vsh)
      n_vsv=read_coeff(vsv_line, vsv)

      lo=(rmin > bottom ? rmin : bottom)
      hi=(rmax < top ? rmax : top)
      for (i=0; i<=20; i++) {
        r=lo + (hi-lo) * i / 20.0
        s=min_positive(eval_poly(vsh, n_vsh, r), eval_poly(vsv, n_vsv, r))
        p=min_positive(eval_poly(vph, n_vph, r), eval_poly(vpv, n_vpv, r))
        v=(s > 0.0 ? s : p)
        if (v > 0.0 && (minv == 0.0 || v < minv)) minv=v
      }
    }
    END {
      if (minv <= 0.0) exit 1
      printf "%.6f\n", minv
    }
  ' "$model"
}


coupling_dist_tolerance_degrees() {
  local model=$1 bottom=$2 top=$3 earth=$4 frequency=$5 fraction=$6
  awk -v bottom="$bottom" -v top="$top" -v earth="$earth" \
      -v frequency="$frequency" -v fraction="$fraction" '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      return line
    }
    function read_coeff(line, arr,    n, i, fields) {
      line=clean(line)
      n=split(line, fields)
      for (i=1; i<=n; i++) arr[i]=fields[i]+0.0
      return n
    }
    function eval_poly(arr, n, r,    x, xp, i, v) {
      x=r/earth
      xp=1.0
      v=0.0
      for (i=1; i<=n; i++) {
        v += arr[i] * xp
        xp *= x
      }
      return v
    }
    function min_positive(a, b) {
      if (a > 0.0 && b > 0.0) return (a < b ? a : b)
      if (a > 0.0) return a
      if (b > 0.0) return b
      return 0.0
    }
    BEGIN {
      pi=atan2(0.0, -1.0)
      if (frequency <= 0.0 || fraction <= 0.0) exit 1
    }
    NR == 4 { nz=$1+0; read_zones=1; next }
    read_zones && zone_count < nz {
      zone_line=$0
      getline vph_line
      getline vpv_line
      getline vsh_line
      getline vsv_line
      getline eta_line

      zone_line=clean(zone_line)
      split(zone_line, z)
      rmin=z[1]+0.0
      rmax=z[2]+0.0
      zone_count++
      if (rmax <= bottom || rmin >= top) next

      n_vph=read_coeff(vph_line, vph)
      n_vpv=read_coeff(vpv_line, vpv)
      n_vsh=read_coeff(vsh_line, vsh)
      n_vsv=read_coeff(vsv_line, vsv)

      lo=(rmin > bottom ? rmin : bottom)
      hi=(rmax < top ? rmax : top)
      for (i=0; i<=20; i++) {
        r=lo + (hi-lo) * i / 20.0
        s=min_positive(eval_poly(vsh, n_vsh, r), eval_poly(vsv, n_vsv, r))
        p=min_positive(eval_poly(vph, n_vph, r), eval_poly(vpv, n_vpv, r))
        v=(s > 0.0 ? s : p)
        if (v > 0.0 && r > 0.0) {
          tolerance_deg=fraction * (v/frequency) / r * 180.0/pi
          if (tolerance_deg > 0.0 && (mintol == 0.0 || tolerance_deg < mintol)) mintol=tolerance_deg
        }
      }
    }
    END {
      if (mintol <= 0.0) exit 1
      printf "%.10f\n", mintol
    }
  ' "$model"
}

find_solid_fluid_interface() {
  local model=$1 bottom=$2 top=$3 earth=$4
  awk -v bottom="$bottom" -v top="$top" -v earth="$earth" '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      return line
    }
    function read_coeff(line, arr,    n, i, fields) {
      line=clean(line)
      n=split(line, fields)
      for (i=1; i<=n; i++) arr[i]=fields[i]+0.0
      return n
    }
    function eval_poly(arr, n, r,    x, xp, i, v) {
      x=r/earth
      xp=1.0
      v=0.0
      for (i=1; i<=n; i++) {
        v += arr[i] * xp
        xp *= x
      }
      return v
    }
    NR == 4 { nz=$1+0; read_zones=1; next }
    read_zones && zone_count < nz {
      zone_count++
      split(clean($0), z)
      rmin[zone_count]=z[1]+0.0
      rmax[zone_count]=z[2]+0.0
      getline vph_line
      getline vpv_line
      getline vsh_line
      getline vsv_line
      getline eta_line
      n_vsh=read_coeff(vsh_line, vsh)
      n_vsv=read_coeff(vsv_line, vsv)
      r=(rmin[zone_count]+rmax[zone_count])*0.5
      fluid[zone_count]=(eval_poly(vsh, n_vsh, r) <= 1.0e-6 && eval_poly(vsv, n_vsv, r) <= 1.0e-6)
      next
    }
    END {
      for (i=1; i<zone_count; i++) {
        boundary=rmax[i]
        if (boundary <= bottom + 1e-6 || boundary >= top - 1e-6) continue
        if (fluid[i] != fluid[i+1]) {
          printf "%.10f\n", boundary
          found=1
          exit
        }
      }
      if (!found) exit 1
    }
  ' "$model"
}
layer_material_id() {
  local model=$1 bottom=$2 top=$3 earth=$4
  awk -v bottom="$bottom" -v top="$top" -v earth="$earth" '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      return line
    }
    function read_coeff(line, arr,    n, i, fields) {
      line=clean(line)
      n=split(line, fields)
      for (i=1; i<=n; i++) arr[i]=fields[i]+0.0
      return n
    }
    function eval_poly(arr, n, r,    x, xp, i, v) {
      x=r/earth
      xp=1.0
      v=0.0
      for (i=1; i<=n; i++) {
        v += arr[i] * xp
        xp *= x
      }
      return v
    }
    NR == 4 { nz=$1+0; read_zones=1; next }
    read_zones && zone_count < nz {
      zone_line=$0
      getline vph_line
      getline vpv_line
      getline vsh_line
      getline vsv_line
      getline eta_line
      split(clean(zone_line), z)
      rmin=z[1]+0.0
      rmax=z[2]+0.0
      zone_count++
      if (rmax <= bottom || rmin >= top) next
      n_vsh=read_coeff(vsh_line, vsh)
      n_vsv=read_coeff(vsv_line, vsv)
      lo=(rmin > bottom ? rmin : bottom)
      hi=(rmax < top ? rmax : top)
      for (i=0; i<=4; i++) {
        r=lo + (hi-lo) * i / 4.0
        if (eval_poly(vsh, n_vsh, r) > 1.0e-6 || eval_poly(vsv, n_vsv, r) > 1.0e-6) solid=1
        sampled=1
      }
    }
    END {
      if (!sampled) exit 1
      print (solid ? 2 : 1)
    }
  ' "$model"
}

bounded_count() {
  local value=$1 min=$2 max=$3
  awk -v v="$value" -v min="$min" -v max="$max" 'BEGIN {
    v=int(v)
    if (v < min) v=min
    if (v > max) v=max
    print v
  }'
}

prepare_dsm_model() {
  local src=$1 dst=$2 attenuation_target_depths=$3 bottom=$4 top=$5 buffer=$6 q_large=$7

  [[ -f "${src}" ]] || { echo "Missing provided DSM model: ${src}" >&2; exit 1; }

  if ! is_false "${attenuation_target_depths}"; then
    cp "${src}" "${dst}"
    return
  fi

  awk -v target_bottom="$bottom" -v target_top="$top" -v buffer="$buffer" -v q_large="$q_large" '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      return line
    }
    function zone_line(line, rmin_new, rmax_new,    fields, n, i, out) {
      n=split(clean(line), fields)
      out=sprintf("%10.4f %10.4f", rmin_new, rmax_new)
      for (i=3; i<=n; i++) out=out sprintf(" %10s", fields[i])
      return out
    }
    function q_line(line, disable,    comment, body, fields, n, i, out, qmu) {
      comment=""
      if (index(line, "#") > 0) comment=substr(line, index(line, "#"))
      body=clean(line)
      n=split(body, fields)
      if (disable && n >= 2) {
        qmu=fields[n-1] + 0.0
        if (qmu >= 0.0) fields[n-1]=q_large
        fields[n]=q_large
      }
      out=sprintf("%31s", fields[1])
      for (i=2; i<=n; i++) out=out sprintf(" %8s", fields[i])
      if (comment != "") out=out " " comment
      return out
    }
    function qmu_value(line,    fields, n) {
      n=split(clean(line), fields)
      if (n < 2) return 0.0
      return fields[n-1] + 0.0
    }
    function emit_segment(r1, r2, disable,    k) {
      if (r2 <= r1 + 1e-7) return
      out[++nout]=zone_line(zline, r1, r2)
      for (k=1; k<=4; k++) out[++nout]=props[k]
      out[++nout]=q_line(eline, disable)
      nzout++
    }
    NR <= 4 {
      header[NR]=$0
      if (NR == 4) {
        split(clean($0), h)
        nz=h[1]+0
      }
      next
    }
    zone_count < nz {
      zline=$0
      getline props[1]
      getline props[2]
      getline props[3]
      getline props[4]
      getline eline

      split(clean(zline), z)
      rmin=z[1]+0.0
      rmax=z[2]+0.0
      lo=target_bottom-buffer
      hi=target_top+buffer

      if (rmax <= lo || rmin >= hi || qmu_value(eline) < 0.0) {
        emit_segment(rmin, rmax, 0)
      } else {
        a=(rmin > lo ? rmin : lo)
        b=(rmax < hi ? rmax : hi)
        emit_segment(rmin, a, 0)
        emit_segment(a, b, 1)
        emit_segment(b, rmax, 0)
      }
      zone_count++
      next
    }
    { tail[++ntail]=$0 }
    END {
      if (nz <= 0 || zone_count != nz) exit 1
      print header[1]
      print header[2]
      print header[3]
      comment=""
      if (index(header[4], "#") > 0) comment=substr(header[4], index(header[4], "#"))
      printf "%4d", nzout
      if (comment != "") printf "   %s", comment
      printf "\n"
      for (i=1; i<=nout; i++) print out[i]
      for (i=1; i<=ntail; i++) print tail[i]
    }
  ' "${src}" > "${dst}.tmp" || {
    rm -f "${dst}.tmp"
    echo "Could not create no-attenuation DSM model from ${src}" >&2
    exit 1
  }
  mv "${dst}.tmp" "${dst}"
}

nex_from_wavelength() {
  local angular_width=$1 radius=$2 speed=$3 frequency=$4 elements_per_wavelength=$5
  awk -v width_deg="$angular_width" -v radius="$radius" -v speed="$speed" \
      -v freq="$frequency" -v epw="$elements_per_wavelength" 'BEGIN {
    if (freq <= 0.0 || speed <= 0.0 || epw <= 0.0) exit 1
    width_km = width_deg * atan2(0,-1) / 180.0 * radius
    wavelength_km = speed / freq
    element_km = wavelength_km / epw
    n = int(width_km / element_km + 0.999999)
    if (n < 8) n=8
    print n
  }'
}

nz_from_wavelength() {
  local bottom=$1 top=$2 speed=$3 frequency=$4 elements_per_wavelength=$5
  awk -v bottom="$bottom" -v top="$top" -v speed="$speed" \
      -v freq="$frequency" -v epw="$elements_per_wavelength" 'BEGIN {
    if (freq <= 0.0 || speed <= 0.0 || epw <= 0.0 || top <= bottom) exit 1
    thickness_km = top - bottom
    wavelength_km = speed / freq
    element_km = wavelength_km / epw
    n = int(thickness_km / element_km + 0.999999)
    if (n < 1) n=1
    print n
  }'
}

# --- 3. Main Logic ---

# Prepare DATA directory
copy_template_data
mkdir -p "${MESHFEM_DIR}"
find "${MESHFEM_DIR}" -maxdepth 1 -type f -name 'topo_*.dat' ! -name 'topo_top.dat' -delete

R_EARTH=$(to_float "$(param R_EARTH 6371.0)")
FREQ_RESOLVED=$(to_float "$(param FREQ_RESOLVED 1.0)")
DSM1D_OR_3D=$(param DSM1D_OR_3D DSM3D)
DSM1D_OR_3D=$(printf '%s' "${DSM1D_OR_3D}" | tr '[:lower:]' '[:upper:]')
case "${DSM1D_OR_3D}" in
  DSM1D|DSM3D) ;;
  *)
    echo "Error: DSM1D_OR_3D must be DSM1D or DSM3D, got '${DSM1D_OR_3D}'." >&2
    exit 1
    ;;
esac

SIMULATION_TYPE=$(param SIMULATION_TYPE 1)
COUPLING_TYPE=$(param COUPLING_TYPE 3)
SINGLE_FORCE_ENZ=$(param SINGLE_FORCE_ENZ -1)
NPROC_LIMIT=$(param NPROC 1)
SAVE_MESH_FILES=$(param SAVE_MESH_FILES .true.)
ATTENUATION_TARGET_DEPTHS=$(param ATTENUATION_TARGET_DEPTHS .false.)
DSM_ATTENUATION_BUFFER_KM=$(to_float "$(param DSM_ATTENUATION_BUFFER_KM 10.0)")
DSM_NO_ATTENUATION_Q=$(to_float "$(param DSM_NO_ATTENUATION_Q 100000.0)")
CENTER_LAT=$(to_float "$(param CENTER_LATITUDE_IN_DEGREES 0.0)")
CENTER_LON=$(to_float "$(param CENTER_LONGITUDE_IN_DEGREES 0.0)")
CENTER_DEPTH_KM=$(to_float "$(param CENTER_DEPTH_KM 0.0)")
DEPTH_BLOCK_KM=$(to_float "$(param DEPTH_BLOCK_KM 1.0)")
GAMMA_ROTATION=$(param GAMMA_ROTATION_AZIMUTH 0.d0)
ANGULAR_WIDTH_XI=$(to_float "$(param ANGULAR_WIDTH_XI_IN_DEGREES 1.0)")
ANGULAR_WIDTH_ETA=$(to_float "$(param ANGULAR_WIDTH_ETA_IN_DEGREES 1.0)")
USE_REGULAR_MESH=$(param USE_REGULAR_MESH .true.)
ILAYER_IRREGULAR_MESH=$(param ILAYER_IRREGULAR_MESH 2)
LOW_RESOLUTION=$(param LOW_RESOLUTION .false.)
COUPLING_DIST_WAVELENGTH_FRACTION=$(to_float "$(param COUPLING_DIST_WAVELENGTH_FRACTION 0.05)")

TOP_RADIUS_KM=$(awk -v r="${R_EARTH}" -v c="${CENTER_DEPTH_KM}" -v d="${DEPTH_BLOCK_KM}" 'BEGIN { printf "%.10f", r - c + d/2.0 }')
BOTTOM_RADIUS_KM=$(awk -v r="${R_EARTH}" -v c="${CENTER_DEPTH_KM}" -v d="${DEPTH_BLOCK_KM}" 'BEGIN { printf "%.10f", r - c - d/2.0 }')
CENTER_RADIUS_KM=$(awk -v r="${R_EARTH}" -v c="${CENTER_DEPTH_KM}" 'BEGIN { printf "%.10f", r - c }')
R_TOP_BOUND=$(param R_TOP_BOUND "$(awk -v r="${TOP_RADIUS_KM}" 'BEGIN { printf "%.1f", r*1000.0 }')")
ELEMENTS_PER_WAVELENGTH=$(to_float "$(param ELEMENTS_PER_WAVELENGTH 1.5)")
OUTER_CORE_ELEMENTS_PER_WAVELENGTH=$(to_float "$(param OUTER_CORE_ELEMENTS_PER_WAVELENGTH 2.0)")

prepare_dsm_model "${DSM_MODEL_BASE}" "${DSM_MODEL}" "${ATTENUATION_TARGET_DEPTHS}" \
  "${BOTTOM_RADIUS_KM}" "${TOP_RADIUS_KM}" "${DSM_ATTENUATION_BUFFER_KM}" "${DSM_NO_ATTENUATION_Q}"

MIN_WAVE_SPEED_KM_S=$(min_wave_speed_in_box "${DSM_MODEL}" "${BOTTOM_RADIUS_KM}" "${TOP_RADIUS_KM}" "${R_EARTH}") || {
  echo "Could not compute minimum wave speed from ${DSM_MODEL}" >&2
  exit 1
}
MIN_WAVELENGTH_KM=$(awk -v v="${MIN_WAVE_SPEED_KM_S}" -v f="${FREQ_RESOLVED}" 'BEGIN {
  if (f <= 0.0) exit 1
  printf "%.6f", v/f
}')
TARGET_ELEMENT_SIZE_KM=$(awk -v w="${MIN_WAVELENGTH_KM}" -v epw="${ELEMENTS_PER_WAVELENGTH}" 'BEGIN {
  if (epw <= 0.0) exit 1
  printf "%.6f", w/epw
}')
COUPLING_DIST_TOLERENCE=$(coupling_dist_tolerance_degrees "${DSM_MODEL}"   "${BOTTOM_RADIUS_KM}" "${TOP_RADIUS_KM}" "${R_EARTH}"   "${FREQ_RESOLVED}" "${COUPLING_DIST_WAVELENGTH_FRACTION}") || {
  echo "Could not compute COUPLING_DIST_TOLERENCE from ${DSM_MODEL}" >&2
  exit 1
}

NEX_XI=$(param NEX_XI "")
NEX_ETA=$(param NEX_ETA "")
if [[ -z "${NEX_XI}" ]]; then
  NEX_XI=$(nex_from_wavelength "${ANGULAR_WIDTH_XI}" "${CENTER_RADIUS_KM}" "${MIN_WAVE_SPEED_KM_S}" "${FREQ_RESOLVED}" "${ELEMENTS_PER_WAVELENGTH}")
fi
if [[ -z "${NEX_ETA}" ]]; then
  NEX_ETA=$(nex_from_wavelength "${ANGULAR_WIDTH_ETA}" "${CENTER_RADIUS_KM}" "${MIN_WAVE_SPEED_KM_S}" "${FREQ_RESOLVED}" "${ELEMENTS_PER_WAVELENGTH}")
fi

NPROC_XI=$(param NPROC_XI "")
NPROC_ETA=$(param NPROC_ETA "")
if [[ -z "${NPROC_XI}" || -z "${NPROC_ETA}" ]]; then
  read -r NPROC_XI NPROC_ETA < <(choose_proc_grid "${NPROC_LIMIT}")
fi
NEX_XI=$(round_up_multiple "${NEX_XI}" "${NPROC_XI}")
NEX_ETA=$(round_up_multiple "${NEX_ETA}" "${NPROC_ETA}")
NPROC=$((NPROC_XI * NPROC_ETA))

CMB_RADIUS=$(find_solid_fluid_interface "${DSM_MODEL}" "${BOTTOM_RADIUS_KM}" "${TOP_RADIUS_KM}" "${R_EARTH}") || {
  echo "Could not find a solid/fluid interface inside the SEM box from ${DSM_MODEL}" >&2
  exit 1
}

LOWER_LAYER_SPEED_KM_S=$(min_wave_speed_in_box "${DSM_MODEL}" "${BOTTOM_RADIUS_KM}" "${CMB_RADIUS}" "${R_EARTH}")
UPPER_LAYER_SPEED_KM_S=$(min_wave_speed_in_box "${DSM_MODEL}" "${CMB_RADIUS}" "${TOP_RADIUS_KM}" "${R_EARTH}")
LOWER_MATERIAL_ID=$(layer_material_id "${DSM_MODEL}" "${BOTTOM_RADIUS_KM}" "${CMB_RADIUS}" "${R_EARTH}")
UPPER_MATERIAL_ID=$(layer_material_id "${DSM_MODEL}" "${CMB_RADIUS}" "${TOP_RADIUS_KM}" "${R_EARTH}")
LOWER_EPW="${ELEMENTS_PER_WAVELENGTH}"
UPPER_EPW="${ELEMENTS_PER_WAVELENGTH}"
if [[ "${LOWER_MATERIAL_ID}" -eq 1 ]]; then
  LOWER_EPW="${OUTER_CORE_ELEMENTS_PER_WAVELENGTH}"
fi
if [[ "${UPPER_MATERIAL_ID}" -eq 1 ]]; then
  UPPER_EPW="${OUTER_CORE_ELEMENTS_PER_WAVELENGTH}"
fi

NZ_LOWER_DEFAULT=$(nz_from_wavelength "${BOTTOM_RADIUS_KM}" "${CMB_RADIUS}" "${LOWER_LAYER_SPEED_KM_S}" "${FREQ_RESOLVED}" "${LOWER_EPW}")
NZ_UPPER_DEFAULT=$(nz_from_wavelength "${CMB_RADIUS}" "${TOP_RADIUS_KM}" "${UPPER_LAYER_SPEED_KM_S}" "${FREQ_RESOLVED}" "${UPPER_EPW}")
NZ_LOWER=$(param NZ_BOTTOM "${NZ_LOWER_DEFAULT}")
NZ_UPPER=$(param NZ_TOP "${NZ_UPPER_DEFAULT}")
NZ_TOTAL=$((NZ_LOWER + NZ_UPPER))

COUPLING_IXI_LOW=$(param COUPLING_IXI_LOW 2)
COUPLING_IETA_LOW=$(param COUPLING_IETA_LOW 2)
COUPLING_IXI_HIGH=$(param COUPLING_IXI_HIGH "$((NEX_XI - 2))")
COUPLING_IETA_HIGH=$(param COUPLING_IETA_HIGH "$((NEX_ETA - 2))")
COUPLING_IR_BOTTOM_DEFAULT=$(awk -v n="${NZ_TOTAL}" 'BEGIN { v=2; if (n <= 3) v=1; if (v > n) v=n; print v }')
COUPLING_IR_TOP_DEFAULT=$(awk -v n="${NZ_TOTAL}" -v b="${COUPLING_IR_BOTTOM_DEFAULT}" 'BEGIN { v=n-2; if (v <= b) v=n; if (v < 1) v=1; print v }')
COUPLING_IR_BOTTOM=$(param COUPLING_IR_BOTTOM "${COUPLING_IR_BOTTOM_DEFAULT}")
COUPLING_IR_TOP=$(param COUPLING_IR_TOP "${COUPLING_IR_TOP_DEFAULT}")

CMB_AUX_ELEMENTS_FROM_COUPLING=$(param CMB_AUX_ELEMENTS_FROM_COUPLING 1)
CMB_AUX_ELEMENTS_FROM_COUPLING=$(bounded_count "${CMB_AUX_ELEMENTS_FROM_COUPLING}" 1 1000000)

# Keep only the requested buffer next to coupling faces. All remaining elements
# stay on the CMB side of the auxiliary interfaces.
NZ_BELOW_CMB_AUX=$(bounded_count "$((COUPLING_IR_BOTTOM + CMB_AUX_ELEMENTS_FROM_COUPLING))" 1 "$((NZ_LOWER - 1))")
NZ_ABOVE_CMB_AUX_TOP=$(bounded_count "$((NZ_TOTAL - COUPLING_IR_TOP + CMB_AUX_ELEMENTS_FROM_COUPLING))" 1 "$((NZ_UPPER - 1))")
NZ_AUX_TO_CMB=$((NZ_LOWER - NZ_BELOW_CMB_AUX))
NZ_CMB_TO_AUX=$((NZ_UPPER - NZ_ABOVE_CMB_AUX_TOP))

Z_BELOW_CMB_AUX=$(awk -v bottom="${BOTTOM_RADIUS_KM}" -v cmb="${CMB_RADIUS}" -v n="${NZ_BELOW_CMB_AUX}" -v nt="${NZ_LOWER}" -v top="${TOP_RADIUS_KM}" 'BEGIN { r=bottom + (cmb-bottom)*n/nt; printf "%.6f", (r-top)*1000.0 }')
Z_CMB=$(awk -v r="${CMB_RADIUS}" -v top="${TOP_RADIUS_KM}" 'BEGIN { printf "%.6f", (r-top)*1000.0 }')
Z_ABOVE_CMB_AUX=$(awk -v cmb="${CMB_RADIUS}" -v top="${TOP_RADIUS_KM}" -v n="${NZ_CMB_TO_AUX}" -v nt="${NZ_UPPER}" 'BEGIN { r=cmb + (top-cmb)*n/nt; printf "%.6f", (r-top)*1000.0 }')
UPPER_AUX_RADIUS_KM=$(awk -v z="${Z_ABOVE_CMB_AUX}" -v top="${TOP_RADIUS_KM}" 'BEGIN { printf "%.10f", top + z/1000.0 }')
ELASTIC_DISCON_RADIUS_KM=$(awk -v cmb="${CMB_RADIUS}" -v upper="${UPPER_AUX_RADIUS_KM}" '
  function clean(line) {
    sub(/#.*/, "", line)
    gsub(/[dD]/, "e", line)
    return line
  }
  NR == 4 { nz=$1+0; read_zones=1; next }
  read_zones && zone_count < nz {
    line=clean($0)
    getline vph_line
    getline vpv_line
    getline vsh_line
    getline vsv_line
    getline eta_line
    split(line, z)
    zone_count++
    r=z[1]+0.0
    if (r > cmb + 1.0e-5 && r < upper - 1.0e-5) {
      printf "%.10f\n", r
      exit
    }
  }
' "${DSM_MODEL}")
if [[ -n "${ELASTIC_DISCON_RADIUS_KM}" ]]; then
  Z_ELASTIC_DISCON=$(awk -v r="${ELASTIC_DISCON_RADIUS_KM}" -v top="${TOP_RADIUS_KM}" 'BEGIN { printf "%.6f", (r-top)*1000.0 }')
  NZ_CMB_TO_DISCON=$(awk -v n="${NZ_CMB_TO_AUX}" -v cmb="${CMB_RADIUS}" -v dis="${ELASTIC_DISCON_RADIUS_KM}" -v upper="${UPPER_AUX_RADIUS_KM}" 'BEGIN {
    v=int(n*(dis-cmb)/(upper-cmb)+0.5)
    if (v < 1) v=1
    if (v > n-1) v=n-1
    print v
  }')
  NZ_DISCON_TO_AUX=$((NZ_CMB_TO_AUX - NZ_CMB_TO_DISCON))
else
  Z_ELASTIC_DISCON=
  NZ_CMB_TO_DISCON=
  NZ_DISCON_TO_AUX=
fi

format_depth_label() {
  awk -v km="$1" 'BEGIN {
    s=sprintf("%.2f", km)
    sub(/0+$/, "", s)
    sub(/\.$/, "", s)
    gsub(/\./, "p", s)
    print s
  }'
}
BELOW_CMB_AUX_OFFSET_KM=$(awk -v z="${Z_BELOW_CMB_AUX}" -v zi="${Z_CMB}" 'BEGIN { printf "%.6f", (zi-z)/1000.0 }')
ABOVE_CMB_AUX_OFFSET_KM=$(awk -v z="${Z_ABOVE_CMB_AUX}" -v zi="${Z_CMB}" 'BEGIN { printf "%.6f", (z-zi)/1000.0 }')
TOPO_BELOW_CMB_FILE="topo_$(format_depth_label "${BELOW_CMB_AUX_OFFSET_KM}")kmBelowCMB.dat"
TOPO_ABOVE_CMB_FILE="topo_$(format_depth_label "${ABOVE_CMB_AUX_OFFSET_KM}")kmAboveCMB.dat"
if [[ -n "${ELASTIC_DISCON_RADIUS_KM}" ]]; then
  ELASTIC_DISCON_OFFSET_KM=$(awk -v z="${Z_ELASTIC_DISCON}" -v zi="${Z_CMB}" 'BEGIN { printf "%.6f", (z-zi)/1000.0 }')
  TOPO_ELASTIC_DISCON_FILE="topo_$(format_depth_label "${ELASTIC_DISCON_OFFSET_KM}")kmAboveCMB_DSMDiscon.dat"
else
  TOPO_ELASTIC_DISCON_FILE=
fi

CMB_TOPO_SOURCE=$(param CMB_TOPOGRAPHY_FILE "DATA/topo_files/latlon_CMB_topo.txt")
if [[ "${CMB_TOPO_SOURCE}" != /* ]]; then
  CMB_TOPO_SOURCE="${ROOT_DIR}/${CMB_TOPO_SOURCE}"
fi
TOMOGRAPHY_PATH=$(param TOMOGRAPHY_PATH "DATA/heterogeneities/tiles")
if [[ "${TOMOGRAPHY_PATH}" != /* ]]; then
  TOMOGRAPHY_PATH="${ROOT_DIR}/${TOMOGRAPHY_PATH}"
fi
if [[ "${DSM1D_OR_3D}" == "DSM3D" ]]; then
  [[ -f "${CMB_TOPO_SOURCE}" ]] || { echo "Missing CMB topography source: ${CMB_TOPO_SOURCE}" >&2; exit 1; }
  [[ -d "${TOMOGRAPHY_PATH}" ]] || { echo "Missing heterogeneity tile directory: ${TOMOGRAPHY_PATH}" >&2; exit 1; }
  compgen -G "${TOMOGRAPHY_PATH}/tomography_model_box*.xyz" >/dev/null || { echo "No tomography_model_box*.xyz files found in ${TOMOGRAPHY_PATH}" >&2; exit 1; }
  cp "${CMB_TOPO_SOURCE}" "${MESHFEM_DIR}/latlon_interface_topo.txt"
fi

replace_key "${DATA_DIR}/Par_file" MODEL "${DSM1D_OR_3D}"
replace_key "${DATA_DIR}/Par_file" SIMULATION_TYPE "${SIMULATION_TYPE}"
replace_key "${DATA_DIR}/Par_file" COUPLING_TYPE "${COUPLING_TYPE}"
replace_key "${DATA_DIR}/Par_file" SINGLE_FORCE_ENZ "${SINGLE_FORCE_ENZ}"
replace_key "${DATA_DIR}/Par_file" NPROC "${NPROC}"
replace_key "${DATA_DIR}/Par_file" SAVE_MESH_FILES "${SAVE_MESH_FILES}"
replace_key "${DATA_DIR}/Par_file" ATTENUATION "${ATTENUATION_TARGET_DEPTHS}"
replace_key "${DATA_DIR}/Par_file" TOMOGRAPHY_PATH "${TOMOGRAPHY_PATH}"
INJECTED_WAVEFIELD_PATH=$(resolve_case_path "$(param INJECTED_WAVEFIELD_PATH "WORK/InjectedWaves/OUTPUT_FILES")")
replace_key "${DATA_DIR}/Par_file" INJECTED_WAVEFIELD_PATH "${INJECTED_WAVEFIELD_PATH}"

replace_key "${MESHFEM_DIR}/Mesh_Par_file" LATITUDE_MIN "0.d0"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" LATITUDE_MAX "${ANGULAR_WIDTH_ETA}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" LONGITUDE_MIN "0.d0"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" LONGITUDE_MAX "${ANGULAR_WIDTH_XI}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" DEPTH_BLOCK_KM "${DEPTH_BLOCK_KM}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" NEX_XI "${NEX_XI}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" NEX_ETA "${NEX_ETA}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" NPROC_XI "${NPROC_XI}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" NPROC_ETA "${NPROC_ETA}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" USE_REGULAR_MESH "${USE_REGULAR_MESH}"
replace_key "${MESHFEM_DIR}/Mesh_Par_file" NDOUBLINGS "${ILAYER_IRREGULAR_MESH}"

awk -v nex_xi="${NEX_XI}" -v nex_eta="${NEX_ETA}" -v nz_lower="${NZ_LOWER}" \
    -v nz_total="${NZ_TOTAL}" -v lower_material="${LOWER_MATERIAL_ID}" -v upper_material="${UPPER_MATERIAL_ID}" '
  BEGIN { skip=0 }
  /^[[:space:]]*NREGIONS[[:space:]]*=/ {
    print "NREGIONS                        = 2"
    print "# define the different regions of the model as :"
    print "#NEX_XI_BEGIN  #NEX_XI_END  #NEX_ETA_BEGIN  #NEX_ETA_END  #NZ_BEGIN #NZ_END  #material_id"
    print "#material_id - 1 for acoustic and 2 for elastic"
    printf "1 %d 1 %d %d %d %d\n", nex_xi, nex_eta, nz_lower + 1, nz_total, upper_material
    printf "1 %d 1 %d 1 %d %d\n", nex_xi, nex_eta, nz_lower, lower_material
    skip=1
    next
  }
  skip && /^[[:space:]]*#/ { next }
  skip && NF == 7 { next }
  { skip=0; print }
' "${MESHFEM_DIR}/Mesh_Par_file" > "${MESHFEM_DIR}/Mesh_Par_file.tmp"
mv "${MESHFEM_DIR}/Mesh_Par_file.tmp" "${MESHFEM_DIR}/Mesh_Par_file"

AUX_NXI=300
AUX_NETA=300

# The top interface is flat at z=0 relative to the top of the SEM box.
constant_topography_grid 0.0 "${AUX_NXI}" "${AUX_NETA}" "${MESHFEM_DIR}/topo_top.dat"
CMB_NXI=420
CMB_NETA=420
AUX_DXI=$(awk -v w="${ANGULAR_WIDTH_XI}" -v n="${AUX_NXI}" 'BEGIN { printf "%.10fd0", w/(n-1) }')
AUX_DETA=$(awk -v w="${ANGULAR_WIDTH_ETA}" -v n="${AUX_NETA}" 'BEGIN { printf "%.10fd0", w/(n-1) }')
if [[ "${DSM1D_OR_3D}" == "DSM1D" ]]; then
  CMB_NXI=${AUX_NXI}
  CMB_NETA=${AUX_NETA}
  CMB_DXI=${AUX_DXI}
  CMB_DETA=${AUX_DETA}
else
  CMB_DXI=$(awk -v w="${ANGULAR_WIDTH_XI}" -v n="${CMB_NXI}" 'BEGIN { printf "%.10fd0", w/(n-1) }')
  CMB_DETA=$(awk -v w="${ANGULAR_WIDTH_ETA}" -v n="${CMB_NETA}" 'BEGIN { printf "%.10fd0", w/(n-1) }')
fi

if [[ -n "${ELASTIC_DISCON_RADIUS_KM}" ]]; then
cat > "${MESHFEM_DIR}/interfaces.dat" <<EOF_INTERFACES
# number of interfaces
 5
#
# Interfaces covered by the SEM box. Topography values are relative to the top of the SEM box.
# Interface 5: auxiliary interface below the CMB, placed above the lower coupling boundary.
# SUPPRESS_UTM_PROJECTION  NXI  NETA  XI_MIN   ETA_MIN    SPACING_XI SPACING_ETA
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 ${TOPO_BELOW_CMB_FILE}
# Interface 4: CMB topography.
 .true.                    ${CMB_NXI} ${CMB_NETA} 0.d0       0.d0      ${CMB_DXI}    ${CMB_DETA}
 topo_CMB.dat
# Interface 3: DSM radial discontinuity above the CMB.
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 ${TOPO_ELASTIC_DISCON_FILE}
# Interface 2: auxiliary interface above the CMB, placed below the upper coupling boundary.
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 ${TOPO_ABOVE_CMB_FILE}
# Interface 1: Represents the top of the mesh
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 topo_top.dat
# Number of elements in the bottom layer
${NZ_BELOW_CMB_AUX}
# Number of elements between the lower auxiliary interface and the CMB
${NZ_AUX_TO_CMB}
# Number of elements between the CMB and the DSM radial discontinuity
${NZ_CMB_TO_DISCON}
# Number of elements between the DSM radial discontinuity and the upper auxiliary interface
${NZ_DISCON_TO_AUX}
# Number of elements in the top layer
${NZ_ABOVE_CMB_AUX_TOP}
EOF_INTERFACES
else
cat > "${MESHFEM_DIR}/interfaces.dat" <<EOF_INTERFACES
# number of interfaces
 4
#
# Interfaces covered by the SEM box. Topography values are relative to the top of the SEM box.
# Interface 4: auxiliary interface below the CMB, placed above the lower coupling boundary.
# SUPPRESS_UTM_PROJECTION  NXI  NETA  XI_MIN   ETA_MIN    SPACING_XI SPACING_ETA
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 ${TOPO_BELOW_CMB_FILE}
# Interface 3: CMB topography.
 .true.                    ${CMB_NXI} ${CMB_NETA} 0.d0       0.d0      ${CMB_DXI}    ${CMB_DETA}
 topo_CMB.dat
# Interface 2: auxiliary interface above the CMB, placed below the upper coupling boundary.
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 ${TOPO_ABOVE_CMB_FILE}
# Interface 1: Represents the top of the mesh
 .true.                    ${AUX_NXI} ${AUX_NETA} 0.d0       0.d0      ${AUX_DXI}    ${AUX_DETA}
 topo_top.dat
# Number of elements in the bottom layer
${NZ_BELOW_CMB_AUX}
# Number of elements between the lower auxiliary interface and the CMB
${NZ_AUX_TO_CMB}
# Number of elements between the CMB and the upper auxiliary interface
${NZ_CMB_TO_AUX}
# Number of elements in the top layer
${NZ_ABOVE_CMB_AUX_TOP}
EOF_INTERFACES
fi

replace_key "${MESHFEM_DIR}/Coupling_Par_file" R_TOP_BOUND "${R_TOP_BOUND}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" ANGULAR_WIDTH_XI_IN_DEGREES "${ANGULAR_WIDTH_XI}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" ANGULAR_WIDTH_ETA_IN_DEGREES "${ANGULAR_WIDTH_ETA}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" CENTER_LATITUDE_IN_DEGREES "${CENTER_LAT}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" CENTER_LONGITUDE_IN_DEGREES "${CENTER_LON}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" GAMMA_ROTATION_AZIMUTH "${GAMMA_ROTATION}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IXI_LOW "${COUPLING_IXI_LOW}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IXI_HIGH "${COUPLING_IXI_HIGH}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IETA_LOW "${COUPLING_IETA_LOW}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IETA_HIGH "${COUPLING_IETA_HIGH}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IR_TOP "${COUPLING_IR_TOP}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_IR_BOTTOM "${COUPLING_IR_BOTTOM}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" COUPLING_DIST_TOLERENCE "${COUPLING_DIST_TOLERENCE}"
replace_key "${MESHFEM_DIR}/Coupling_Par_file" LOW_RESOLUTION "${LOW_RESOLUTION}"

constant_topography "${Z_BELOW_CMB_AUX}" "${MESHFEM_DIR}/topo_top.dat" "${MESHFEM_DIR}/${TOPO_BELOW_CMB_FILE}"
if [[ "${DSM1D_OR_3D}" == "DSM3D" ]]; then
  convert_interface_topography 2 "DATA/meshfem3D_files/latlon_interface_topo.txt" "${Z_CMB}" "${MESHFEM_DIR}/topo_CMB.dat"
else
  echo "DSM1D_OR_3D=DSM1D: using flat CMB interface; skipping xcubedsphere_topo conversion."
  constant_topography_grid "${Z_CMB}" "${CMB_NXI}" "${CMB_NETA}" "${MESHFEM_DIR}/topo_CMB.dat"
fi
if [[ -n "${ELASTIC_DISCON_RADIUS_KM}" ]]; then
  constant_topography "${Z_ELASTIC_DISCON}" "${MESHFEM_DIR}/topo_top.dat" "${MESHFEM_DIR}/${TOPO_ELASTIC_DISCON_FILE}"
fi
constant_topography "${Z_ABOVE_CMB_AUX}" "${MESHFEM_DIR}/topo_top.dat" "${MESHFEM_DIR}/${TOPO_ABOVE_CMB_FILE}"

awk -v lat="${CENTER_LAT}" -v lon="${CENTER_LON}" 'BEGIN {
  printf "%-8s %-8s %12.6f %12.6f %8.3f %8.3f\n", "DE", "CENTER", lat, lon, 0.0, 0.0
}' > "${DATA_DIR}/STATIONS"

# Summary of preparation
echo "----------------------------------------------------------------------"
echo "Preparation Summary:"
summary_line "Prepared SPECFEM3D mesh/database inputs in ${DATA_DIR}"
summary_line "  MODEL=${DSM1D_OR_3D}"
summary_line "  NPROC=${NPROC}, NPROC_XI=${NPROC_XI}, NPROC_ETA=${NPROC_ETA}"
summary_line "  min_wave_speed=${MIN_WAVE_SPEED_KM_S} km/s, min_wavelength=${MIN_WAVELENGTH_KM} km"
summary_line "  COUPLING_DIST_TOLERENCE=${COUPLING_DIST_TOLERENCE} deg (${COUPLING_DIST_WAVELENGTH_FRACTION} wavelength)"
summary_line "  NEX_XI=${NEX_XI}, NEX_ETA=${NEX_ETA}, NZ_TOTAL=${NZ_TOTAL}"
echo "----------------------------------------------------------------------"
echo "Step 1: Preparation complete. Move to Step 2 or Step 3 (mesh/database generation)."
echo "----------------------------------------------------------------------"

cd "$ROOT_DIR"
