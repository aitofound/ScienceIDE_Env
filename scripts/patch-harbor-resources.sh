#!/usr/bin/env bash
# Make Harbor's Docker environment honour a task's [environment] block.
#
# Harbor declares resources it does not apply. `[environment] gpus`, `cpus` and
# `memory_mb` are read from task.toml, printed in the trial config, and then
# ignored by the Docker environment — the container gets the whole machine.
# Two failures follow, and both were measured in the 2026-08-12 sweep:
#
#   gpus   Harbor refuses the task outright rather than allocating a device.
#          `harbor/environments/docker/docker.py` declares no `gpus` capability
#          (only beam.py and opensandbox.py do, in 0.18.0 and 0.21.0 alike), so
#          base.py:736 raises before anything builds. The packaging machine
#          never saw it: `--override-gpus 0` zeroes the effective count and the
#          check is skipped. Once worked around, the container sees every card
#          on the host — sa-0008 passed on eight GPUs and failed on the one it
#          declares, which is the difference between "saturated, retire it" and
#          "the only package whose criteria reject anything".
#
#   cpus   Not applied at all. sa-0007's oracle reported 104.3 s where the same
#          CPU reference measured 62.9 s on the same host minutes later — 1.66x,
#          from four concurrent trials sharing 96 cores. Every package times its
#          incumbent under this budget and every submission runs without it, so
#          the two sides of a speedup are not the same measurement. Five of the
#          eight packages also declare a *serial* incumbent while granting 4-8
#          cores, which makes CPU parallelism free speedup: sa-0006 scored a
#          1.327x record with eight OpenMP workers and no GPU work at all.
#
# This patch adds the three lines to Harbor's own compose templates that let a
# caller supply them:
#
#     cpus:        ${SCIACCEL_CPUS:-0}          # CFS quota, 0 = unlimited
#     cpuset:      ${SCIACCEL_CPUSET:-}         # physical cores, empty = all
#     mem_limit:   ${SCIACCEL_MEM_MB:-0}m       # 0 = unlimited
#     environment:
#       NVIDIA_VISIBLE_DEVICES: ${SCIACCEL_GPUS:-all}
#
# Unset every variable and the templates behave exactly as Harbor ships them,
# so an unpatched workflow is unaffected. scripts/run-task.sh sets them from the
# package's own [environment] block, which is the point: the declaration stops
# being decorative.
#
# WHY BOTH cpus AND cpuset
#
# `cpus` is a CFS quota — it bounds CPU *time*, and `nproc` inside the container
# still reports every core on the host. Measured: with `cpus: 4` on this
# 96-thread machine, `nproc` said 96. A submission that sizes its thread pool
# from `nproc` will still spawn 96 threads and then contend with itself.
#
# `cpuset` pins the container to named cores, and `nproc` then reports the truth
# — measured: `cpuset: 8-11` gave `nproc` = 4. It also isolates concurrent
# trials from each other, which is what the 1.66x above was. Prefer it; the
# quota is the fallback when you would rather not manage core assignment.
#
# On a NUMA machine keep a cpuset inside one node where you can. This host has
# two nodes of 24 cores (48 threads) each, so ranges that straddle core 47 pay
# for remote memory. run-task.sh allocates contiguous ranges and does not model
# NUMA; override SCIACCEL_CPUSET directly when that matters.
#
# GPU DELIVERY, AND WHY IT IS AN ENV VAR RATHER THAN --gpus
#
# Harbor never passes `--gpus`, so the device has to arrive another way: with
# nvidia as Docker's *default runtime*, the NVIDIA container runtime reads
# `NVIDIA_VISIBLE_DEVICES` from the container's environment at creation and
# mounts exactly those devices. That is real device-level isolation — only
# /dev/nvidiaN appears and `nvidia-smi` inside reports one card — which
# CUDA_VISIBLE_DEVICES masking would not achieve, since nvidia-smi ignores it
# and an agent that can see eight cards will use eight.
#
# This edits an installed third-party package, so it does not survive a Harbor
# upgrade. Re-run it after upgrading. Idempotent; keeps a .orig backup.
#
# Usage:  bash scripts/patch-harbor-resources.sh [--revert]

