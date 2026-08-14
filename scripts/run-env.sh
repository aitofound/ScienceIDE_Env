#!/usr/bin/env bash
# Describe the machine a trial actually ran on, as JSON on stdout.
#
#   bash scripts/run-env.sh > run_env.json
#
# WHY
#
# `reward.json` is `{equivalence_pass, speedup}` and says nothing about the
# hardware that produced it. In the 2026-08-12 sweep, sa-0008 was run twice and
# the two reward files were byte-identical — while one container saw eight
# A100s and the other saw the one the package declares. That difference decided
# whether the package reads as saturated and due for retirement or as the only
# one in the registry whose criteria reject anything. Nothing in the produced
# artifacts distinguished them; the operator's memory did.
#
# The same sweep found the host's own state mattering just as much: four
# concurrent trials sharing 96 cores moved one CPU reference from 62.9 s to
# 104.3 s, and another tenant's job held ~70 GB on every card throughout. None
# of that was recorded either.
#
# WHAT IT RECORDS, AND FROM WHERE
#
# Host facts come from the host. The container's view comes from a throwaway
# container started with the same SCIACCEL_* variables the trial used, because
# what matters for reproducing a result is not what the machine has but what
# the trial could reach. Run it with those variables exported — run-task.sh
# does — and the `container` block describes the envelope that trial ran in.
#
# Without Docker, or without a driver, the corresponding blocks come back null
# rather than absent: a reader should be able to tell "there was no GPU" from
# "nobody looked".

set -uo pipefail

PROBE_IMAGE=${SCIACCEL_PROBE_IMAGE:-nvidia/cuda:12.6.2-devel-ubuntu24.04}

jstr() {
  # JSON string escaping for arbitrary command output: quotes, backslashes,
  # control characters and newlines, which appear in driver strings and paths.
  if [ $# -eq 0 ] || [ -z "${1:-}" ]; then printf 'null'; return; fi
  printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\t/\\t/g' \
    | awk 'BEGIN{printf "\""} {printf "%s%s", sep, $0; sep="\\n"} END{printf "\""}'
}

jnum() { if [ -z "${1:-}" ]; then printf 'null'; else printf '%s' "$1"; fi; }

# --- host ------------------------------------------------------------------

NOW=$(date -u +%Y-%m-%dT%H:%M:%SZ)
HOSTNAME_=$(hostname 2>/dev/null || true)
KERNEL=$(uname -r 2>/dev/null || true)
HOST_CPUS=$(nproc 2>/dev/null || true)
HOST_MEM_MB=$(awk '/MemTotal/ {printf "%d", $2/1024}' /proc/meminfo 2>/dev/null || true)

DRIVER=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1 || true)
GPU_NAMES=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | sort -u | paste -sd';' - || true)
GPU_COUNT=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | grep -c . || true)
# Memory already in use across the host when this trial started: the honest
# measure of how much of the machine belonged to somebody else.
GPU_USED_MIB=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null | awk '{s+=$1} END{printf "%d", s}' || true)
GPU_TOTAL_MIB=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null | awk '{s+=$1} END{printf "%d", s}' || true)

DOCKER_V=$(docker --version 2>/dev/null | sed 's/,.*//' || true)
DOCKER_RUNTIME=$(docker info --format '{{.DefaultRuntime}}' 2>/dev/null || true)
DOCKER_CGROUP=$(docker info --format '{{.CgroupDriver}}' 2>/dev/null || true)
# Concurrency: how many other containers were running. Four at once cost 1.66x
# on a CPU baseline in the sweep this file exists because of.
CONTAINERS=$(docker ps -q 2>/dev/null | grep -c . || true)
HARBOR_V=$(harbor --version 2>/dev/null | head -1 || true)

# --- what a container with this envelope actually sees ---------------------

