#!/usr/bin/env bash
# Prove the machine can run this registry, before spending a night finding out.
#
#   bash scripts/smoke-gpu.sh              # preflight only, ~2 minutes
#   bash scripts/smoke-gpu.sh sa-0008 0    # preflight, then the oracle and nop
#
# WHY
#
# Every check here corresponds to something that went wrong on 2026-08-12, and
# each was expensive because it surfaced late and pointed elsewhere:
#
#   Harbor refuses a GPU task on the Docker environment. The first real run died
#   on it, and the handoff's own instructions ("drop the --override-gpus 0")
#   were the thing that triggered it. Invisible from the packaging machine,
#   where --override-gpus 0 skips the check entirely.
#
#   On the systemd cgroup driver, any daemon-reload revokes a running
#   container's GPUs. It killed a 35-minute trial mid-flight, and the symptom
#   points nowhere near the cause: NVML fails inside the container while every
#   /dev/nvidia* is still present and the host's nvidia-smi stays healthy. What
#   triggered it was installing an unrelated package during the run.
#
#   [environment] cpus and gpus are declared and not applied. sa-0008 passed on
#   eight GPUs and failed on the one it declares; a CPU baseline moved 1.66x
#   between an idle host and four concurrent trials.
#
# Each check prints what it found, not just whether it passed, because the value
# is in the numbers when one of them is surprising.
#
# Exit status is the number of failed checks, so CI can gate on it.

set -uo pipefail

SLUG=${1:-}
SLOT=${2:-0}
IMAGE=${SCIACCEL_PROBE_IMAGE:-nvidia/cuda:12.6.2-devel-ubuntu24.04}
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

FAIL=0
ok()   { printf '  \033[32mok\033[0m    %s\n' "$1"; }
bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; FAIL=$((FAIL+1)); }
warn() { printf '  \033[33mwarn\033[0m  %s\n' "$1"; }
head_() { printf '\n%s\n' "$1"; }

# --- 1. the toolchain ------------------------------------------------------

head_ "toolchain"

if command -v docker >/dev/null 2>&1; then ok "docker $(docker --version 2>/dev/null | sed 's/,.*//;s/Docker version //')"
else bad "docker not installed"; fi

if docker info >/dev/null 2>&1; then
  ok "docker daemon reachable"
else
  if id -nG 2>/dev/null | tr ' ' '\n' | grep -qx docker; then
    bad "cannot reach the daemon, though you are in the docker group — open a new shell, or run this under 'sg docker -c'"
  else
    bad "cannot reach the docker daemon (not in the docker group?)"
  fi
fi

command -v nvidia-smi >/dev/null 2>&1 && ok "nvidia-smi $(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1)" \
                                       || bad "nvidia-smi not on PATH — no driver on this host"

if command -v harbor >/dev/null 2>&1; then ok "harbor $(harbor --version 2>/dev/null | head -1)"
else bad "harbor not on PATH"; fi

# --- 2. the two daemon settings nothing else will tell you about -----------

head_ "docker configuration"

RUNTIME=$(docker info --format '{{.DefaultRuntime}}' 2>/dev/null || echo "")
if [ "${RUNTIME}" = "nvidia" ]; then
  ok "default runtime is nvidia — containers Harbor starts without --gpus still get one"
else
  bad "default runtime is '${RUNTIME:-unknown}', not nvidia. Harbor never passes --gpus, so no container will see a device:
          sudo nvidia-ctk runtime configure --runtime=docker --set-as-default && sudo systemctl restart docker"
fi

CGROUP=$(docker info --format '{{.CgroupDriver}}' 2>/dev/null || echo "")
if [ "${CGROUP}" = "cgroupfs" ]; then
  ok "cgroup driver is cgroupfs — a daemon-reload will not revoke a running container's GPUs"
else
  bad "cgroup driver is '${CGROUP:-unknown}'. On systemd, any daemon-reload silently revokes GPUs from
          running containers, and the failure looks like a broken driver:
          add \"exec-opts\": [\"native.cgroupdriver=cgroupfs\"] to /etc/docker/daemon.json and restart"
fi

# --- 3. the compose patch --------------------------------------------------

head_ "harbor patch"

HARBOR_DIR=$(python3 -c 'import harbor.environments.docker as m,pathlib;print(pathlib.Path(m.__file__).parent)' 2>/dev/null \
  || "${HOME}/.local/share/uv/tools/harbor/bin/python" -c 'import harbor.environments.docker as m,pathlib;print(pathlib.Path(m.__file__).parent)' 2>/dev/null \
  || echo "")
if [ -n "${HARBOR_DIR}" ] && grep -q SCIACCEL_CPUSET "${HARBOR_DIR}/docker-compose-build.yaml" 2>/dev/null; then
  ok "compose templates carry the resource variables"
elif [ -n "${HARBOR_DIR}" ] && grep -q SCIACCEL_GPUS "${HARBOR_DIR}/docker-compose-build.yaml" 2>/dev/null; then
  bad "templates have the GPU-only patch — re-run: bash scripts/patch-harbor-resources.sh"
else
  bad "templates unpatched; [environment] will not be applied. Run: bash scripts/patch-harbor-resources.sh"
fi

# --- 4. a GPU that actually computes ---------------------------------------

head_ "device"

