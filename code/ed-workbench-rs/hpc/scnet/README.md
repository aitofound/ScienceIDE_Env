# SCNet HPC benchmark

These scripts reproduce the v0.4.0 workbench on the authorized SCNet
allocation and run an 18-case H2O/6-31G frozen-core Davidson robustness matrix.
Each case uses one 56-core node, 56 Rayon workers, and 56 fixed source blocks.
At full array concurrency the experiment uses 1,008 CPU cores.

This is task-parallel ensemble throughput.  The v0.4.0 solver is
shared-memory-only, so the experiment is not a claim that one Davidson solve
uses 1,008 cores.

## Fixed inputs

- source commit: `48f1964a1b3b88090497e1ffce285fde09c98541`
- Rust: `1.89.0`
- account: `giggleliu`
- partition: `xhacnormalb`
- remote root:
  `/work/share/giggleliu/cfys01/quantum-harness-129`

The remote source checkout must be detached at the fixed commit and clean.
Orchestration scripts are staged outside that checkout.

## Stage an offline toolchain

SCNet compute nodes do not provide external DNS.  Download the official
`rust-1.89.0-x86_64-unknown-linux-gnu.tar.xz` archive on a connected machine,
verify it against the accompanying `.sha256` file, and install it under:

```bash
/work/share/giggleliu/cfys01/quantum-harness-129/toolchains/rust-1.89.0
```

On the connected machine, generate the exact dependency tree:

```bash
cargo vendor --locked --versioned-dirs /temporary/vendor
```

Upload it to
`/work/share/giggleliu/cfys01/quantum-harness-129/toolchains/vendor` and place
the generated source-replacement configuration in
`toolchains/cargo/config.toml`.  The scheduled job sets
`CARGO_NET_OFFLINE=true`; a missing toolchain or crate fails the smoke gate
instead of attempting network access from a compute node.

The `libcint-src` crate normally clones libcint during its CMake build.  Export
the tracked tree from exact upstream tag `v6.1.2`
(`8d13863ff481cea27efea5e56c9e4d352cdb8f80`) into a one-commit, non-shallow
offline Git repository tagged `v6.1.2`.  Its Git tree must remain
`3de5cd4cf6b7f3fe04d53dfeed3dc85f69eb1133`.  Archive it as
`libcint-v6.1.2-offline.tar.gz` and upload it to the toolchain directory.  The
recorded archive SHA-256 is:

```text
9e5a4b9aea855317f48e7915b5ecd49cb2bbd96dee33cc073a36f65dafe2e16a
```

The build job loads CMake 3.25, expands that archive locally, verifies the Git
commit, and sets `CINT_SRC` to the local repository.

The scheduled build exports the pinned source with `git archive`, copies the
single compressed `vendor.tar.gz` file, expands it on node-local storage, and
compiles below `$SLURM_TMPDIR`.  This avoids recursive shared-filesystem reads
of the vendor tree and avoids placing Cargo's metadata-heavy `target` tree on
the shared filesystem.  Only the final release binary and evidence are copied
back to:

```text
/work/share/giggleliu/cfys01/quantum-harness-129/artifacts/v0.4.0/
```

## Stage source and scripts

From an authenticated machine, create the remote root and clone the fixed
source:

```bash
ssh SCNET 'mkdir -p /work/share/giggleliu/cfys01/quantum-harness-129 &&
  if test ! -d /work/share/giggleliu/cfys01/quantum-harness-129/source-v0.4.0/.git; then
    git clone https://github.com/JunkaiWang-TheoPhy/quantum-harness-129-workbench-rust.git \
      /work/share/giggleliu/cfys01/quantum-harness-129/source-v0.4.0
  fi &&
  cd /work/share/giggleliu/cfys01/quantum-harness-129/source-v0.4.0 &&
  git checkout --detach 48f1964a1b3b88090497e1ffce285fde09c98541'

scp -r hpc/scnet SCNET:/work/share/giggleliu/cfys01/quantum-harness-129/orchestration-v1
```

`SCNET` denotes the user's configured SSH command or host alias.  Private-key
paths are intentionally not stored in this repository.

Copy `hpc/scnet/cargo-config.toml` to
`/work/share/giggleliu/cfys01/quantum-harness-129/toolchains/cargo/config.toml`
after staging the vendor tree.

## Submit the gated run

Submit build/smoke first:

```bash
ssh SCNET 'cd /work/share/giggleliu/cfys01/quantum-harness-129/orchestration-v1 &&
  sbatch --parsable \
    --export=ALL,QH129_ORCHESTRATION=$PWD \
    build-smoke.sbatch'
```

Require a zero exit status and inspect:

```text
runs/<job-id>/build-smoke/
```

Only after that gate passes, submit production:

```bash
ssh SCNET 'cd /work/share/giggleliu/cfys01/quantum-harness-129/orchestration-v1 &&
  sbatch --parsable \
    --export=ALL,QH129_ORCHESTRATION=$PWD \
    davidson-robustness.sbatch'
```

The array header is `0-17%18`; each task requests 56 CPUs.  Slurm can therefore
allocate at most `18 × 56 = 1,008` CPUs to this array.

If the individual solves finish before Slurm can launch all 18 array elements,
run the replicate experiment:

```bash
ssh SCNET 'cd /work/share/giggleliu/cfys01/quantum-harness-129/orchestration-v1 &&
  sbatch --parsable \
    --export=ALL,QH129_ORCHESTRATION=$PWD \
    davidson-replicates.sbatch'
```

It executes the same 18-case matrix, but repeats every case 12 times.  The 216
solves provide cross-node timing distributions and test deterministic numerical
repeatability while giving the scheduler enough time to establish full array
concurrency.  No artificial delay is used.  Each task writes a `summary.tsv`
plus the stdout, stderr, GNU time report, and hashes for every replicate.

For a gang-scheduled, utilization-oriented thousand-core run, submit:

```bash
ssh SCNET 'cd /work/share/giggleliu/cfys01/quantum-harness-129/orchestration-v1 &&
  sbatch --parsable \
    --export=ALL,QH129_ORCHESTRATION=$PWD \
    davidson-gang-1008.sbatch'
```

This job requests 18 nodes at once and starts 72 independent processes:
4 processes per node, 14 Rayon threads per process, for `72 × 14 = 1,008`
concurrent CPUs.  Process `p` evaluates case `floor(p / 4)` and one of four
three-replicate groups, yielding the same 18 cases and 216 total samples.  The
gang allocation cannot run as a partial array, and packing four moderately
threaded solvers per node avoids treating idle capacity inside one 56-CPU
small-system solve as useful parallel work.  If the association lacks 1,008
free CPUs, Slurm safely leaves the job pending with `AssocGrpCpuLimit`.

## Failure and resubmission

Every task writes an isolated directory below:

```text
runs/<array-job-id>/davidson-array/<array-index>/
```

Inspect `exit-status.txt`, `davidson.stderr`, and Slurm terminal state.
Resubmit only failed indices, for example:

```bash
sbatch --array=7,11 davidson-robustness.sbatch
```

Remote results are retained.  Download them without deleting or moving the
remote copy.
