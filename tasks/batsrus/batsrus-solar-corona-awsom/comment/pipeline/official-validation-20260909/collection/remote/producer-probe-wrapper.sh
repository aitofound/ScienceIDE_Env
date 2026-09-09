#!/usr/bin/env bash
# Additive no-science producer probe: actual awsom run.sh, invalid zero-session
# fixture stops before BATSRUS execution while exercising copy/configure/BINDIR/cache.
set -Eeuo pipefail
BASE=/mnt/ssd/huangzesen/sab-runs/pr463-validation-20260909-em7353
PROBE="$BASE/producer-probe"
LOG="$BASE/logs/producer-probe.log"
EXIT="$BASE/producer-probe.exit"
mkdir -p "$BASE/logs" "$PROBE"
exec >>"$LOG" 2>&1
printf 'PROBE_START=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'HOST=%s\n' "$(hostname)"
printf 'IMAGE=%s\n' "sciaccel-batsrus-solar-corona-awsom-env"
printf 'FIXTURE=awsom run.sh nominal with SAB_SESSIONS=0 (expected pre-science stop)\n'
run_probe() {
  local label=$1 out="$PROBE/out-$1" log="$PROBE/$1.docker.log" status_file="$PROBE/$1.exit"
  test ! -e "$out"; mkdir -p "$out"
  set +e
  /home/huangzesen/bin/docker run --rm --name "sciaccel-pr463-probe-$label" --network none --cpus 8 --memory 16g \
    --volume "$PROBE:/probe:rw" sciaccel-batsrus-solar-corona-awsom-env \
    env -i PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin HOME=/root LANG=C.UTF-8 LC_ALL=C \
      SOURCE_DIR=/workspace/code OUT_DIR="/probe/out-$label" CHECK_DIR=/workspace/tests/checks/awsom \
      SAB_SOURCE_FINGERPRINT=452b3eb778286cd87a7fd100eeee3ccf46975c5dd52304c52d23145107837347 \
      SAB_BUILD_CACHE_ROOT=/probe/cache SAB_BUILD_JOBS=8 SAB_SESSIONS=0 SAB_MAX_ITERATION=0 \
      bash -x /workspace/tests/checks/awsom/run.sh nominal >"$log" 2>&1
  local status=$?
  set -e
  printf '%s\n' "$status" > "$status_file"
  printf 'PROBE_%s_EXIT=%s\n' "$label" "$status"
  grep -E 'cp -R|cd |Config\.pl|SAB_BUILD_CACHE=|SAB_BUILD_BINDIR=|SAB_BUILD_SECONDS=|SAB_SESSIONS=' "$log" || true
}
run_probe cold
run_probe hit
printf 'PROBE_END=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf 'PROBE_EXPECTATION=both exit 1 at SAB_SESSIONS=0 after cold/hit configuration; no BATSRUS/PostProc invocation\n'
printf '%s\n' 0 > "$EXIT"
