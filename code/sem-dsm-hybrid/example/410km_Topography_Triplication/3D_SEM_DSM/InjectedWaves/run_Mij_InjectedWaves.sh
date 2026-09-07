#!/bin/bash

# Build injected-wave tables for the six moment-tensor basis components as
# six independent Slurm jobs: Mzz, Mrr, Mtt, Mzr, Mzt, and Mrt.
#
# This script does not read CMTSOLUTION or combine moment components. The
# Green's functions in ../DSM/src_to_box/<Mij>/ are assumed to have already
# been computed for each Mij basis source.

set -euo pipefail

echo
echo "Preparing six injected-wave jobs for moment-tensor basis components: $(date)"

###################################### User parameters ######################################

dir_SEM="../SPECFEM3D/OUTPUT_FILES/DATABASES_MPI/"
dir_DSM="../DSM/src_to_box/"
dir_Injection_src="../../../../src/InjectedWaves/src/"
dir_OUTPUT="OUTPUT_FILES"
dir_DATA="DATA"
dir_BUILD="BUILD_component_executables"

# MPI / scheduler settings for this case.
modules=""
mpiruncmd="${MPIRUN_CMD:-mpirun}"
NPROC="${NPROC:-262}"

# 1 = single force, 2 = moment tensor, 3 = explosion.
source_type=2

# Ignored for source_type=2, but the Fortran reader always expects this line.
declare -a SINGLE_FORCE_ENZ=("0" "0" "0")

# Number of frequencies used from the DSM Green's-function database.
# Must be <= nfreq in ${dir_DSM}/Mzz/OUTPUT_FILES/DSM_Par_file.
nfreq_inject="${NFREQ_INJECT:-4096}"

# Time window to save in the injected-wave tables.
time_start="${TIME_START:-35.0}"
time_end="${TIME_END:-200.0}"

# Number of time samples saved in one package.
npt_one_package="${NPT_ONE_PACKAGE:-600}"

# Optional frequency-domain taper for DSM Green's functions.
filtering="${FILTERING:-0}"
freq_low="${FREQ_LOW:-0.005}"
freq_high="${FREQ_HIGH:-1.0}"

# Set SUBMIT=0 to only prepare files and print the run commands.
SUBMIT="${SUBMIT:-1}"

moment_sources=(Mzz Mrr Mtt Mzr Mzt Mrt)

###################################### Validation ###########################################

for path in "${dir_SEM}" "${dir_DSM}" "${dir_Injection_src}"; do
  if [[ ! -e "${path}" ]]; then
    echo "ERROR: required path does not exist: ${path}" >&2
    exit 1
  fi
done

if [[ ! -x "${dir_Injection_src}/extract_Injectedwaves" ]]; then
  echo "ERROR: missing executable: ${dir_Injection_src}/extract_Injectedwaves" >&2
  exit 1
fi

if ! command -v make >/dev/null 2>&1; then
  echo "ERROR: make is required to build component-specific executables" >&2
  exit 1
fi

for src in "${moment_sources[@]}"; do
  if [[ ! -f "${dir_DSM}/${src}/OUTPUT_FILES/DSM_Par_file" ]]; then
    echo "ERROR: missing DSM Green's-function setup for ${src}: ${dir_DSM}/${src}/OUTPUT_FILES/DSM_Par_file" >&2
    exit 1
  fi
done

nfreq_dsm="$(awk 'NR == 1 {print $1; exit}' "${dir_DSM}/Mzz/OUTPUT_FILES/DSM_Par_file")"
if (( nfreq_inject > nfreq_dsm )); then
  echo "ERROR: NFREQ_INJECT=${nfreq_inject} is larger than DSM nfreq=${nfreq_dsm}" >&2
  exit 1
fi

###################################### Prepare files ########################################

echo "    (1) Building component-specific extract_Injectedwaves executables"
mkdir -p "${dir_BUILD}"
for src in "${moment_sources[@]}"; do
  build_src_dir="${dir_BUILD}/${src}"
  mkdir -p "${build_src_dir}"
  cp "${dir_Injection_src}"/*.f90 "${dir_Injection_src}"/Makefile "${build_src_dir}/"

  awk -v selected_src="${src}" '
    BEGIN {in_moment_block = 0}
    /else if\(source_type\.eq\.2\) then/ {
      print
      print "     ncomp_source=1"
      print "     allocate(sources_selected(ncomp_source))"
      print "     sources_selected(1)=\047" selected_src "\047"
      in_moment_block = 1
      next
    }
    in_moment_block && /else if\(source_type\.eq\.3\) then/ {
      in_moment_block = 0
      print
      next
    }
    !in_moment_block {print}
  ' "${dir_Injection_src}/read_input.f90" > "${build_src_dir}/read_input.f90"

  make -C "${build_src_dir}" clean >/dev/null
  make -C "${build_src_dir}" >/dev/null
  cp "${build_src_dir}/extract_Injectedwaves" "./extract_Injectedwaves_${src}"
done

echo "    (2) Creating output folders"
mkdir -p "${dir_OUTPUT}" "${dir_DATA}"
for src in "${moment_sources[@]}"; do
  mkdir -p \
    "${dir_OUTPUT}/${src}/velo_solid" \
    "${dir_OUTPUT}/${src}/stress" \
    "${dir_OUTPUT}/${src}/disp_fluid" \
    "${dir_OUTPUT}/${src}/pressure" \
    "${dir_OUTPUT}/${src}/chi_dot"
done

echo "    (3) Writing ${dir_DATA}/Par_file"
{
  echo "${source_type}"
  echo "${SINGLE_FORCE_ENZ[@]}"
  echo "${dir_DSM}"
  echo "${dir_SEM}"
  echo "${nfreq_inject}"
  echo "${time_start} ${time_end}"
  echo "${npt_one_package}"
  echo "${filtering}"
  echo "${freq_low} ${freq_high}"
} > "${dir_DATA}/Par_file"

echo "    (4) Writing one scavenger submit script per moment component"
for src in "${moment_sources[@]}"; do
  cat > "submit_InjectedWaves_${src}.cmd" <<EOS
#!/bin/bash
#SBATCH --time=05:00:00
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=8G
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger
#SBATCH -J 410km_${src}_Inject
#SBATCH -o OUTPUT_FILES/${src}/slurm-%j.out
#SBATCH -e OUTPUT_FILES/${src}/slurm-%j.out

set -e
if [[ -n "${modules}" ]]; then
  module load ${modules}
fi
${mpiruncmd} ./extract_Injectedwaves_${src}
EOS
done

###################################### Run ##################################################

if [[ "${SUBMIT}" == "1" ]]; then
  echo "    (5) Submitting six injected-wave extraction jobs"
  for src in "${moment_sources[@]}"; do
    sbatch "submit_InjectedWaves_${src}.cmd"
  done
else
  echo "Prepared injected-wave inputs. To run without Slurm:"
  for src in "${moment_sources[@]}"; do
    echo "  ${mpiruncmd} -np ${NPROC} ./extract_Injectedwaves_${src}"
  done
  echo "Or submit with:"
  for src in "${moment_sources[@]}"; do
    echo "  sbatch submit_InjectedWaves_${src}.cmd"
  done
fi
