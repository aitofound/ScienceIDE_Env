#!/usr/bin/env bash
# Give Harbor's Docker environment a way to hand a container exactly one GPU.
#
# Harbor does not allocate GPUs on the Docker environment. Only its Beam and
# OpenSandbox environments declare `gpus=True`; `harbor/environments/docker/
# docker.py` does not, so `base.py` raises as soon as a task asks for one:
#
#   RuntimeError: Task requires 1 GPU(s) but EnvironmentType.DOCKER
#   environment does not support GPU allocation.
#
# Verified identical in Harbor 0.18.0 and 0.21.0. The packaging machine never
# saw it because `--override-gpus 0` drives the effective count to zero and the
# check is skipped.
#
# The workaround has two halves and needs both:
#
#   1. nvidia as Docker's *default* runtime, so containers Harbor starts without
#      `--gpus` still get the NVIDIA stack injected.
#   2. this patch, which adds an `environment:` block to Harbor's own compose
#      templates so `NVIDIA_VISIBLE_DEVICES` reaches the container.
#
# With both in place, `SCIACCEL_GPUS=3 harbor run ...` mounts only /dev/nvidia3
# and `nvidia-smi` inside the container reports exactly one device. That is
# real device-level isolation, not CUDA_VISIBLE_DEVICES masking — nvidia-smi
# would still list every card under the latter, and a task package that
# declares `gpus = 1` deserves better than a suggestion.
#
# Unset SCIACCEL_GPUS and the templates fall back to `all`, which is Harbor's
# behaviour today.
#
# This edits an installed third-party package, so it does not survive a Harbor
# upgrade. Re-run it after upgrading. Idempotent; keeps a .orig backup.
#
# Usage:  bash scripts/patch-harbor-gpu.sh [--revert]

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
  # Last resort: ask uv to resolve it.
  if command -v uv >/dev/null 2>&1; then
    out=$(uv tool run --from harbor python -c "${LOCATE}" 2>/dev/null) || out=""
    [ -n "${out}" ] && [ -d "${out}" ] && { printf '%s' "${out}"; return 0; }
  fi
  return 1
}

DIR=$(find_dir) || {
  echo "error: could not locate harbor.environments.docker" >&2
  echo "       set HARBOR_PYTHON to the interpreter that has Harbor installed:" >&2
  echo "         HARBOR_PYTHON=/path/to/python bash scripts/patch-harbor-gpu.sh" >&2
  exit 1
}

echo "harbor at: ${DIR}"

TEMPLATES=(docker-compose-build.yaml docker-compose-prebuilt.yaml)

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

  if grep -q "SCIACCEL_GPUS" "${target}"; then
    echo "already patched: ${f}"
    continue
  fi

  cp -n "${target}" "${target}.orig"
  cat >> "${target}" <<'YAML'
    environment:
      NVIDIA_VISIBLE_DEVICES: ${SCIACCEL_GPUS:-all}
YAML
  echo "patched ${f}"
done

echo
echo "Docker also needs nvidia as its default runtime:"
echo "  sudo nvidia-ctk runtime configure --runtime=docker --set-as-default"
echo "  sudo systemctl restart docker"
