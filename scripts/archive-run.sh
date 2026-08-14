#!/usr/bin/env bash
# Turn a finished Harbor job directory into a record that outlives it.
#
#   bash scripts/archive-run.sh <job-dir> [archive-root]
#
# WHY
#
# A Harbor job directory is a working area, not a record, and treating it as
# one loses evidence. Three ways, all of them hit during the 2026-08-12 sweep:
#
#   Harbor refuses to reuse a job directory even after a failed run
#   (FileExistsError), so the obvious fix is to delete it and retry — which is
#   what happened, and it destroyed a 144 KB agent transcript. The run had been
#   invalidated by a GPU fault, but the transcript was still perfectly good
#   evidence about the task's difficulty. Verdicts and evidence have different
#   lifetimes and should not share a directory.
#
#   Images are deleted after every trial by default (`delete=True`), so the
#   oracle, nop and agent runs of one package each pay the build. NWChem's is an
#   hour. run-task.sh passes --no-delete; this script exists for what --no-delete
#   does not cover.
#
#   Nothing in the registry could hold the result. `registry/records.yaml` has
#   room for `record` and `solve_rate` — the operator's official numbers — and
#   nothing else. Eight verification runs had nowhere to live but a markdown
#   file and eight PR comments.
#
# WHAT IT KEEPS
#
# Everything small and decisive is copied: the reward, the per-criterion report,
# the provenance, the submitted timing and device-activity records, the
# verifier's stdout. Agent transcripts are copied when they are under the size
# cap and recorded by hash and size when they are over it, because a 70 MB
# session belongs on a host that serves it and a checksum is what makes the
# hosted copy trustworthy.
#
# manifest.json lists every file with its sha256, so an archive that has been
# edited since it was written can be told from one that has not. run.yaml is the
# one-row summary; scripts/index-runs.mjs collects those into
# registry/runs.yaml, which is the part that belongs in git. Archiving and
# committing are deliberately separate steps: what an operator records and what
# the registry publishes are different decisions.
#
# Environment:
#   SCIACCEL_ARCHIVE_MAX_MB   per-file copy cap, default 25

set -uo pipefail

JOB_DIR=${1:-}
ARCHIVE_ROOT=${2:-${SCIACCEL_ARCHIVE:-${HOME}/sciaccel-archive}}
MAX_MB=${SCIACCEL_ARCHIVE_MAX_MB:-25}

if [ -z "${JOB_DIR}" ] || [ ! -d "${JOB_DIR}" ]; then
  echo "usage: archive-run.sh <job-dir> [archive-root]" >&2
  exit 2
fi

# Normalise away a trailing slash: `for j in .../*/` hands one over, and every
# later ${f#${JOB_DIR}/} prefix strip then silently fails, which turns archived
# names into flattened absolute paths.
JOB_DIR=${JOB_DIR%/}

JOB_NAME=$(basename "${JOB_DIR}")
# Job names are <slug>-<agent>-<stamp>; recover the parts without assuming the
# slug has no dashes beyond the sa-NNNN shape.
SLUG=$(printf '%s' "${JOB_NAME}" | grep -oE '^[a-z]{2}-[0-9]{4}' || true)
STAMP=$(printf '%s' "${JOB_NAME}" | grep -oE '[0-9]{8}T[0-9]{6}Z$' || date -u +%Y%m%dT%H%M%SZ)
AGENT=$(printf '%s' "${JOB_NAME}" | sed -E "s/^${SLUG}-//; s/-${STAMP}$//" || true)

# A job directory named by hand — or by an earlier convention — carries none of
# this in its name. Let the caller say so rather than filing the run under
# "unknown", which is how evidence stops being findable.
SLUG=${SCIACCEL_ARCHIVE_SLUG:-${SLUG}}
AGENT=${SCIACCEL_ARCHIVE_AGENT:-${AGENT}}
STAMP=${SCIACCEL_ARCHIVE_STAMP:-${STAMP}}
[ -n "${SLUG}" ] || SLUG=unknown

# The agent belongs in the path, not just in run.yaml. The oracle, nop and agent
# runs of one package are commonly launched together and share a timestamp — the
# 2026-08-12 sweep's queue used one stamp for all three — so a <slug>/<stamp>
# directory silently interleaves three runs and keeps whichever wrote last.
DEST="${ARCHIVE_ROOT}/${SLUG}/${STAMP}-${AGENT:-unknown}"
# A re-archive of the same run replaces it rather than merging into it, so a
# directory never holds files from two different trials.
rm -rf "${DEST}"
mkdir -p "${DEST}" || { echo "archive-run: cannot create ${DEST}" >&2; exit 1; }

sha() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }
size() { stat -c%s "$1" 2>/dev/null || echo 0; }

