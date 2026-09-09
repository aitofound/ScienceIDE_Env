#!/usr/bin/env bash
set -u
ROOT=/home/huangzesen/sab-pr450-validation-20260909-em-828e
CLI=$ROOT/skills/package-sciaccel-task/scripts/sab.py
TASK=$ROOT/tasks/batsrus/batsrus-multifluid-fivemoment
PIPE=$ROOT/pipeline
RUNROOT=$ROOT/runroot
LOG=$ROOT/official-supervisor-corrected.log
RECEIPT=$ROOT/official-terminal-exit-corrected.json
START_UTC=$(date -u +%Y-%m-%dT%H:%M:%SZ)
BUILD_CMD="python3 $CLI task build --task $TASK --which both"
SELFCHECK_CMD="python3 $CLI task selfcheck --task $TASK --run-root $RUNROOT"
terminal_written=0
build_rc=null
selfcheck_rc=null
final_rc=1
write_receipt() {
  rc=$1
  end_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  tmp="$RECEIPT.tmp.$$"
  printf '{"attempt":"authorized-original-science-attempt","started_utc":"%s","finished_utc":"%s","build_rc":%s,"selfcheck_rc":%s,"exit_code":%s,"task":"batsrus-multifluid-fivemoment","head":"9ada1287b9608b5402dafb01d37b7556f3b08df3","consent_where":"local","human_ref":"exact6874ref","run_root":"%s","log":"%s","build_command":"%s","selfcheck_command":"%s","pipeline":"%s"}\n' "$START_UTC" "$end_utc" "$build_rc" "$selfcheck_rc" "$rc" "$RUNROOT" "$LOG" "$BUILD_CMD" "$SELFCHECK_CMD" "$PIPE" > "$tmp"
  mv "$tmp" "$RECEIPT"
  terminal_written=1
}
trap 'rc=$?; if [ "$terminal_written" -eq 0 ]; then write_receipt "$rc"; fi; exit "$rc"' EXIT
printf 'SUPERVISOR_START %s\n' "$START_UTC"
printf 'ATTEMPT authorized-original-science-attempt\nTASK_HEAD 9ada1287b9608b5402dafb01d37b7556f3b08df3\nCONSENT where=local human_ref=exact6874ref\nRUN_ROOT %s\nLOG %s\nRECEIPT %s\nBUILD_COMMAND %s\nSELFCHECK_COMMAND %s\n' "$RUNROOT" "$LOG" "$RECEIPT" "$BUILD_CMD" "$SELFCHECK_CMD"
if [ -e "$RUNROOT" ]; then
  printf 'REFUSING_NONABSENT_RUN_ROOT %s\n' "$RUNROOT" >&2
  final_rc=2
  write_receipt "$final_rc"
  exit "$final_rc"
fi
PATH=/home/huangzesen/bin:$PATH SAB_PIPE_DIR="$PIPE" python3 "$CLI" task build --task "$TASK" --which both
build_rc=$?
printf 'BUILD_RC %s\n' "$build_rc"
if [ "$build_rc" -eq 0 ]; then
  PATH=/home/huangzesen/bin:$PATH SAB_PIPE_DIR="$PIPE" python3 "$CLI" task selfcheck --task "$TASK" --run-root "$RUNROOT"
  selfcheck_rc=$?
  printf 'SELFCHECK_RC %s\n' "$selfcheck_rc"
else
  printf 'SELFCHECK_NOT_STARTED_BUILD_FAILED\n'
fi
if [ "$build_rc" -eq 0 ] && [ "$selfcheck_rc" -eq 0 ]; then final_rc=0; else final_rc=1; fi
write_receipt "$final_rc"
exit "$final_rc"
