#!/bin/bash
rm -f job.pfe.new

# ============================================================
# Helper: abort on cancel
# ============================================================
cancel_check() {
    if [ $? -ne 0 ]; then
        echo "❌ Cancelled by user."
        exit 1
    fi
}

# ============================================================
# Interactive version function
# ============================================================
interactive_job() {
    # --- Machine Configuration ---
    declare -A models=(
      ["bro_ele"]=28
      ["cas_ait"]=40
      ["mil_ait"]=128
      ["rom_ait"]=128
      ["sky_ele"]=40
    )
    MODEL_ORDER=("bro_ele" "cas_ait" "mil_ait" "rom_ait" "sky_ele")

    # --- Detect previously selected models from old job.pfe ---
    declare -A previous_selected
    if [ -f job.pfe ]; then
        while read -r line; do
            if [[ "$line" =~ select=.*:model= ]]; then
                model=$(echo "$line" | sed -E 's/.*model=([^ :]+).*/\1/')
                previous_selected[$model]=1
            fi
        done < job.pfe
    fi

    # --- Job Name ---
    JOB_NAME=$(whiptail --inputbox "Enter job name (leave empty for SWMF):" 8 60 "" 3>&1 1>&2 2>&3)
    cancel_check
    [ -z "$JOB_NAME" ] && JOB_NAME="SWMF"

    # --- CPUs ---
    while true; do
        TARGET_CPUS=$(whiptail --inputbox "Enter total number of CPUs to request:" 8 60 "" 3>&1 1>&2 2>&3)
        cancel_check
        if [[ "$TARGET_CPUS" =~ ^[0-9]+$ ]] && [ "$TARGET_CPUS" -gt 0 ]; then
            break
        fi
        whiptail --msgbox "Please enter a valid positive integer for CPUs." 8 50
    done

    # --- Walltime ---
    WALLTIME_HOURS=$(whiptail --inputbox "Enter walltime (hours, default 8):" 8 60 "" 3>&1 1>&2 2>&3)
    cancel_check
    if ! [[ "$WALLTIME_HOURS" =~ ^[0-9]+$ ]] || [ -z "$WALLTIME_HOURS" ]; then
        WALLTIME_HOURS=8
    fi
    if [ "$WALLTIME_HOURS" -gt 8 ]; then
        QUEUE_LINE="#PBS -q long"
    else
        QUEUE_LINE="#PBS -q normal"
    fi

    # --- Build checklist items ---
    checklist_items=()
    for model in "${MODEL_ORDER[@]}"; do
        desc="${models[$model]} CPUs/node"
        if [ ${#previous_selected[@]} -eq 0 ]; then
            checklist_items+=("$model" "$desc" "ON")
        else
            if [ -n "${previous_selected[$model]}" ]; then
                checklist_items+=("$model" "$desc" "ON")
            else
                checklist_items+=("$model" "$desc" "OFF")
            fi
        fi
    done

    # --- Dynamic width and height ---
    max_len=0
    for model in "${MODEL_ORDER[@]}"; do
        desc="${model} ${models[$model]} CPUs/node"
        len=${#desc}
        (( len > max_len )) && max_len=$len
    done
    whiptail_width=$((max_len + 10))
    [ $whiptail_width -lt 40 ] && whiptail_width=40
    [ $whiptail_width -gt 100 ] && whiptail_width=100

    num_models=${#MODEL_ORDER[@]}
    whiptail_height=$((num_models + 5))

    # --- Show checklist dialog ---
    MODEL_SELECTION=$(whiptail --title "Select Models" --checklist \
    "Select which models to include (Space = toggle, Enter = confirm):" \
    $whiptail_height $whiptail_width $num_models \
    "${checklist_items[@]}" \
    3>&1 1>&2 2>&3)
    cancel_check
    MODEL_SELECTION=$(echo "$MODEL_SELECTION" | tr -d '"')

    # --- Generate PBS select lines ---
    declare -a pbs_lines_array
    is_first_active_line=true
    for model in ${MODEL_SELECTION}; do
        ncpus=${models[$model]}
        select_val=$(( (TARGET_CPUS + ncpus - 1) / ncpus ))
        line="-l select=${select_val}:ncpus=${ncpus}:model=${model}"
        if $is_first_active_line; then
            pbs_lines_array+=("#PBS ${line}")
            is_first_active_line=false
        else
            pbs_lines_array+=("### PBS ${line}")
        fi
    done
    PBS_SELECT_LINES=$(printf "%s\n" "${pbs_lines_array[@]}")

    # --- Write job.pfe ---
    cat << EOF > job.pfe
#!/bin/csh
########################################################
#PBS -S /bin/csh
#PBS -N ${JOB_NAME}
########################################################
${PBS_SELECT_LINES}
########################################################
${QUEUE_LINE}
#PBS -l walltime=${WALLTIME_HOURS}:00:00
#PBS -j oe
#PBS -m e
########################################################
cd \$PBS_O_WORKDIR
setenv MPI_TYPE_DEPTH 20
mpiexec ./SWMF.exe > runlog_\`date +%y%m%d%H%M\`
exit
if(! -f SWMF.SUCCESS) exit
if(-f SWMF.DONE) exit
./Restart.pl
qsub job.pfe
EOF

    # --- Submit job ---
    if [ -x ./qsub.pfe.pbspl.pl ]; then
        ./qsub.pfe.pbspl.pl job.pfe "${JOB_NAME}"
    fi

    whiptail --msgbox "✅ Job '${JOB_NAME}' submitted requesting ${TARGET_CPUS} CPUs (Walltime: ${WALLTIME_HOURS}h)" 8 70
}

# ============================================================
# Main script logic
# ============================================================

if [ "$#" -eq 0 ]; then
    # Run interactive GUI version
    interactive_job
    exit 0
fi

# --- Classic positional-argument version ---
if [ "$#" -lt 2 ] || ! [[ "$2" =~ ^[0-9]+$ ]]; then
    echo "❌ Error: Invalid arguments."
    echo "Usage: $0 <NAME> <CPUs> [options]"
    exit 1
fi

JOB_NAME=$1
TARGET_CPUS=$2
shift 2

WALLTIME_HOURS=8
QUEUE_LINE="#PBS -q normal"
declare -A EXCLUDE_MODELS

for arg in "$@"; do
  case $arg in
    -long)
      QUEUE_LINE="#PBS -q long"
      ;;
    -t=*)
      val="${arg#*=}"
      if [[ "$val" =~ ^[0-9]+$ ]]; then
        WALLTIME_HOURS="$val"
      fi
      ;;
    -no*)
      model_to_exclude="${arg#-no}"
      EXCLUDE_MODELS["$model_to_exclude"]=1
      ;;
  esac
done

 declare -A models=(
      ["bro_ele"]=28
      ["cas_ait"]=40
      ["mil_ait"]=128
      ["rom_ait"]=128
      ["sky_ele"]=40
    )
    MODEL_ORDER=("bro_ele" "cas_ait" "mil_ait" "rom_ait" "sky_ele")

declare -a pbs_lines_array
is_first_active_line=true

for model in "${MODEL_ORDER[@]}"; do
  if [[ -v EXCLUDE_MODELS[$model] ]]; then
    continue
  fi
  ncpus=${models[$model]}
  select_val=$(( (TARGET_CPUS + ncpus - 1) / ncpus ))
  line="-l select=${select_val}:ncpus=${ncpus}:model=${model}"
  if $is_first_active_line; then
    pbs_lines_array+=("#PBS ${line}")
    is_first_active_line=false
  else
    pbs_lines_array+=("### PBS ${line}")
  fi
done
PBS_SELECT_LINES=$(printf "%s\n" "${pbs_lines_array[@]}")

cat << EOF > job.pfe
#!/bin/csh
########################################################
#PBS -S /bin/csh
#PBS -N ${JOB_NAME}
########################################################
${PBS_SELECT_LINES}
########################################################
${QUEUE_LINE}
#PBS -l walltime=${WALLTIME_HOURS}:00:00
#PBS -j oe
#PBS -m e
########################################################
cd \$PBS_O_WORKDIR
setenv MPI_TYPE_DEPTH 20
mpiexec ./SWMF.exe > runlog_\`date +%y%m%d%H%M\`
exit
if(! -f SWMF.SUCCESS) exit
if(-f SWMF.DONE) exit
./Restart.pl
qsub job.pfe
EOF

./qsub.pfe.pbspl.pl job.pfe "${JOB_NAME}"
echo "✅ Requested ${TARGET_CPUS} CPUs on all queues"
