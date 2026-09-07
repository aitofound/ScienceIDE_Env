#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd -P)"
WORK_DIR="${WORK_DIR:-410km_triplication_mij_python}"
SPECTOTIME="${SPECTOTIME:-$ROOT/../../../../../src/DSM/src/DSM_FreqToTimeSac/spectotime}"
TIME_LIMIT="${TIME_LIMIT:-01:00:00}"
MEM_PER_CPU="${MEM_PER_CPU:-4G}"
NTASKS="${NTASKS:-1}"
MODULES="${MODULES:-}"
PARTITION="${PARTITION:-}"
QOS="${QOS:-}"

submit_ids=()

for comp in Mzz Mrr Mtt Mzr Mzt Mrt; do
  d="$ROOT/$WORK_DIR/$comp"
  comp_abs="$(cd "$d" && pwd -P)"
  submit_script="$comp_abs/submit_freq_to_time_${comp}.cmd"

  cp "$SPECTOTIME" "$comp_abs/"
  chmod +x "$comp_abs/spectotime"

  partition_line=""
  qos_line=""
  module_line=""
  if [ -n "$PARTITION" ]; then
    partition_line="#SBATCH --partition=$PARTITION"
  fi
  if [ -n "$QOS" ]; then
    qos_line="#SBATCH --qos=$QOS"
  fi
  if [ -n "$MODULES" ]; then
    module_line="module load $MODULES"
  fi

  cat > "$submit_script" <<JOB_EOF
#!/bin/bash
#SBATCH --time=$TIME_LIMIT
#SBATCH --ntasks=$NTASKS
#SBATCH --mem-per-cpu=$MEM_PER_CPU
#SBATCH -J "freq2sac_$comp"
#SBATCH --output=slurm-freq2sac.%j.out
#SBATCH --error=slurm-freq2sac.%j.err
$partition_line
$qos_line

set -euo pipefail
cd "$comp_abs"

$module_line

mkdir -p OUTPUT_FILES/disp_solid_time_sac
rm -f OUTPUT_FILES/disp_solid_time_sac/L*_dep*_dist*.bh?
rm -f OUTPUT_FILES/L*_dep*_dist*.bh?

awk '
  FNR == NR {
    if (FNR > 2) {
      dep[++nd] = \$1
      zone[nd] = \$2
    }
    next
  }
  FNR > 1 {
    dist[++nt] = \$1
  }
  END {
    for (i = 1; i <= nd; i++) {
      for (j = 1; j <= nt; j++) {
        printf "L%d_dep%.2f dist%.2f %.6f %.6f\\n", zone[i], dep[i], dist[j], -6.7 + dist[j], 0.0
      }
    }
  }
' DATA/depth_solid_list DATA/dist_solid_list > DATA/station_list

cat > DATA/Par_file_freq2sac <<PAR_EOF
6
4096
2048.0
100.0 0.0 -6.7
3
1.0e-3
PAR_EOF

for f in OUTPUT_FILES/disp_solid/freq_*; do
  ln -sfn "../OUTPUT_FILES/disp_solid/\$(basename "\$f")" "DATA/\$(basename "\$f")"
done

./spectotime
mv OUTPUT_FILES/L*_dep*_dist*.bh? OUTPUT_FILES/disp_solid_time_sac/
JOB_EOF

  job_id=$(sbatch --parsable "$submit_script")
  submit_ids+=("$job_id")
  echo "submitted $comp: $job_id"
done

printf 'submitted %d freq-to-time jobs: %s\n' "${#submit_ids[@]}" "${submit_ids[*]}"