set -euo pipefail

LOCATE='import harbor.environments.docker as m, pathlib; print(pathlib.Path(m.__file__).parent)'

find_dir() {
  # Harbor is commonly installed into its own interpreter (uv tool, pipx), so
  # the shell's python3 usually cannot see it. Try the obvious places in turn.
  local out
  for py in "${HARBOR_PYTHON:-}" python3 \
            "${HOME}/.local/share/uv/tools/harbor/bin/python" \
            "${HOME}/.local/pipx/venvs/harbor/bin/python"; do
    [ -n "${py}" ] || continue
    command -v "${py}" >/dev/null 2>&1 || [ -x "${py}" ] || continue
    out=$("${py}" -c "${LOCATE}" 2>/dev/null) || continue
    [ -n "${out}" ] && [ -d "${out}" ] && { printf '%s' "${out}"; return 0; }
  done
  if command -v uv >/dev/null 2>&1; then
    out=$(uv tool run --from harbor python -c "${LOCATE}" 2>/dev/null) || out=""
    [ -n "${out}" ] && [ -d "${out}" ] && { printf '%s' "${out}"; return 0; }
  fi
  return 1
}

DIR=$(find_dir) || {
  echo "error: could not locate harbor.environments.docker" >&2
  echo "       set HARBOR_PYTHON to the interpreter that has Harbor installed:" >&2
  echo "         HARBOR_PYTHON=/path/to/python bash scripts/patch-harbor-resources.sh" >&2
  exit 1
}

echo "harbor at: ${DIR}"

TEMPLATES=(docker-compose-build.yaml docker-compose-prebuilt.yaml)
MARKER=SCIACCEL_GPUS

if [ "${1:-}" = "--revert" ]; then
  for f in "${TEMPLATES[@]}"; do
    if [ -f "${DIR}/${f}.orig" ]; then
      mv "${DIR}/${f}.orig" "${DIR}/${f}"
      echo "reverted ${f}"
    else
      echo "no backup for ${f}, leaving as is"
    fi
  done
  exit 0
fi

for f in "${TEMPLATES[@]}"; do
  target="${DIR}/${f}"
  [ -f "${target}" ] || { echo "error: ${target} not found" >&2; exit 1; }

  # An earlier release of this script shipped as patch-harbor-gpu.sh and added
  # only the environment block. Recognise that, restore, and re-apply in full,
  # so upgrading does not leave a half-patched template behind.
  if grep -q "${MARKER}" "${target}" && ! grep -q "SCIACCEL_CPUSET" "${target}"; then
    if [ -f "${target}.orig" ]; then
      cp "${target}.orig" "${target}"
      echo "re-applying over the GPU-only patch: ${f}"
    fi
  fi

  if grep -q "SCIACCEL_CPUSET" "${target}"; then
    echo "already patched: ${f}"
    continue
  fi

  cp -n "${target}" "${target}.orig" 2>/dev/null || true
  cat >> "${target}" <<'YAML'
    cpus: ${SCIACCEL_CPUS:-0}
    cpuset: ${SCIACCEL_CPUSET:-}
    mem_limit: ${SCIACCEL_MEM_MB:-0}m
    environment:
      NVIDIA_VISIBLE_DEVICES: ${SCIACCEL_GPUS:-all}
YAML
  echo "patched ${f}"
done

echo
echo "Docker also needs nvidia as its default runtime, or no container gets a GPU:"
echo "  sudo nvidia-ctk runtime configure --runtime=docker --set-as-default"
echo "  sudo systemctl restart docker"
echo
echo "And on the systemd cgroup driver, any daemon-reload silently revokes a"
echo "running container's GPUs — see HANDOFF-GPU.md section 7:"
echo '  /etc/docker/daemon.json  ->  "exec-opts": ["native.cgroupdriver=cgroupfs"]'
