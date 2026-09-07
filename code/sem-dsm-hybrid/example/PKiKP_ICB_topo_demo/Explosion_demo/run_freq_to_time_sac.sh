#!/bin/bash

set -e

###################################### Start: Specify Input Parameters ######################################

# Run from this tensor directory, e.g. Mzz/.
dir_src="${dir_src:-../../../../../../src/DSM/src/DSM_FreqToTimeSac}"

NPROC="${NPROC:-1}"
modules="${modules:-}"
mpiruncmd="${mpiruncmd:-mpirun}"
submit_job="${submit_job:-1}"
write_submit_script="${write_submit_script:-1}"

# Leave empty to use the value in DATA/dsm_model.
nfrequency="${nfrequency:-}"

# Controls:
#   ncomp_seismogram=1: fluid_quantity=potential|pressure
#   ncomp_seismogram=3: media_type=solid with solid_quantity=disp|velo,
#                       or media_type=fluid with fluid_quantity=disp
#   ncomp_seismogram=6: stress in solid media
ncomp_seismogram="${ncomp_seismogram:-3}"
media_type="${media_type:-solid}"
solid_quantity="${solid_quantity:-disp}"
fluid_quantity="${fluid_quantity:-potential}"

###################################### End: Specify Input Parameters ########################################

dsm_model="DATA/dsm_model"

case "${ncomp_seismogram}" in
    1)
        media_type="fluid"
        case "${fluid_quantity}" in
            potential) freq_subdir="potential"; sac_subdir="potential_fluid_time_sac" ;;
            pressure)  freq_subdir="pressure";  sac_subdir="pressure_fluid_time_sac" ;;
            *) echo "Error: ncomp_seismogram=1 requires fluid_quantity=potential or pressure"; exit 1 ;;
        esac
        depth_list="DATA/depth_fluid_list"
        dist_list="DATA/dist_fluid_list"
        ;;
    3)
        case "${media_type}" in
            solid)
                case "${solid_quantity}" in
                    disp) freq_subdir="disp_solid"; sac_subdir="disp_solid_time_sac" ;;
                    velo) freq_subdir="velo_solid"; sac_subdir="velo_solid_time_sac" ;;
                    *) echo "Error: solid ncomp_seismogram=3 requires solid_quantity=disp or velo"; exit 1 ;;
                esac
                depth_list="DATA/depth_solid_list"
                dist_list="DATA/dist_solid_list"
                ;;
            fluid)
                if [ "${fluid_quantity}" != "disp" ]; then
                    echo "Error: fluid ncomp_seismogram=3 requires fluid_quantity=disp"
                    exit 1
                fi
                freq_subdir="disp_fluid"
                sac_subdir="disp_fluid_time_sac"
                depth_list="DATA/depth_fluid_list"
                dist_list="DATA/dist_fluid_list"
                ;;
            *) echo "Error: media_type must be solid or fluid"; exit 1 ;;
        esac
        ;;
    6)
        media_type="solid"
        freq_subdir="stress"
        sac_subdir="stress_solid_time_sac"
        depth_list="DATA/depth_solid_list"
        dist_list="DATA/dist_solid_list"
        ;;
    *) echo "Error: ncomp_seismogram must be 1, 3, or 6"; exit 1 ;;
esac

freq_dir="OUTPUT_FILES/${freq_subdir}"
sac_dir="OUTPUT_FILES/${sac_subdir}"

for required in "${dsm_model}" "${depth_list}" "${dist_list}" "${dir_src}/spectotime"; do
    if [ ! -e "${required}" ]; then
        echo "Error: cannot find ${required}"
        exit 1
    fi
done

if ! compgen -G "${freq_dir}/freq_*" > /dev/null; then
    echo "Error: no frequency files found in ${freq_dir}"
    exit 1
fi

nfreq_DSM=$(awk 'NR==1 {print $2}' "${dsm_model}")
if [ -z "${nfrequency}" ]; then
    nfrequency="${nfreq_DSM}"
fi
if [ "${nfrequency}" -gt "${nfreq_DSM}" ]; then
    echo "Error: nfrequency (${nfrequency}) exceeds nfreq_DSM (${nfreq_DSM})."
    exit 1
fi

ndep=$(awk 'NR==2 {print $1}' "${depth_list}")
ndist=$(awk 'NR==1 {print $1}' "${dist_list}")
if [ "${ndep}" -le 0 ] || [ "${ndist}" -le 0 ]; then
    echo "Error: no stations to convert from ${depth_list} and ${dist_list}"
    exit 1
fi
nstation=$((ndep * ndist))

time_length=$(awk 'NR==1 {print $1}' "${dsm_model}")
omega_imag=$(awk 'NR==2 {print $1}' "${dsm_model}")
source_depth=$(awk '
    NR == 4 {
        n_structure_zone = $1
        source_line = NR + 6 * n_structure_zone + 1
    }
    NR == source_line { print $1; exit }
' "${dsm_model}")

if [ -z "${source_depth}" ]; then
    echo "Error: failed to read source depth from ${dsm_model}"
    exit 1
fi

mkdir -p DATA "${sac_dir}"

cat << EOF > DATA/Par_file_freq2sac
${nstation} # Number of stations (ndep * ndist)
${nfrequency} # Number of frequencies
${time_length} # Total time length in seconds
${source_depth} 0.0 0.0 # Source depth (km), source latitude, source longitude
${ncomp_seismogram} # Number of components
${omega_imag} # Imaginary part of omega
"${freq_dir}" # Frequency-domain input directory
"${sac_dir}" # SAC output directory
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
                       zone[idep], depth[idep], dist[idist],
                       dist[idist], 0.0
            }
        }
    }
' "${depth_list}" "${dist_list}" > DATA/station_list

script_name="submit_ifft_save_sac_${sac_subdir}.cmd"
if [ "${write_submit_script}" -eq 1 ]; then
    cat << EOF > "${script_name}"
#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --ntasks=${NPROC}
#SBATCH -J "DSMsyn_tosac"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger
#SBATCH --output=slurm.%N.%j.out
#SBATCH --error=slurm.%N.%j.err

${mpiruncmd} ${dir_src}/spectotime
EOF

    if [ -n "${modules}" ]; then
        sed -i "/^${mpiruncmd//\//\\/} /i module load ${modules}" "${script_name}"
    fi
fi

echo "Prepared DATA/Par_file_freq2sac and DATA/station_list"
echo "  input spectra: ${freq_dir}/freq_*"
echo "  SAC output: ${sac_dir}"
echo "  ncomp=${ncomp_seismogram} nstation=${nstation} nfrequency=${nfrequency}"

if [ "${submit_job}" -eq 1 ]; then
    sbatch "${script_name}"
else
    echo "Run manually: ${mpiruncmd} ${dir_src}/spectotime"
fi