if docker info >/dev/null 2>&1; then
  PROBE=$(docker run --rm -e NVIDIA_VISIBLE_DEVICES="${SLOT}" "${IMAGE}" bash -c '
      cat > /tmp/p.cu <<EOF
#include <cstdio>
__global__ void k(float*o){o[threadIdx.x]=threadIdx.x*2.0f;}
int main(){int n=0;cudaError_t e=cudaGetDeviceCount(&n);
if(e!=cudaSuccess||n==0){printf("DEVICES=0\n");return 1;}
printf("DEVICES=%d\n",n);
float*d;cudaMalloc(&d,32);k<<<1,8>>>(d);
cudaError_t s=cudaDeviceSynchronize();float h[8];cudaMemcpy(h,d,32,cudaMemcpyDeviceToHost);
printf("KERNEL=%s OUT=%.1f\n",cudaGetErrorString(s),h[3]);
return (s==cudaSuccess&&h[3]==6.0f)?0:1;}
EOF
      nvcc -o /tmp/p /tmp/p.cu >/dev/null 2>&1 && /tmp/p' 2>/dev/null | grep -E "^(DEVICES|KERNEL)=") || PROBE=""

  if printf '%s' "${PROBE}" | grep -q 'OUT=6.0'; then
    ok "compiled and ran a kernel: $(printf '%s' "${PROBE}" | tr '\n' ' ')"
  else
    bad "no working CUDA runtime in a container: ${PROBE:-nvcc or the kernel failed}"
  fi

  # The one that cost a trial. Reload while a container holds a device, then ask
  # the container again — a passing 'device' check above proves nothing about
  # whether the device survives the next apt install.
  CID=$(docker run -d --rm -e NVIDIA_VISIBLE_DEVICES="${SLOT}" "${IMAGE}" sleep 90 2>/dev/null || echo "")
  if [ -n "${CID}" ]; then
    BEFORE=$(docker exec "${CID}" nvidia-smi -L 2>/dev/null | grep -c '^GPU' || echo 0)
    sudo systemctl daemon-reload 2>/dev/null || warn "could not run daemon-reload (no sudo?) — reload survival untested"
    sleep 2
    AFTER=$(docker exec "${CID}" nvidia-smi -L 2>/dev/null | grep -c '^GPU' || echo 0)
    docker stop -t 1 "${CID}" >/dev/null 2>&1
    if [ "${BEFORE}" -gt 0 ] && [ "${AFTER}" = "${BEFORE}" ]; then
      ok "GPUs survive a daemon-reload (${BEFORE} before, ${AFTER} after)"
    elif [ "${BEFORE}" -gt 0 ]; then
      bad "daemon-reload revoked the container's GPUs (${BEFORE} -> ${AFTER}) — this will kill long runs mid-flight"
    else
      warn "container saw no GPU to begin with; reload survival not tested"
    fi
  fi
fi

# --- 5. the declared envelope, if a package was named ----------------------

if [ -n "${SLUG}" ]; then
  head_ "envelope for ${SLUG}"
  TOML="${REPO_ROOT}/tasks/${SLUG}/task.toml"
  if [ ! -f "${TOML}" ]; then
    bad "${TOML} not found — check out the branch that carries the package"
  else
    getk() { awk -v k="$1" '/^\[/{i=($0~/^\[environment\]/)} i&&$1==k&&$2=="="{gsub(/[^0-9]/,"",$3);print $3;exit}' "${TOML}"; }
    D_CPUS=$(getk cpus); D_MEM=$(getk memory_mb); D_GPUS=$(getk gpus)
    echo "        declares: gpus=${D_GPUS:-unset} cpus=${D_CPUS:-unset} memory_mb=${D_MEM:-unset}"

    if [ -n "${D_CPUS}" ]; then
      FIRST=$(( SLOT * D_CPUS )); LAST=$(( FIRST + D_CPUS - 1 ))
      SEEN=$(docker run --rm -e NVIDIA_VISIBLE_DEVICES="${SLOT}" \
               --cpuset-cpus "${FIRST}-${LAST}" --memory "${D_MEM:-0}m" "${IMAGE}" \
               bash -c 'echo "SEEN $(nproc) $(nvidia-smi -L 2>/dev/null | grep -c "^GPU") $(( $(cat /sys/fs/cgroup/memory.max 2>/dev/null || echo 0) / 1048576 ))"' 2>/dev/null \
               | sed -n 's/^SEEN //p') || SEEN=""
      set -- ${SEEN:-0 0 0}
      [ "${1}" = "${D_CPUS}" ] && ok "cpus enforced: container sees ${1}" || bad "cpus declared ${D_CPUS}, container sees ${1}"
      [ "${2}" = "${D_GPUS:-1}" ] && ok "gpus enforced: container sees ${2}" || bad "gpus declared ${D_GPUS:-1}, container sees ${2}"
      [ -n "${D_MEM}" ] && { [ "${3}" = "${D_MEM}" ] && ok "memory enforced: ${3} MB" || bad "memory declared ${D_MEM} MB, cgroup says ${3} MB"; }
    fi
  fi

  if [ -d "${REPO_ROOT}/tasks/${SLUG}/environment" ] && [ "${FAIL}" -eq 0 ]; then
    head_ "ladder for ${SLUG}"
    echo "        running oracle then nop; each should score 1 and 0"
    bash "${REPO_ROOT}/scripts/run-task.sh" "${SLUG}" oracle "${SLOT}" >/dev/null 2>&1 \
      && ok "oracle ran" || bad "oracle run failed"
    bash "${REPO_ROOT}/scripts/run-task.sh" "${SLUG}" nop "${SLOT}" >/dev/null 2>&1 \
      && ok "nop ran" || bad "nop run failed"
    echo "        verdicts: check registry/runs.yaml after 'npm run runs'"
  fi
fi

head_ "$([ "${FAIL}" -eq 0 ] && echo 'all checks passed' || echo "${FAIL} check(s) failed")"
exit "${FAIL}"
