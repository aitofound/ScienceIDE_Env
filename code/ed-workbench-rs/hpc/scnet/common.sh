#!/usr/bin/env bash

set -euo pipefail

readonly QH129_EXPECTED_COMMIT="48f1964a1b3b88090497e1ffce285fde09c98541"
readonly QH129_EXPECTED_LOCK_SHA256="3e47c3256ebc4bb6503c447f124c2050d8a8c718f567ff1e53efe727d196533b"
readonly QH129_EXPECTED_FCIDUMP_SHA256="826dd373a8b6047dff8136168431a803b59d9ef029a074da3b8f74f22603db3e"
readonly QH129_EXPECTED_LIBCINT_ARCHIVE_SHA256="9e5a4b9aea855317f48e7915b5ecd49cb2bbd96dee33cc073a36f65dafe2e16a"
readonly QH129_EXPECTED_LIBCINT_TREE="3de5cd4cf6b7f3fe04d53dfeed3dc85f69eb1133"
readonly QH129_REFERENCE_ENERGY="-76.121174204141980"

readonly QH129_REMOTE_ROOT="${QH129_REMOTE_ROOT:-/work/share/giggleliu/cfys01/quantum-harness-129}"
readonly QH129_SOURCE="${QH129_SOURCE:-${QH129_REMOTE_ROOT}/source-v0.4.0}"
readonly QH129_RUN_ROOT="${QH129_RUN_ROOT:-${QH129_REMOTE_ROOT}/runs}"
readonly QH129_TOOLCHAIN_ROOT="${QH129_TOOLCHAIN_ROOT:-${QH129_REMOTE_ROOT}/toolchains}"
readonly QH129_RUST_HOME="${QH129_RUST_HOME:-${QH129_TOOLCHAIN_ROOT}/rust-1.89.0}"
readonly QH129_CARGO_HOME="${QH129_CARGO_HOME:-${QH129_TOOLCHAIN_ROOT}/cargo}"
readonly QH129_VENDOR_ROOT="${QH129_VENDOR_ROOT:-${QH129_TOOLCHAIN_ROOT}/vendor}"
readonly QH129_LIBCINT_ARCHIVE="${QH129_LIBCINT_ARCHIVE:-${QH129_TOOLCHAIN_ROOT}/libcint-v6.1.2-offline.tar.gz}"
readonly QH129_ARTIFACT_ROOT="${QH129_ARTIFACT_ROOT:-${QH129_REMOTE_ROOT}/artifacts/v0.4.0}"
readonly QH129_BINARY="${QH129_BINARY:-${QH129_ARTIFACT_ROOT}/ed_workbench_rs}"
readonly QH129_FCIDUMP="${QH129_FCIDUMP:-${QH129_SOURCE}/fixtures/h2o-631g-fc/FCIDUMP}"

qh129_sha256() {
    sha256sum "$1" | awk '{print $1}'
}

qh129_git() {
    git --git-dir="${QH129_SOURCE}/.git" --work-tree="${QH129_SOURCE}" "$@"
}

qh129_verify_source() {
    local actual_commit
    local actual_lock_sha256
    local actual_fcidump_sha256

    test -d "${QH129_SOURCE}/.git"
    actual_commit="$(qh129_git rev-parse HEAD)"
    test "${actual_commit}" = "${QH129_EXPECTED_COMMIT}"
    test -z "$(qh129_git status --porcelain --untracked-files=no)"

    actual_lock_sha256="$(qh129_sha256 "${QH129_SOURCE}/Cargo.lock")"
    test "${actual_lock_sha256}" = "${QH129_EXPECTED_LOCK_SHA256}"

    actual_fcidump_sha256="$(qh129_sha256 "${QH129_FCIDUMP}")"
    test "${actual_fcidump_sha256}" = "${QH129_EXPECTED_FCIDUMP_SHA256}"
}

qh129_activate_toolchain() {
    export CARGO_HOME="${QH129_CARGO_HOME}"
    export PATH="${QH129_RUST_HOME}/bin:${QH129_CARGO_HOME}/bin:${PATH}"
    export CARGO_NET_OFFLINE=true
}

qh129_require_toolchain() {
    qh129_activate_toolchain
    test -d "${QH129_VENDOR_ROOT}"
    test -f "${QH129_CARGO_HOME}/config.toml"
    test "$(qh129_sha256 "${QH129_LIBCINT_ARCHIVE}")" = \
        "${QH129_EXPECTED_LIBCINT_ARCHIVE_SHA256}"
    rustc --version | grep -F 'rustc 1.89.0 '
    cargo --version
}

qh129_record_environment() {
    local output="$1"
    local binary_sha256="not-built"

    if test -x "${QH129_BINARY}"; then
        binary_sha256="$(qh129_sha256 "${QH129_BINARY}")"
    fi

    {
        printf 'schema=quantum-harness-129.scnet-environment.v1\n'
        printf 'recorded_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        printf 'job_id=%s\n' "${SLURM_JOB_ID:-not-in-slurm}"
        printf 'array_job_id=%s\n' "${SLURM_ARRAY_JOB_ID:-none}"
        printf 'array_task_id=%s\n' "${SLURM_ARRAY_TASK_ID:-none}"
        printf 'hostname=%s\n' "$(hostname)"
        printf 'allocated_cpus=%s\n' "${SLURM_CPUS_ON_NODE:-unknown}"
        printf 'source_commit=%s\n' "${QH129_EXPECTED_COMMIT}"
        printf 'cargo_lock_sha256=%s\n' "${QH129_EXPECTED_LOCK_SHA256}"
        printf 'fcidump_sha256=%s\n' "${QH129_EXPECTED_FCIDUMP_SHA256}"
        printf 'libcint_archive_sha256=%s\n' "${QH129_EXPECTED_LIBCINT_ARCHIVE_SHA256}"
        printf 'libcint_tree=%s\n' "${QH129_EXPECTED_LIBCINT_TREE}"
        printf 'binary_sha256=%s\n' "${binary_sha256}"
        printf 'kernel=%s\n' "$(uname -srmo)"
        printf 'cpu_model=%s\n' "$(awk -F: '/model name/{gsub(/^ +/,"",$2); print $2; exit}' /proc/cpuinfo)"
        printf 'memory_kib=%s\n' "$(awk '/MemTotal/{print $2}' /proc/meminfo)"
        if command -v rustc >/dev/null 2>&1; then
            printf 'rustc=%s\n' "$(rustc --version)"
            printf 'cargo=%s\n' "$(cargo --version)"
        fi
        printf 'gcc=%s\n' "$(gcc --version | head -1)"
    } >"${output}"
}
