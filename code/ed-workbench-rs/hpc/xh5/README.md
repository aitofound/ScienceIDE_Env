# XH5 Slurm configuration

These scripts target the SCNet North China Region 1 Xiongheng
`xhacnormalb` partition:

- one node;
- 128 CPU cores;
- approximately 513.5 GB RAM;
- Slurm scheduling.

The input is the all-electron H2O/cc-pVDZ Hamiltonian in the C2v ground-state
A1 sector. Its exact determinant dimension is 451,681,246 and one dense
`f64` vector is 3.365 GiB.

Run the scripts in this order:

1. `smoke.slurm`: parser, binary, account, and scheduler validation.
2. `probe.slurm`: one Davidson iteration using 64 Rayon threads and four
   deterministic source blocks.
3. `production.slurm`: a 64-thread, 64-block production configuration sized
   to finish within an 11.5-hour backfill allocation. The 64 private
   accumulation vectors use about 215.4 GiB.

The account currently exposes only 25 GB of `/work` storage. Consequently the
scripts use in-memory Davidson with `max-subspace=6`; they do not use the
disk-backed checkpoint workspace.

## Accepted production result

Slurm job `23008083` completed with exit code `0:0` on 2026-07-30:

```text
energy: -76.243218589558566
residual norm: 6.602e-8
iterations: 21
converged: true
elapsed: 03:55:43
```

The job requested one 64-CPU task and 384 GiB with `--exclusive`. Slurm
therefore accounted the whole 128-CPU node, while the Rust process used 64
Rayon workers. Final step `MaxRSS` was 233,052,988 KiB, or 222.257 GiB.

The exact input, output, stderr, machine-readable acceptance record,
independent PySCF cross-check, and scientific interpretation are committed
under `fixtures/h2o-ccpvdz-ae` and
`reports/h2o-ccpvdz-c2v-fci.md`.