MANIFEST_ROWS=""
add_row() {
  local rel=$1 src=$2 kept=$3
  local s h
  s=$(size "${src}"); h=$(sha "${src}")
  MANIFEST_ROWS="${MANIFEST_ROWS}${MANIFEST_ROWS:+,}
    {\"path\": \"${rel}\", \"source\": \"${src#${JOB_DIR}/}\", \"bytes\": ${s:-0}, \"sha256\": \"${h:-}\", \"copied\": ${kept}}"
}

copy_in() {
  local src=$1 rel=$2
  [ -f "${src}" ] || return 0
  local bytes cap
  bytes=$(size "${src}")
  cap=$(( MAX_MB * 1024 * 1024 ))
  mkdir -p "${DEST}/$(dirname "${rel}")"
  if [ "${bytes}" -le "${cap}" ]; then
    cp -a "${src}" "${DEST}/${rel}"
    add_row "${rel}" "${src}" true
  else
    # Too large to keep here. Record what it was and what it hashed to, so a
    # hosted copy can be matched against this run rather than merely asserted
    # to belong to it.
    add_row "${rel}" "${src}" false
  fi
}

# The decisive small files, wherever Harbor put them under the trial directory.
while IFS= read -r f; do
  case "$(basename "${f}")" in
    reward.json|equivalence_report.json|timing.json|device_activity.json|run_env.json)
      copy_in "${f}" "$(printf '%s' "${f#${JOB_DIR}/}" | tr '/' '_')" ;;
  esac
done < <(find "${JOB_DIR}" -type f -name '*.json' 2>/dev/null)

# Verifier stdout and the trial log: short, and the only place a grader crash
# explains itself.
while IFS= read -r f; do
  copy_in "${f}" "$(printf '%s' "${f#${JOB_DIR}/}" | tr '/' '_')"
done < <(find "${JOB_DIR}" -type f \( -name 'test-stdout.txt' -o -name 'trial.log' -o -name 'job.log' \) 2>/dev/null)

# Agent transcripts — the evidence a deleted job directory takes with it.
while IFS= read -r f; do
  copy_in "${f}" "transcripts/$(basename "${f}" .txt).jsonl"
done < <(find "${JOB_DIR}" -type f -path '*/agent/*' -name '*.txt' 2>/dev/null)

# --- the one-row summary ---------------------------------------------------

REWARD=$(find "${JOB_DIR}" -name reward.json -type f 2>/dev/null | head -1)
PASS=""; SPEEDUP=""
if [ -n "${REWARD}" ]; then
  PASS=$(grep -oE '"equivalence_pass"[[:space:]]*:[[:space:]]*[0-9]+' "${REWARD}" | grep -oE '[0-9]+$' || true)
  SPEEDUP=$(grep -oE '"speedup"[[:space:]]*:[[:space:]]*[0-9.eE+-]+' "${REWARD}" | sed 's/.*:[[:space:]]*//' || true)
fi

ENVJ=$(find "${JOB_DIR}" -name run_env.json -type f 2>/dev/null | head -1)
getj() { [ -n "${ENVJ}" ] && grep -oE "\"$1\"[[:space:]]*:[[:space:]]*(\"[^\"]*\"|[0-9.]+|null)" "${ENVJ}" | head -1 | sed 's/.*:[[:space:]]*//; s/^"//; s/"$//' || true; }

# Read the summary block, whose keys are unique in the document. Reading
# "gpu_count" instead matched the HOST's count first and recorded 8 where the
# container had 1 — the exact confusion this record exists to prevent.
C_GPUS=$(getj container_gpu_count); C_CPUS=$(getj container_cpu_count)
HOSTNAME_=$(getj host_name); DRIVER=$(getj host_gpu_driver)
CONTAINERS=$(getj host_containers_running); HARBOR_V=$(getj harbor_version)

cat > "${DEST}/run.yaml" <<YAML
# One verification run. scripts/index-runs.mjs collects these into
# registry/runs.yaml; this file is the archive's own copy and is never edited.
slug: ${SLUG}
at: $(date -u -d "$(printf '%s' "${STAMP}" | sed -E 's/^(....)(..)(..)T(..)(..)(..)Z$/\1-\2-\3 \4:\5:\6/')" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || echo "${STAMP}")
agent: ${AGENT:-unknown}
equivalence_pass: ${PASS:-null}
speedup: ${SPEEDUP:-null}
container_gpus: ${C_GPUS:-null}
container_cpus: ${C_CPUS:-null}
host: ${HOSTNAME_:-null}
gpu_driver: ${DRIVER:-null}
concurrent_containers: ${CONTAINERS:-null}
harbor: ${HARBOR_V:-null}
archive: ${SLUG}/${STAMP}-${AGENT:-unknown}
YAML

cat > "${DEST}/manifest.json" <<JSON
{
  "schema": "sciaccel/archive@1",
  "job_name": "${JOB_NAME}",
  "job_dir": "${JOB_DIR}",
  "archived_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "max_copied_mb": ${MAX_MB},
  "files": [${MANIFEST_ROWS}
  ]
}
JSON

COUNT=$(grep -c '"path"' "${DEST}/manifest.json" 2>/dev/null || echo 0)
echo "archived ${COUNT} file(s) -> ${DEST}"
