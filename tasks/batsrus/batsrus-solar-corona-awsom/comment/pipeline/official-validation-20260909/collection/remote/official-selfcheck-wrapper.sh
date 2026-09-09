#!/usr/bin/env bash
# Durable PR463 official selfcheck wrapper; additive validation artifact only.
# The terminal exit file is outside the CLI-created run root.
set -Eeuo pipefail
BASE=/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353
TOOLROOT="$BASE/toolroot"
PIPE="$BASE/pipeline"
RUNROOT="$BASE/runroot-selfcheck-20260909-em7353"
LOG="$BASE/logs/official-selfcheck.log"
LIVE="$BASE/official.live"
EXIT="$BASE/terminal.exit"
PIDFILE="$BASE/official.pid"
mkdir -p "$BASE/logs"
test ! -e "$RUNROOT"
test ! -e "$LIVE"
test ! -e "$EXIT"
exec >>"$LOG" 2>&1
printf 'OFFICIAL_WRAPPER_START=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'HOST=%s\n' "$(hostname)"
printf 'RUNROOT=%s (verified absent before CLI)\n' "$RUNROOT"
printf 'COMMAND=/usr/bin/python3 skills/package-sciaccel-task/scripts/sab.py task selfcheck --task tasks/batsrus/batsrus-solar-corona-awsom --run-root %s\n' "$RUNROOT"
cd "$TOOLROOT"
export PATH="/home/huangzesen/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export SAB_PIPE_DIR="$PIPE"
set +e
/usr/bin/python3 skills/package-sciaccel-task/scripts/sab.py task selfcheck --task tasks/batsrus/batsrus-solar-corona-awsom --run-root "$RUNROOT" &
cmd_pid=$!
printf '%s\n' "$cmd_pid" > "$PIDFILE"
sleep 1
if ps -p "$cmd_pid" -o pid=,ppid=,etime=,stat=,args=; then
  printf 'OFFICIAL_LIVE=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$LIVE"
  printf 'PID=%s\nRUNROOT=%s\nLOG=%s\n' "$cmd_pid" "$RUNROOT" "$LOG" >> "$LIVE"
fi
wait "$cmd_pid"
status=$?
set -e
printf '%s\n' "$status" > "$EXIT"
printf 'OFFICIAL_WRAPPER_END=%s EXIT=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$status"
exit "$status"
