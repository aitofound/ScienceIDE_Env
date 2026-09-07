#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DSM_DIR="${DSM_DIR:-${ROOT_DIR}/../DSM/1D_tele_syn_Vs6.0IC/PKIKP}"
SPECTOTIME="${SPECTOTIME:-${ROOT_DIR}/../../../../src/DSM/src/DSM_FreqToTimeSac/spectotime}"
MPIRUN="${MPIRUN:-}"
NPROC="${NPROC:-1}"
SUBMIT="${SUBMIT:-0}"
SAC_SUBDIR="${SAC_SUBDIR:-disp_solid_sac}"

cd "${DSM_DIR}"

for required in dsm_model depth_solid_list dist_solid_list OUTPUT_FILES/disp_solid "${SPECTOTIME}"; do
    if [ ! -e "${required}" ]; then
        echo "Error: cannot find ${required}" >&2
        exit 1
    fi
done

nfreq_dsm=$(awk 'NR == 1 {print $2}' dsm_model)
time_length=$(awk 'NR == 1 {print $1}' dsm_model)
omega_imag=$(awk 'NR == 2 {print $1}' dsm_model)
source_depth=$(awk '
    NR == 4 {
        n_structure_zone = $1
        source_line = NR + 6 * n_structure_zone + 1
    }
    NR == source_line { print $1; exit }
' dsm_model)

ndep=$(awk 'NR == 2 {print $1}' depth_solid_list)
ndist=$(awk 'NR == 1 {print $1}' dist_solid_list)
nstation=$((ndep * ndist))

mkdir -p DATA "OUTPUT_FILES/${SAC_SUBDIR}"

cat > DATA/Par_file_freq2sac << EOF
${nstation} # Number of stations (ndep * ndist)
${nfreq_dsm} # Number of frequencies
${time_length} # Total time length in seconds
${source_depth} 0.0 0.0 # Source depth (km), source latitude, source longitude
3 # Number of components
${omega_imag} # Imaginary part of omega
"OUTPUT_FILES/disp_solid" # Frequency-domain input directory
"OUTPUT_FILES/${SAC_SUBDIR}" # SAC output directory
EOF

awk '
    NR == FNR {
        if (FNR > 2) {
            ndep++
            depth[ndep] = $1
            zone[ndep] = $2
        }
        next
    }
    FNR > 1 {
        ndist++
        dist[ndist] = $1
    }
    END {
        for (idep = 1; idep <= ndep; idep++) {
            for (idist = 1; idist <= ndist; idist++) {
                printf "L%d_dep%.2f dist%.2f %.2f %.1f\n",
                       zone[idep], depth[idep], dist[idist], dist[idist], 0.0
            }
        }
    }
' depth_solid_list dist_solid_list > DATA/station_list

cat > submit_freq2sac_disp_solid.cmd << EOF
#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --ntasks=${NPROC}
#SBATCH -J "PKIKP_1D_tosac"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger
#SBATCH --output=slurm.%N.%j.freq2sac.out
#SBATCH --error=slurm.%N.%j.freq2sac.err

${MPIRUN:+${MPIRUN} }${SPECTOTIME}
EOF

echo "Prepared ${DSM_DIR}/DATA/Par_file_freq2sac"
echo "Prepared ${DSM_DIR}/DATA/station_list with ${nstation} stations"
echo "SAC output directory: ${DSM_DIR}/OUTPUT_FILES/${SAC_SUBDIR}"

if [ "${SUBMIT}" -eq 1 ]; then
    sbatch submit_freq2sac_disp_solid.cmd
else
    if [ -n "${MPIRUN}" ]; then
        "${MPIRUN}" "${SPECTOTIME}"
    else
        "${SPECTOTIME}"
    fi
fi
