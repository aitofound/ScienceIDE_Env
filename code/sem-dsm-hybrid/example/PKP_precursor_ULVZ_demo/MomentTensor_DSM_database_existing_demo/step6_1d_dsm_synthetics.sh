#!/usr/bin/env bash
#
# Step 6: Compute 1-D teleseismic synthetics using DSM.
# This script sets up and runs DSM for the selected source mode.

# Observed runtime estimate from completed Slurm logs for this case.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   dsm_1d_freq2sac: ntasks=1, runs=6, range=00:00:03-00:00:05, avg=00:00:04.
#   dsm_1d_green: ntasks=256, runs=6, range=00:16:36-00:36:41, avg=00:24:43.
#   dsm_1d_synthesis: ntasks=1, runs=1, runtime=00:00:00.
# End observed runtime estimate.

set -euo pipefail

# --- 1. Path Setup ---
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
WORK_DIR="${ROOT_DIR}/WORK/DSM/1D_tele_syn"
DSM_SOLVER_DIR="$(cd "${ROOT_DIR}/../../../src/DSM/src/DSM_Solver" && pwd)"
DSM_FREQ2SAC_DIR="$(cd "${ROOT_DIR}/../../../src/DSM/src/DSM_FreqToTimeSac" && pwd)"
CMTSOLUTION_FILE="${ROOT_DIR}/DATA/CMTSOLUTION"
STATION_FILE="${ROOT_DIR}/DATA/tele_station.txt"
FREQ2SAC_RUNNER="${ROOT_DIR}/run_freq_to_time_sac.sh"

echo "----------------------------------------------------------------------"
echo "Starting Step 6: 1-D Teleseismic Synthetics (DSM)"
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

resolve_case_path() {
  local path=$1
  if [[ -z "${path}" ]]; then
    return 1
  fi
  if [[ "${path}" = /* ]]; then
    printf '%s\n' "${path}"
  else
    printf '%s\n' "${ROOT_DIR}/${path}"
  fi
}

# --- 3. User Configuration (Overridable by Par_file_SEM_DSM) ---
# Number of processors used for DSM solver
NPROC=$(param DSM_NPROC $(param NPROC 256))
echo "1-D DSM synthetic computation typically takes about 3 hours using 512 cores."
# DSM solver memory per CPU
DSM_MEM_PER_CPU=$(param DSM_MEM_PER_CPU "4G")
# DSM solver time limit
DSM_TIME_LIMIT=$(param DSM_TIME_LIMIT "23:59:00")

# Resolved frequency (Hz)
FREQ_RESOLVED=$(param FREQ_RESOLVED 1.0)

# DSM solver parameters. TIME_LENGTH_DSM_1D overrides the automatic sum below.
TIME_LENGTH=$(param TIME_LENGTH_DSM_1D "")
TIME_LENGTH_SOURCE_TO_BOX=$(param TIME_LENGTH_DSM_SOURCE_TO_BOX "")
TIME_LENGTH_BOX_TO_RECEIVER=$(param TIME_LENGTH_DSM_BOX_TO_RECEIVER "")
N_FREQUENCY=$(param N_FREQUENCY_DSM_1D "")
OMEGA_IMAG=$(param OMEGA_IMAG_DSM_1D "")

TIME_LENGTH_1D_USER_SPECIFIED=1
if [[ -z "${TIME_LENGTH}" ]]; then
    TIME_LENGTH_1D_USER_SPECIFIED=0
fi

max_metadata_value() {
  local key=$1
  shift
  awk -v key="$key" '
    BEGIN { found=0; max=0.0 }
    FILENAME != lastfile { lastfile=FILENAME }
    $0 ~ "^" key "=" {
      v=$0
      sub("^" key "=", "", v)
      sub(/#.*/, "", v)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
      gsub(/[dD]/, "e", v)
      if (v ~ /^[-+]?([0-9]+[.]?[0-9]*|[.][0-9]+)([eE][-+]?[0-9]+)?$/) {
        if (!found || v + 0.0 > max) max = v + 0.0
        found = 1
      }
    }
    END { if (found) printf "%.6f\n", max }
  ' "$@"
}

