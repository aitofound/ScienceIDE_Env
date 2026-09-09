#!/usr/bin/env bash
# Durable PR463 build wrapper; additive validation artifact, not repository source.
set -Eeuo pipefail
BASE=/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353
TOOLROOT="$BASE/toolroot"
PIPE="$BASE/pipeline"
LOG="$BASE/logs/task-build.log"
EXIT="$BASE/build.exit"
mkdir -p "$BASE/logs" "$PIPE"
exec >>"$LOG" 2>&1
printf 'BUILD_WRAPPER_START=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'HOST=%s\n' "$(hostname)"
printf 'COMMAND=env PATH=/home/huangzesen/bin:$PATH SAB_PIPE_DIR=%s /usr/bin/python3 skills/package-sciaccel-task/scripts/sab.py task build --task tasks/batsrus/batsrus-solar-corona-awsom --which both\n' "$PIPE"
cd "$TOOLROOT"
export PATH="/home/huangzesen/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export SAB_PIPE_DIR="$PIPE"
set +e
/usr/bin/python3 skills/package-sciaccel-task/scripts/sab.py task build --task tasks/batsrus/batsrus-solar-corona-awsom --which both
status=$?
set -e
printf '%s\n' "$status" > "$EXIT"
printf 'BUILD_WRAPPER_END=%s EXIT=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$status"
exit "$status"