C_GPUS=""; C_CPUS=""; C_MEM_MB=""; C_HOSTMEM=""; C_GPU_NAMES=""; C_OK=false
if docker info >/dev/null 2>&1; then
  OUT=$(docker run --rm \
          -e NVIDIA_VISIBLE_DEVICES="${SCIACCEL_GPUS:-all}" \
          ${SCIACCEL_CPUSET:+--cpuset-cpus "${SCIACCEL_CPUSET}"} \
          ${SCIACCEL_MEM_MB:+--memory "${SCIACCEL_MEM_MB}m"} \
          "${PROBE_IMAGE}" \
          bash -c 'echo "CPUS=$(nproc)";
                   # /proc/meminfo is not namespaced — it reports the host total
                   # even under --memory, which would make this field a lie. The
                   # limit lives in the cgroup: v2 first, then v1, then nothing.
                   lim=$(cat /sys/fs/cgroup/memory.max 2>/dev/null || cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null || echo max);
                   case "$lim" in
                     max|"" ) echo "MEM=" ;;
                     *) echo "MEM=$(( lim / 1048576 ))" ;;
                   esac;
                   echo "HOSTMEM=$(awk "/MemTotal/ {printf \"%d\", \$2/1024}" /proc/meminfo)";
                   echo "GPUN=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | grep -c . || echo 0)";
                   echo "GPUS=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | sort -u | paste -sd";" -)"' \
        2>/dev/null) || OUT=""
  if [ -n "${OUT}" ]; then
    C_OK=true
    C_CPUS=$(printf '%s' "${OUT}" | sed -n 's/^CPUS=//p')
    C_MEM_MB=$(printf '%s' "${OUT}" | sed -n 's/^MEM=//p')
    C_HOSTMEM=$(printf '%s' "${OUT}" | sed -n 's/^HOSTMEM=//p')
    C_GPUS=$(printf '%s' "${OUT}" | sed -n 's/^GPUN=//p')
    C_GPU_NAMES=$(printf '%s' "${OUT}" | sed -n 's/^GPUS=//p')
  fi
fi

cat <<JSON
{
  "schema": "sciaccel/run_env@1",
  "captured_at": "${NOW}",
  "host": {
    "hostname": $(jstr "${HOSTNAME_}"),
    "kernel": $(jstr "${KERNEL}"),
    "cpus": $(jnum "${HOST_CPUS}"),
    "memory_mb": $(jnum "${HOST_MEM_MB}"),
    "gpu_driver": $(jstr "${DRIVER}"),
    "gpu_names": $(jstr "${GPU_NAMES}"),
    "gpu_count": $(jnum "${GPU_COUNT}"),
    "gpu_memory_used_mib": $(jnum "${GPU_USED_MIB}"),
    "gpu_memory_total_mib": $(jnum "${GPU_TOTAL_MIB}"),
    "docker": $(jstr "${DOCKER_V}"),
    "docker_default_runtime": $(jstr "${DOCKER_RUNTIME}"),
    "docker_cgroup_driver": $(jstr "${DOCKER_CGROUP}"),
    "containers_running": $(jnum "${CONTAINERS}"),
    "harbor": $(jstr "${HARBOR_V}")
  },
  "requested": {
    "SCIACCEL_GPUS": $(jstr "${SCIACCEL_GPUS:-}"),
    "SCIACCEL_CPUSET": $(jstr "${SCIACCEL_CPUSET:-}"),
    "SCIACCEL_CPUS": $(jstr "${SCIACCEL_CPUS:-}"),
    "SCIACCEL_MEM_MB": $(jstr "${SCIACCEL_MEM_MB:-}")
  },
  "container": {
    "probed": ${C_OK},
    "image": $(jstr "${PROBE_IMAGE}"),
    "cpus": $(jnum "${C_CPUS}"),
    "memory_limit_mb": $(jnum "${C_MEM_MB}"),
    "memory_visible_mb": $(jnum "${C_HOSTMEM}"),
    "gpu_count": $(jnum "${C_GPUS}"),
    "gpu_names": $(jstr "${C_GPU_NAMES}")
  },
  "summary": {
    "_note": "Every key here is unique in this document, so a reader without a JSON parser can grep for it. archive-run.sh does exactly that, and got the host's gpu_count instead of the container's when the names collided — the one field this file exists to record.",
    "container_gpu_count": $(jnum "${C_GPUS}"),
    "container_cpu_count": $(jnum "${C_CPUS}"),
    "container_memory_limit_mb": $(jnum "${C_MEM_MB}"),
    "host_name": $(jstr "${HOSTNAME_}"),
    "host_gpu_driver": $(jstr "${DRIVER}"),
    "host_containers_running": $(jnum "${CONTAINERS}"),
    "harbor_version": $(jstr "${HARBOR_V}")
  }
}
JSON