time_length_1d_required_sum() {
  local src_metadata=()
  local recv_metadata=()
  local src_window="" recv_window="" required

  if [[ -f "${ROOT_DIR}/WORK/DSM/src_to_box/DATA/src_to_box_metadata.env" ]]; then
    src_metadata+=("${ROOT_DIR}/WORK/DSM/src_to_box/DATA/src_to_box_metadata.env")
  fi
  while IFS= read -r -d '' file; do
    src_metadata+=("$file")
  done < <(find "${ROOT_DIR}/WORK/DSM/src_to_box" -path '*/DATA/src_to_box_metadata.env' -print0 2>/dev/null || true)
  configured_src_dir=$(param DSM_SRC_TO_BOX_DIR "")
  if [[ -n "${configured_src_dir}" ]]; then
    configured_src_dir=$(resolve_case_path "${configured_src_dir}")
    while IFS= read -r -d '' file; do
      src_metadata+=("$file")
    done < <(find "${configured_src_dir}" -path '*/DATA/src_to_box_metadata.env' -print0 2>/dev/null || true)
  fi

  while IFS= read -r -d '' file; do
    recv_metadata+=("$file")
  done < <(find "${ROOT_DIR}/WORK/DSM/box_to_recv" -path '*/DATA/box_to_recv_metadata_*.env' -print0 2>/dev/null || true)

  if [[ ${#src_metadata[@]} -gt 0 ]]; then
    src_window=$(max_metadata_value TIME_SERIES_LENGTH "${src_metadata[@]}")
  fi
  if [[ ${#recv_metadata[@]} -gt 0 ]]; then
    recv_window=$(max_metadata_value TIME_SERIES_LENGTH "${recv_metadata[@]}")
  fi

  if [[ -z "${src_window}" ]]; then
    src_window="${TIME_LENGTH_SOURCE_TO_BOX}"
  fi
  if [[ -z "${recv_window}" ]]; then
    recv_window="${TIME_LENGTH_BOX_TO_RECEIVER}"
  fi

  if [[ -z "${src_window}" || -z "${recv_window}" ]]; then
    echo "Error: Cannot derive TIME_LENGTH_DSM_1D because source-to-box or box-to-receiver DSM time length is missing." >&2
    echo "Run steps 4 and 5 first, or set TIME_LENGTH_DSM_SOURCE_TO_BOX and TIME_LENGTH_DSM_BOX_TO_RECEIVER in ${PARAM_FILE}." >&2
    return 1
  fi

  required=$(awk -v src="${src_window}" -v recv="${recv_window}" 'BEGIN { printf "%.6f", src + recv }')
  printf "%s %s %s\n" "${required}" "${src_window}" "${recv_window}"
}

configure_time_length_1d() {
  local timing required src_window recv_window

  timing=$(time_length_1d_required_sum) || exit 1
  read -r required src_window recv_window <<< "${timing}"

  if [[ ${TIME_LENGTH_1D_USER_SPECIFIED} -eq 0 ]]; then
    TIME_LENGTH="${required}"
    echo "TIME_LENGTH_DSM_1D not set; using source-to-box + box-to-receiver DSM time lengths = ${TIME_LENGTH} s (${src_window} + ${recv_window})."
    return
  fi

  awk -v time_length="${TIME_LENGTH}" -v required="${required}" 'BEGIN { exit (time_length + 0.0 > required + 0.0) ? 0 : 1 }' || {
    echo "Error: TIME_LENGTH_DSM_1D=${TIME_LENGTH} s must be larger than source-to-box + box-to-receiver DSM time lengths: ${required} s (${src_window} + ${recv_window})."
    echo "Increase TIME_LENGTH_DSM_1D in ${PARAM_FILE}, or leave it unset so Step 6 can derive the sum automatically."
    exit 1
  }
  echo "Verified TIME_LENGTH_DSM_1D=${TIME_LENGTH} s > source-to-box + box-to-receiver DSM time lengths = ${required} s (${src_window} + ${recv_window})."
}

configure_time_length_1d


# Parameters for defining depths in each layer (zone).
# In this case, only one depth (station at free surface) is set.
NDEP_TOP_LAYER1=1
ID_ZONE_LAYER1=""
TOP_DEP_LAYER1=0.0
BOT_DEP_LAYER1=1.0

DEPTH_TOLERANCE=0.0025
DDEPTH_FOR_STRESS=0.01

# save displacement (1) or velocity (2) seismograms
SAVE_VELO=1

# source type: 1 for moment tensor, 2 for single force.
MOMENT_OR_FORCE=1
SOURCE_MODE=$(param SINGLE_FORCE_ENZ -1)

# Frequency-to-Time SAC Conversion
RUN_FREQ_TO_TIME_SAC=1
FREQ2SAC_NPROC=$(param freq2sac_NPROC 1)
FREQ2SAC_TIME_ESTIMATE=$(param freq2sac_time_estimate "01:00:00")
FREQ2SAC_MEM_PER_CPU=$(param freq2sac_mem_per_cpu "4G")
SLURM_PARTITION=$(param SLURM_PARTITION "")
SLURM_QOS=$(param SLURM_QOS "")
SBATCH_PARTITION_DIRECTIVE=""
SBATCH_QOS_DIRECTIVE=""
if [[ -n "${SLURM_PARTITION}" ]]; then
    SBATCH_PARTITION_DIRECTIVE="#SBATCH --partition=${SLURM_PARTITION}"
fi
if [[ -n "${SLURM_QOS}" ]]; then
    SBATCH_QOS_DIRECTIVE="#SBATCH --qos=${SLURM_QOS}"
fi

# Media and quantity for conversion
FREQ2SAC_MEDIA_TYPE="solid"
FREQ2SAC_SOLID_QUANTITY="disp"
FREQ2SAC_NCOMP=3

# HPC settings
MPIRUNCMD="mpirun"
MODULES=""

# --- 4. Validation ---
if [[ ! -f "${CMTSOLUTION_FILE}" ]]; then
    echo "Error: CMTSOLUTION file not found at ${CMTSOLUTION_FILE}"
    exit 1
fi

if [[ ! -f "${STATION_FILE}" ]]; then
    echo "Error: Station file not found at ${STATION_FILE}"
    exit 1
fi

if [[ "${RUN_FREQ_TO_TIME_SAC}" -eq 1 && ! -f "${FREQ2SAC_RUNNER}" ]]; then
    echo "Error: Frequency-to-SAC runner not found at ${FREQ2SAC_RUNNER}"
    exit 1
fi

# Determine the DSM model file to use for parameter extraction
DSM_MODEL_FOR_PARAMS="${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input"
if [[ ! -f "${DSM_MODEL_FOR_PARAMS}" ]]; then
    echo "Warning: WORK/SPECFEM3D/DATA/dsm_model_input not found, using DATA/dsm_model_base for parameter detection"
    DSM_MODEL_FOR_PARAMS="${ROOT_DIR}/DATA/dsm_model_base"
fi

if [[ ! -f "${DSM_MODEL_FOR_PARAMS}" ]]; then
    echo "Error: Could not find any DSM model file to detect parameters."
    exit 1
fi

# Automatically detect ID_ZONE_LAYER1 (the top zone) from the model file
# Line 4 contains n_structure_zone
AUTO_ID_ZONE=$(awk 'NR==4 {print $1}' "${DSM_MODEL_FOR_PARAMS}")
ID_ZONE_LAYER1=$(param ID_ZONE_LAYER1 "${AUTO_ID_ZONE}")

echo "Detected ID_ZONE_LAYER1: ${ID_ZONE_LAYER1} (from $(basename "${DSM_MODEL_FOR_PARAMS}"))"

# Load source parameters from CMTSOLUTION
source_depth_km=$(awk '$1 == "depth:" {print $2}' "${CMTSOLUTION_FILE}")
source_lat=$(awk '$1 == "latitude:" {print $2}' "${CMTSOLUTION_FILE}")
source_lon=$(awk '$1 == "longitude:" {print $2}' "${CMTSOLUTION_FILE}")

if [[ -z "${source_depth_km}" || -z "${source_lat}" || -z "${source_lon}" ]]; then
    echo "Error: Failed to read source parameters from ${CMTSOLUTION_FILE}"
    exit 1
fi


explosion_moment_vector() {
  awk '
    BEGIN {
      keys[1]="Mrr"; keys[2]="Mtt"; keys[3]="Mpp"; keys[4]="Mrt"; keys[5]="Mrp"; keys[6]="Mtp"
    }
    /^[[:space:]]*(Mrr|Mtt|Mpp|Mrt|Mrp|Mtp):/ {
      key=$1; sub(":", "", key); val=$2 + 0.0; m[key]=val
      if (val < 0) abs=-val; else abs=val
      if (abs > maxabs) maxabs=abs
    }
    END {
      for (i=1; i<=6; i++) if (!(keys[i] in m)) { printf "missing %s in CMTSOLUTION\n", keys[i] > "/dev/stderr"; exit 1 }
      if (maxabs == 0.0) { print "CMTSOLUTION moment tensor is zero" > "/dev/stderr"; exit 1 }
      tol = 1.0e-6 * maxabs
      if ((m["Mrr"] - m["Mtt"] > tol) || (m["Mtt"] - m["Mrr"] > tol) || \
          (m["Mtt"] - m["Mpp"] > tol) || (m["Mpp"] - m["Mtt"] > tol) || \
          (m["Mrt"] > tol) || (-m["Mrt"] > tol) || \
          (m["Mrp"] > tol) || (-m["Mrp"] > tol) || \
          (m["Mtp"] > tol) || (-m["Mtp"] > tol)) {
        print "SINGLE_FORCE_ENZ=-1 requires an explosion source: Mrr, Mtt, Mpp equal and off-diagonal terms zero" > "/dev/stderr"
        exit 1
      }
      exponent = int(log(maxabs)/log(10.0))
      if (10.0^exponent > maxabs) exponent--
      scale = 10.0^exponent
      printf "%d", exponent
      for (i=1; i<=6; i++) printf " %.12g", m[keys[i]] / scale
      printf "\n"
    }
  ' "${CMTSOLUTION_FILE}"
}

case "${SOURCE_MODE}" in
    -1)
        EXPLOSION_MOMENT_VECTOR=$(explosion_moment_vector)
        ;;
    0)
        EXPLOSION_MOMENT_VECTOR=""
        ;;
    *)
        echo "Error: Step 6 supports SINGLE_FORCE_ENZ=-1 explosion or 0 moment-tensor Green functions; got ${SOURCE_MODE}."
        exit 1
        ;;
esac

echo "Source parameters from CMTSOLUTION:"
echo "  Depth: ${source_depth_km} km"
echo "  Lat:   ${source_lat}"
echo "  Lon:   ${source_lon}"
echo "  Mode:  SINGLE_FORCE_ENZ=${SOURCE_MODE}"

# --- 5. Execution ---
mkdir -p "${WORK_DIR}"
cd "${WORK_DIR}"

# Collect freq2sac job IDs for the final synthesis dependency
freq2sac_ids=()

setup_and_run_job() {
    local tensor_dir=$1
    local force_vector=$2
    local job_name=$3

    echo "Setting up ${tensor_dir} ..."
    mkdir -p "${tensor_dir}/DATA"
    cd "${tensor_dir}"

    # Copy solver and runner
    cp "${DSM_SOLVER_DIR}/dsmti" .
    if [[ "${RUN_FREQ_TO_TIME_SAC}" -eq 1 ]]; then
        cp "${FREQ2SAC_RUNNER}" .
        chmod +x "$(basename "${FREQ2SAC_RUNNER}")"
    fi

    # Create output directories
    mkdir -p OUTPUT_FILES/{coef_cAnddcdr,disp_fluid,disp_solid,stress,potential,pressure,velo_solid}

    # Copy input data
    cp "${STATION_FILE}" DATA/tele_station.txt
    # Use the dsm_model_input prepared in Step 1
    if [[ -f "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input" ]]; then
        cp "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input" DATA/
    else
        echo "Warning: WORK/SPECFEM3D/DATA/dsm_model_input not found, using DATA/dsm_model_base"
        cp "${ROOT_DIR}/DATA/dsm_model_base" DATA/dsm_model_input
    fi

    # Prepare station distances
    echo "    Calculating station distances..."
    awk -v slat="${source_lat}" -v slon="${source_lon}" '
    BEGIN {
      pi = atan2(0.0,-1.0)
      deg2rad = pi / 180.0
      rad2deg = 180.0 / pi
      lat1 = slat * deg2rad
    }
    /^[[:space:]]*#/ || NF < 4 { next }
    {
      lat2 = $4 * deg2rad
      dlon = ($3 - slon) * deg2rad
      cosdel = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(dlon)
      if (cosdel > 1.0) cosdel = 1.0
      if (cosdel < -1.0) cosdel = -1.0
      dist = atan2(sqrt(1.0 - cosdel * cosdel), cosdel) * rad2deg
      printf("%16.8f %-8s %-8s %14.6f %14.6f\n",
             dist, $1, $2, $3, $4)
    }' DATA/tele_station.txt | sort -n > DATA/station_distances.tmp

    awk '{
      dist[NR] = $1
      line[NR] = sprintf("%-8s %-8s %14.6f %14.6f %16.8f",
                         $2, $3, $4, $5, $1)
    }
    END {
      print NR > "DATA/dist_solid_list"
      close("DATA/dist_solid_list")
      system("rm -f DATA/station_distances.txt")
      for (i = 1; i <= NR; i++) {
        printf("%16.8f\n", dist[i]) >> "DATA/dist_solid_list"
        print line[i] >> "DATA/station_distances.txt"
      }
    }' DATA/station_distances.tmp
    rm -f DATA/station_distances.tmp
    cp DATA/dist_solid_list DATA/dist_fluid_list

    # Prepare depth list
    echo "    Preparing depth lists..."
    echo "${DDEPTH_FOR_STRESS} ${DEPTH_TOLERANCE}" > DATA/depth_solid_list
    echo "${NDEP_TOP_LAYER1}" >> DATA/depth_solid_list
    echo "${NDEP_TOP_LAYER1} ${TOP_DEP_LAYER1} ${BOT_DEP_LAYER1} ${ID_ZONE_LAYER1}" | awk '{
      if ($1 <= 1) {
        printf("%10.4f %d\n", $2, $4)
      } else {
        ddep=($3-$2)/($1-1.0)
        for (idep=1; idep<=$1; idep++) {
          if (idep==$1) printf("%10.4f %d\n", $3, $4)
          else printf("%10.4f %d\n", $2+(idep-1)*ddep, $4)
        }
      }
    }' >> DATA/depth_solid_list

    echo "${DDEPTH_FOR_STRESS} ${DEPTH_TOLERANCE}" > DATA/depth_fluid_list
    echo 0 >> DATA/depth_fluid_list

    # Determine TIME_LENGTH and N_FREQUENCY
    local job_time_length="${TIME_LENGTH}"
    local job_n_frequency="${N_FREQUENCY}"
    if [[ -z "${job_n_frequency}" ]]; then
        job_n_frequency=$(awk -v t="${job_time_length}" -v f="${FREQ_RESOLVED}" 'BEGIN {
          n = 1
          while (n / t <= f) { n *= 2 }
          print n
        }')
    fi

    # Modify dsm_model_input
    echo "    Modifying dsm_model (N_FREQUENCY=${job_n_frequency})..."
    awk -v time_length="${job_time_length}" \
        -v n_frequency="${job_n_frequency}" \
        -v omega_imag="${OMEGA_IMAG}" \
        -v source_depth_km="${source_depth_km}" \
        -v source_lat="${source_lat}" \
        -v source_lon="${source_lon}" \
        -v moment_or_force="${MOMENT_OR_FORCE}" \
        -v save_velo="${SAVE_VELO}" \
        -v force="${force_vector}" '
        {
        if (NR == 1) {
            print time_length " " n_frequency "  #time_series_length n_freqnency"
        } else if (NR == 2) {
            if (omega_imag == "") omega_imag = $1
            print omega_imag "         #omega_imag";
        } else if (NR == 3) {
            print $0
        } else if (NR == 4) {
            n_structure_zone = $1
            post_structure_line = NR + 6 * n_structure_zone
            print $0
        } else if (post_structure_line > 0 && NR <= post_structure_line) {
            print $0
        } else if (NR == post_structure_line + 1) {
            print source_depth_km " " source_lat " " source_lon " " moment_or_force " #source_depth (km) source_lat source_lon source_type";
        } else if (NR == post_structure_line + 2) {
            print force " #exp_dyn-cm Mij,i=1-6 (moment tensor) OR exp_dyn Force_i,i=1,6 (only first three forces functional)";
        } else if (NR == post_structure_line + 3) {
            print "\"DATA/depth_solid_list\" #file listing the Greens function depths in solid media";
        } else if (NR == post_structure_line + 4) {
            print "\"DATA/dist_solid_list\" #file listing the Greens function distances in solid media";
        } else if (NR == post_structure_line + 5) {
            print "\"DATA/depth_fluid_list\" #file listing the Greens function depths in fluid media";
        } else if (NR == post_structure_line + 6) {
            print "\"DATA/dist_fluid_list\" #file listing the Greens function distances in fluid media";
        } else if (NR == post_structure_line + 7) {
            print save_velo " #save displacement (choose 1) or velocity (choose 2) seismograms in solid media";
        } else {
            print $0;
        }
    }' DATA/dsm_model_input > DATA/dsm_model

    # Submit DSM job
    cat << EOF > submit_DSM_${tensor_dir}.cmd
#!/bin/bash
#SBATCH --time=${DSM_TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${DSM_MEM_PER_CPU}
#SBATCH -J "DSM_${tensor_dir}"
${SBATCH_PARTITION_DIRECTIVE}
${SBATCH_QOS_DIRECTIVE}

job_start_epoch=\$(date +%s)
echo "Job started at: \$(date '+%Y-%m-%d %H:%M:%S %Z')"
print_job_runtime() {
    local job_status=\$?
    local job_end_epoch=\$(date +%s)
    local job_elapsed=\$((job_end_epoch-job_start_epoch))
    printf 'Job finished at: %s\n' "\$(date '+%Y-%m-%d %H:%M:%S %Z')"
    printf 'Job runtime: %02d:%02d:%02d (%d seconds)\n' \$((job_elapsed/3600)) \$(((job_elapsed%3600)/60)) \$((job_elapsed%60)) "\${job_elapsed}"
    printf 'Job exit status: %d\n' "\${job_status}"
}
trap print_job_runtime EXIT

${MPIRUNCMD} ./dsmti < DATA/dsm_model
EOF

    dsm_jobid=$(sbatch --parsable submit_DSM_${tensor_dir}.cmd)
    echo "    Submitted DSM job: ${dsm_jobid}"

    if [[ "${RUN_FREQ_TO_TIME_SAC}" -eq 1 ]]; then
        cat << EOF > submit_freq2sac_${tensor_dir}.cmd
#!/bin/bash
#SBATCH --time=${FREQ2SAC_TIME_ESTIMATE}
#SBATCH --ntasks=${FREQ2SAC_NPROC}
#SBATCH --mem-per-cpu=${FREQ2SAC_MEM_PER_CPU}
#SBATCH -J "f2s_${tensor_dir}"
#SBATCH --dependency=afterok:${dsm_jobid}
${SBATCH_PARTITION_DIRECTIVE}
${SBATCH_QOS_DIRECTIVE}

job_start_epoch=\$(date +%s)
echo "Job started at: \$(date '+%Y-%m-%d %H:%M:%S %Z')"
print_job_runtime() {
    local job_status=\$?
    local job_end_epoch=\$(date +%s)
    local job_elapsed=\$((job_end_epoch-job_start_epoch))
    printf 'Job finished at: %s\n' "\$(date '+%Y-%m-%d %H:%M:%S %Z')"
    printf 'Job runtime: %02d:%02d:%02d (%d seconds)\n' \$((job_elapsed/3600)) \$(((job_elapsed%3600)/60)) \$((job_elapsed%60)) "\${job_elapsed}"
    printf 'Job exit status: %d\n' "\${job_status}"
}
trap print_job_runtime EXIT

submit_job=0 \\
write_submit_script=0 \\
NPROC=${FREQ2SAC_NPROC} \\
dir_src="${DSM_FREQ2SAC_DIR}" \\
mpiruncmd="${MPIRUNCMD}" \\
ncomp_seismogram=${FREQ2SAC_NCOMP} \\
media_type="${FREQ2SAC_MEDIA_TYPE}" \\
solid_quantity="${FREQ2SAC_SOLID_QUANTITY}" \\
./$(basename "${FREQ2SAC_RUNNER}")

${MPIRUNCMD} ${DSM_FREQ2SAC_DIR}/spectotime
EOF
        freq2sac_jobid=$(sbatch --parsable submit_freq2sac_${tensor_dir}.cmd)
        freq2sac_ids+=("${freq2sac_jobid}")
        echo "    Submitted freq-to-SAC job: ${freq2sac_jobid}"
    fi

    cd ..
}

# Run only the components required by the selected source mode.
if [[ "${SOURCE_MODE}" == "-1" ]]; then
    setup_and_run_job "explosion" "${EXPLOSION_MOMENT_VECTOR}" "DSM_explosion"
else
    setup_and_run_job "Mzz" "7 1.0 0.0 0.0 0.0 0.0 0.0" "DSM_Mzz"
    setup_and_run_job "Mrr" "7 0.0 1.0 0.0 0.0 0.0 0.0" "DSM_Mrr"
    setup_and_run_job "Mtt" "7 0.0 0.0 1.0 0.0 0.0 0.0" "DSM_Mtt"
    setup_and_run_job "Mzr" "7 0.0 0.0 0.0 1.0 0.0 0.0" "DSM_Mzr"
    setup_and_run_job "Mzt" "7 0.0 0.0 0.0 0.0 1.0 0.0" "DSM_Mzt"
    setup_and_run_job "Mrt" "7 0.0 0.0 0.0 0.0 0.0 1.0" "DSM_Mrt"
fi

# Submit final synthesis job
if [[ "${RUN_FREQ_TO_TIME_SAC}" -eq 1 ]]; then
    echo "Submitting final synthesis job..."
    dep_list=$(IFS=:; echo "${freq2sac_ids[*]}")
    SYNTHESIS_REFERENCE_COMPONENT="Mrr"
    if [[ "${SOURCE_MODE}" == "-1" ]]; then
        SYNTHESIS_REFERENCE_COMPONENT="explosion"
    fi
    
    cat << EOF > submit_synthesis.cmd
#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --ntasks=1
#SBATCH -J "DSM_syn"
#SBATCH --dependency=afterok:${dep_list}
${SBATCH_PARTITION_DIRECTIVE}
${SBATCH_QOS_DIRECTIVE}

job_start_epoch=\$(date +%s)
echo "Job started at: \$(date '+%Y-%m-%d %H:%M:%S %Z')"
print_job_runtime() {
    local job_status=\$?
    local job_end_epoch=\$(date +%s)
    local job_elapsed=\$((job_end_epoch-job_start_epoch))
    printf 'Job finished at: %s\n' "\$(date '+%Y-%m-%d %H:%M:%S %Z')"
    printf 'Job runtime: %02d:%02d:%02d (%d seconds)\n' \$((job_elapsed/3600)) \$(((job_elapsed%3600)/60)) \$((job_elapsed%60)) "\${job_elapsed}"
    printf 'Job exit status: %d\n' "\${job_status}"
}
trap print_job_runtime EXIT

# Run from the demo root; the helper reads DSM component files from WORK/DSM/1D_tele_syn.
cd ${ROOT_DIR}
python3 auxiliary/synthesize_cmtsolution_sac.py --input-subdir "${FREQ2SAC_SOLID_QUANTITY}_solid_time_sac" --clean --reference-component "${SYNTHESIS_REFERENCE_COMPONENT}"
EOF
    syn_jobid=$(sbatch --parsable submit_synthesis.cmd)
    echo "    Submitted synthesis job: ${syn_jobid} (depends on: ${dep_list})"
fi

echo "All jobs submitted."
